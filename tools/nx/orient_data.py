#!/usr/bin/env python3
"""Static id lists and 079MAX4 input loaders for the orient NX injection.

Feeds ``inject_orient_maps_nx.py``: the 东方神州 (701/702 Shanghai/Shaolin)
and 泰国 (500xxx Thailand) map ids, the missing Tile/Back library names, the
NPC/Mob img ids parsed with the strict->tolerant fallback chain, and the
map/npc/mob name entries (map079 ``Map.json`` + server ``String.wz`` XMLs).

Pure functions only; stdlib plus the repo's own 079 helpers.
"""
from __future__ import annotations

import copy
import struct
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
for _p in (REPO / 'tools', REPO / 'tools/nx'):
    sys.path.insert(0, str(_p))

from extract_079_data import decode_pixels, parse_img as parse_img_strict  # noqa: E402
from img079_parser import parse_img as parse_img_tolerant  # noqa: E402
from inject_npc_nx import (T_BITMAP, T_NONE, is_state_list, prop_desc,  # noqa: E402
                           sort_kids)
from map079_tree import best_tiny_block, load_tree, window_ok  # noqa: E402
from merge_skill_nx import intern  # noqa: E402

REF = REPO / 'link_repos/.ref-079max4'
CLIENT_DATA = (REF / '079MAX4-2023年度版/怀旧岛079MAX4客户端(登录器在bin内)/data')
SERVER_STRING_WZ = REF / '079MAX4-2023年度版/怀旧岛079MAX4服务端/wz/String.wz'
ORIENT_DIR = REF / 'extracted/orient'
MAP_NAME_JSON = REF / 'extracted/map079/Map.json'

# 28 Shanghai/Shaolin map ids (Map/Map7) + 9 Thailand ids (Map/Map5).
ORIENT_MAP_IDS = (701000000, 701000100, 701000200, 701010000, 701010100,
                  701010200, 701010300, 701010310, 701010320, 701010321,
                  701010322, 701010323, 701010324, 701010400, 701010500,
                  701010600, 702000000, 702010000, 702020000, 702030000,
                  702040000, 702050000, 702070100, 702070200, 702070300,
                  702070400, 702080000, 702100000)
THAI_MAP_IDS = (500000000, 500010000, 500010100, 500020000, 500020100,
                500020101, 500020200, 500020300, 500020400)
# Missing Map.nx libraries: (parent node, tree stem) -- Tile/Back children
# carry the .img suffix, and every other referenced oS/tS/bS set is present.
LIB_INSERTS = (('Tile', 'ShanghaiNJL'), ('Tile', 'thai'), ('Back', 'YuyuanCN'))

# NPC imgs to inject: every file in data/Npc except the two already shipped.
NPC_IMG_SKIP = (9030000, 9270052)
# Name entries wanted from String.wz/Npc.img.xml (32 exist; the 3 without an
# entry -- 1082100, 9000087, 9310045 -- are skipped silently and reported).
NPC_NAME_IDS = (1082100, 9000087, 9300015, 9310031, 9310032, 9310033, 9310034,
                9310035, 9310036, 9310037, 9310038, 9310039, 9310040, 9310041,
                9310042, 9310043, 9310045, 9310046, 9310047, 9310048, 9310049,
                9310050, 9310051, 9310052, 9310053, 9310055, 9310057, 9310060,
                9310082, 9310084, 9310085, 9310100, 9330042, 9330078, 9330090)
MOB_IDS = tuple(range(9600012, 9600025))
NAME_FIELDS = ('streetName', 'mapName')


def fail(msg):
    sys.exit('orient_data: ' + msg)


def npc_img_ids():
    """63 ids: every data/Npc/*.img except the shipped 9030000/9270052."""
    ids = sorted(int(p.stem) for p in CLIENT_DATA.joinpath('Npc').glob('*.img'))
    return tuple(i for i in ids if i not in NPC_IMG_SKIP)


def parse_img_fallback(img_path):
    """Strict Npc-style parse first, then the tolerant Map-family variant."""
    data = img_path.read_bytes()
    try:
        tree = parse_img_strict(data)
    except Exception:  # noqa: BLE001 - body-layout quirks (3 known imgs)
        tree = parse_img_tolerant(data)
    return deref_uols(tree, img_path.name)


