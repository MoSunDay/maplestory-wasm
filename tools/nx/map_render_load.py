#!/usr/bin/env python3
"""Quantify per-map texture/atlas pressure for the WASM client.

Reads assets/Map.nx directly and reports, per map id:
  * layer 0..7 obj instances and unique obj sprites (all animation frames),
  * tile instances and unique tile pixels,
  * background entries split by animated / tiled / moving, with the pixel
    cost of every animation frame (the client queues all frames of an
    on-screen animation via Animation::prepare_visible).

Atlas capacity is 8192*8192 px with fonts sharing the texture; the client
forces a full atlas wipe (clearinternal) once a row no longer fits, so any
map whose unique visible pixel set exceeds capacity re-uploads forever.

Pure functions only; stdlib plus the repo's own NX helpers.
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from map079_tree import T_BITMAP, child_indices, find_path, node_name  # noqa: E402
from merge_skill_nx import parse_nx  # noqa: E402

ATLAS_PX = 8192 * 8192
MB = 1024 * 1024


def child_map(nx, idx):
    """Name -> child node index for one node."""
    return {node_name(nx, c): c for c in child_indices(nx, idx)}


def int_value(nx, idx, default=0):
    node = nx['nodes'][idx]
    if node[3] == 1:
        return struct.unpack_from('<q', node[4], 0)[0]
    return default


def str_value(nx, idx, default=''):
    node = nx['nodes'][idx]
    if node[3] == 3:
        sid = struct.unpack_from('<I', node[4], 0)[0]
        return nx['strings'][sid].decode('utf-8', 'replace')
    return default


def bool_value(nx, idx, default=False):
    return int_value(nx, idx, 1 if default else 0) != 0


def bitmap_size(nx, idx):
    """(w, h) for a bitmap node, else None."""
    node = nx['nodes'][idx]
    if node[3] != T_BITMAP:
        return None
    _, w, h = struct.unpack_from('<IHH', node[4], 0)
    return (w, h)


def node_path(nx, parts):
    idx = find_path(nx, [p.encode() for p in parts])
    return idx


def sprite_frames(nx, idx):
    """[(w, h)] for a sprite node: either a bare bitmap or numeric children."""
    if idx is None:
        return []
    frames = []
    for c in child_indices(nx, idx):
        size = bitmap_size(nx, c)
        if size:
            frames.append(size)
    if not frames:
        size = bitmap_size(nx, idx)
        if size:
            frames.append(size)
    return frames


def sprite_cost(nx, idx):
    """(frames, pixels) of every frame of the sprite animation."""
    frames = sprite_frames(nx, idx)
    return len(frames), sum(w * h for w, h in frames)


def collect_obj_entry(nx, entry, obj_root, costs):
    """Register one obj instance; returns (sprite_key, animated)."""
    props = child_map(nx, entry)
    oS = str_value(nx, props.get(b'oS', entry), '')
    l0 = str_value(nx, props.get(b'l0', entry), '')
    l1 = str_value(nx, props.get(b'l1', entry), '')
    l2 = str_value(nx, props.get(b'l2', entry), '')
    key = (oS, l0, l1, l2)
    if key not in costs:
        idx = node_path(nx, ['Obj', oS + '.img', l0, l1, l2])
        costs[key] = sprite_cost(nx, idx)
    return key


def collect_back_entry(nx, entry, back_root, costs):
    """Register one background entry; returns its descriptor dict."""
    props = child_map(nx, entry)
    def val(name, default=0):
        return int_value(nx, props.get(name.encode(), entry), default)
    def sval(name, default=''):
        return str_value(nx, props.get(name.encode(), entry), default)
    def bval(name, default=False):
        return bool_value(nx, props.get(name.encode(), entry), default)
    bS, no = sval('bS'), val('no', -1)
    animated = bval('ani')
    key = (bS, animated, no)
    if bS and no >= 0 and key not in costs:
        idx = node_path(nx, ['Back', bS + '.img', 'ani' if animated else 'back', str(no)])
        costs[key] = sprite_cost(nx, idx)
    return {
        'bS': bS, 'no': no, 'ani': animated, 'type': val('type'),
        'front': bval('front'), 'cx': val('cx'), 'cy': val('cy'),
        'rx': val('rx'), 'ry': val('ry'),
    }


def analyze_map(nx, mapid):
    top = 'Map' + str(mapid // 100000000)
    root = node_path(nx, ['Map', top, str(mapid) + '.img'])
    if root is None:
        return None
    root_children = child_map(nx, root)
    info_idx = root_children.get(b'info')
    info = child_map(nx, info_idx) if info_idx is not None else {}

    obj_costs, back_costs, tile_costs = {}, {}, {}
    obj_instances, tile_instances = [], []
    for layer in range(8):
        layer_idx = root_children.get(str(layer).encode())
        if layer_idx is None:
            continue
        layer_props = child_map(nx, layer_idx)
        obj_root = layer_props.get(b'obj')
        if obj_root is not None:
            for entry in child_indices(nx, obj_root):
                obj_instances.append(collect_obj_entry(nx, entry, obj_root, obj_costs))
        tile_root = layer_props.get(b'tile')
        if tile_root is not None:
            ts_idx = child_map(nx, info_idx).get(b'tS') if info_idx is not None else None
            ts = str_value(nx, ts_idx, '') if ts_idx is not None else ''
            for entry in child_indices(nx, tile_root):
                t_props = child_map(nx, entry)
                t_key = (ts, str_value(nx, t_props.get(b'u', entry), ''))
                tile_instances.append(t_key)
                if t_key not in tile_costs and ts:
                    idx = node_path(nx, ['Tile', ts + '.img', t_key[1]])
                    tile_costs[t_key] = sprite_cost(nx, idx)

    backs = []
    back_root = root_children.get(b'back')
    if back_root is not None:
        no = 0
        while True:
            entry = child_map(nx, back_root).get(str(no).encode())
            if entry is None:
                break
            desc = collect_back_entry(nx, entry, back_root, back_costs)
            if desc['bS']:
                backs.append(desc)
            no += 1

    def sprite_px(costs):
        return sum(px for _, px in costs.values())

    def animated_px(costs):
        return sum(px for frames, px in costs.values() if frames > 1)

    def info_int(name):
        idx = info.get(name.encode())
        return int_value(nx, idx) if idx is not None else None

    bounds = {
        'w': [info_int('VRLeft'), info_int('VRRight')],
        'h': [info_int('VRTop'), info_int('VRBottom')],
    }
    width = (bounds['w'][1] or 0) - (bounds['w'][0] or 0)
    height = (bounds['h'][1] or 0) - (bounds['h'][0] or 0)
    info_out = {
        'width': width,
        'height': height,
        'cloud': bool_value(nx, info[b'cloud']) if b'cloud' in info else False,
    }
    return {
        'mapid': mapid,
        'objs': len(obj_instances),
        'unique_obj_sprites': len(obj_costs),
        'unique_obj_px': sprite_px(obj_costs),
        'animated_obj_px': animated_px(obj_costs),
        'animated_obj_sprites': sum(1 for f, _ in obj_costs.values() if f > 1),
        'tiles': len(tile_instances),
        'unique_tile_px': sprite_px(tile_costs),
        'backs': len(backs),
        'ani_backs': sum(1 for b in backs if b['ani']),
        'moving_backs': sum(1 for b in backs if b['type'] >= 4),
        'tiled_backs': sum(1 for b in backs if b['type'] in (2, 3, 4, 6, 7)),
        'unique_back_px': sprite_px(back_costs),
        'animated_back_px': animated_px(back_costs),
        **info_out,
    }


def fmt_row(r):
    total_px = r['unique_obj_px'] + r['unique_tile_px'] + r['unique_back_px']
    return (
        '%(mapid)9d | objs %(objs)4d/%(unique_obj_sprites)4d ani%(animated_obj_sprites)3d | '
        'tiles %(tiles)4d | backs %(backs)3d ani%(ani_backs)3d tiled%(tiled_backs)3d | '
        'uniq px %(mpx)6.1fM (%(pct)4.0f%% atlas) | ani px %(apx)6.1fM | cloud %(cloud)d'
    ) % {
        **r,
        'mpx': total_px / MB,
        'pct': 100.0 * total_px / ATLAS_PX,
        'apx': (r['animated_obj_px'] + r['animated_back_px']) / MB,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--assets', default='assets/Map.nx')
    ap.add_argument('ids', nargs='+', type=int)
    args = ap.parse_args()
    nx = parse_nx(args.assets)
    print('atlas capacity: %.1f Mpx (8192x8192, fonts share it; >~80%% triggers wipes)' % (ATLAS_PX / MB))
    print('     mapid | objs inst/uniq ani... | tiles | backs ... | unique pixel load | animated px | cloud')
    for mapid in args.ids:
        r = analyze_map(nx, mapid)
        print(fmt_row(r) if r else ('%9d | not found' % mapid))


if __name__ == '__main__':
    main()