def deref_uols(node, label):
    """Materialize uol children as copies of their sibling targets.

    The 079 NPC/Mob imgs alias repeated frames with WZ uol nodes (state-level
    only, sibling-name targets); the shipping NX archives resolve them away
    (NoLifeWzToNx copies the target node, leaving no type-7 uol nodes), and
    ``inject_npc_nx.prop_union`` rejects the kind -- so resolve them here.
    """
    kids = node.get('children')
    if not kids:
        return node
    fixed = [(name, deref_uols(child, label)) for name, child in kids]
    by_name = {}
    for name, child in fixed:
        by_name.setdefault(name, child)
    for _ in range(len(fixed) + 1):  # follow chained uols to a fixpoint
        if not any(c['kind'] == 'uol' for _, c in fixed):
            break
        fixed = [(name, by_name.get(c['target'], c) if c['kind'] == 'uol' else c)
                 for name, c in fixed]
    shared = {}
    for _, child in fixed:
        shared[id(child)] = shared.get(id(child), 0) + 1
    out = []
    for name, child in fixed:
        if child['kind'] == 'uol':
            fail('%s: unresolvable uol %s -> %r' % (label, name, child['target']))
        out.append((name, copy.deepcopy(child) if shared[id(child)] > 1 else child))
    return {**node, 'children': out}


def string_fields(node):
    return [(c['name'], c['value']) for c in node.get('children', [])
            if c['kind'] == 'str']


def map_name_entries():
    """{mapid: [(streetName, v), (mapName, v)]} for the 37 injected maps."""
    tree = load_tree(MAP_NAME_JSON)
    root = {c['name']: c for c in tree['children']}
    thai = root.get('thai')
    if thai is None:
        fail('Map.json: no thai section')
    entries = {}
    for ids, holder in ((ORIENT_MAP_IDS, root), (THAI_MAP_IDS,
                                                  {c['name']: c for c in thai['children']})):
        for mid in ids:
            node = holder.get(str(mid))
            if node is None:
                fail('Map.json: no name entry for %d' % mid)
            fields = {k: v for k, v in string_fields(node)}
            missing = [f for f in NAME_FIELDS if f not in fields]
            if missing:
                fail('Map.json/%d: missing %s' % (mid, missing))
            entries[mid] = [(f, fields[f]) for f in NAME_FIELDS]
    return entries


def xml_string_entries(xml_path, wanted):
    """({id: [(field, value)...]}, [missing ids]) from a String.wz img XML."""
    root = ET.parse(xml_path).getroot()
    by_name = {d.get('name'): d for d in root.findall('imgdir')}
    entries, skipped = {}, []
    for ident in wanted:
        node = by_name.get(str(ident))
        fields = ([(e.get('name'), e.get('value')) for e in node.findall('string')]
                  if node is not None else [])
        if fields:
            entries[ident] = fields
        else:
            skipped.append(ident)
    return entries, skipped


def npc_name_entries():
    """{npcid: [(field, value)...]}; ids without an entry are dropped."""
    entries, _ = xml_string_entries(SERVER_STRING_WZ / 'Npc.img.xml',
                                    NPC_NAME_IDS)
    return entries


def mob_name_entries():
    entries, skipped = xml_string_entries(
        SERVER_STRING_WZ / 'Mob.img.xml', MOB_IDS)
    if skipped:
        fail('Mob.img.xml: no name entry for %s' % skipped)
    return entries


# ---------------------------------------------- img tree -> NX descriptors
#
# ``inject_npc_nx.npc_subtree`` hard-fails on canvases whose LZ4 block
# overruns the strict read window (clen+16 <= 4*w*h), but the NoLifeNx
# bitmap reader allows the tiny-canvas slack the shipping v83 archive
# itself uses (``map079_tree.window_ok``): Npc/9310082.img has a 3x2
# canvas whose minimum block is 21 B in a 24 B window.  Rebuild the same
# descriptor chain here with the relaxed window check and a caller-chosen
# wrapper name (Npc wrappers are plain, Mob wrappers zero-padded).

def image_canvas_desc(name, canvas, strings, index, bitmaps):
    """Bitmap descriptor with inline img pixels (decoded RGBA + tiny slack)."""
    w, h = canvas['width'], canvas['height']
    block = best_tiny_block(decode_pixels(canvas))
    if not window_ok(len(block), w, h):
        fail('%s: LZ4 block %d B exceeds the %d B client read window'
             % (name, len(block), 4 * w * h))
    bitmaps.append(struct.pack('<I', len(block)) + block)
    union = struct.pack('<IHH', len(bitmaps) - 1, w, h)
    kids = sort_kids([prop_desc(pn, pc, strings, index)
                      for pn, pc in canvas.get('children', [])], strings)
    return (intern(strings, index, name.encode('utf-8')), T_BITMAP, union, kids)


def image_list_desc(name, child, strings, index, bitmaps):
    kids = sort_kids([image_desc_for(cn, grand, strings, index, bitmaps)
                      for cn, grand in child.get('children', [])], strings)
    return (intern(strings, index, name.encode('utf-8')), T_NONE, b'\0' * 8, kids)


def image_desc_for(name, child, strings, index, bitmaps):
    if child['kind'] == 'canvas':
        return image_canvas_desc(name, child, strings, index, bitmaps)
    if child['kind'] == 'list':
        return image_list_desc(name, child, strings, index, bitmaps)
    return prop_desc(name, child, strings, index)


def image_wrapper_desc(img_root, name_b, strings, index, bitmaps):
    """``npc_subtree`` equivalent: info + canvas states, name given as bytes."""
    info_children, states = [], []
    for name, child in img_root.get('children', []):
        if is_state_list(child):
            states.append(image_list_desc(name, child, strings, index, bitmaps))
        else:
            info_children.append((name, child))
    kids = sort_kids(
        ([image_list_desc('info', {'children': info_children},
                          strings, index, bitmaps)] if info_children else [])
        + states, strings)
    return (intern(strings, index, name_b), T_NONE, b'\0' * 8, kids)


def repair_head_insert(nx, parent_idx, at, base):
    """Fix a parent's base after a head-of-block sorted splice.

    ``inject_npc_nx.shifted`` bumps every child pointer >= the splice slot,
    so a wrapper sorted *before* the parent's current first child (at ==
    base) leaves the parent pointing one past the new head; reset it to
    the wrapper's slot.  ``at > base`` inserts need no repair (base < at
    is never shifted), and root-level splicers that keep ``root[1]`` as-is
    are already correct.
    """
    if at == base:
        name, _, num, typ, union = nx['nodes'][parent_idx]
        nx['nodes'][parent_idx] = (name, at, num, typ, union)


# The orient dump stores canvas pixels at their native bpp (all 88 canvases
# are ARGB4444, 2 B/px), while map079_tree.wrapper_desc requires pre-decoded
# 4*w*h RGBA8888 bins (the map079 dump layout).  Materialize the decoded
# copies into a scratch "pixels/" tree so wrapper_desc stays untouched.

def write_rgba(canvas, orient, out_dir):
    raw = (orient / canvas['pixels']).read_bytes()
    rgba = raw if len(raw) == 4 * canvas['width'] * canvas['height'] \
        else decode_pixels({**canvas, 'pixels': raw})
    (out_dir / Path(canvas['pixels']).name).write_bytes(rgba)


def walk_canvases(tree, orient, out_dir):
    for child in tree.get('children', []):
        if child['kind'] == 'canvas':
            write_rgba(child, orient, out_dir)
        else:
            walk_canvases(child, orient, out_dir)


def rgba_pixel_root(orient_dir):
    """(TemporaryDirectory, pixel-root Path) with decoded RGBA pixel bins."""
    orient = Path(orient_dir)
    holder = tempfile.TemporaryDirectory(prefix='orient-rgba-')
    out_dir = Path(holder.name) / 'pixels'
    out_dir.mkdir()
    for tree_json in sorted(orient.glob('*.json')):
        walk_canvases(load_tree(tree_json), orient, out_dir)
    return holder, Path(holder.name)
