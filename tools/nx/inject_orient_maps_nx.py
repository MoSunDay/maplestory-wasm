#!/usr/bin/env python3
"""Inject the 079MAX4 orient (701/702 Shanghai/Shaolin) and Thailand (500)
assets into the client NX archives.

* ``Map.nx``: 28 ``Map/Map7`` maps, 9 ``Map/Map5`` maps, ``Tile/ShanghaiNJL``,
  ``Tile/thai`` and ``Back/YuyuanCN`` (sorted inserts; the 079 trees live in
  ``link_repos/.ref-079max4/extracted/orient`` with native-bpp pixel bins).
* ``Npc.nx``: the 63 staged 079 NPC imgs absent from the archive (9030000 and
  9270052 were injected earlier); uol aliases are materialized first and
  9310060/9330078 need the tolerant 079 parser (custom canvas payload).
* ``Mob.nx``: 9600012..9600024 (same payload quirks on 9600013).
* ``String.nx``: ``Map.img/elin`` 9 Thai map names, ``Map.img/event`` 28
  orient names, ``Npc.img`` the 32 names the 079 pack carries (1082100 /
  9000087 / 9310045 have no source entry) and ``Mob.img`` 13 names.

All canvases are converted to RGBA8888 and LZ4-compressed with the client
read-window constraint (tiny frames use the reference writer's slack).  One
shot: reruns are refused while the ``*.pre-orient-backup`` files exist.

Usage:
    inject_orient_maps_nx.py [--assets-dir assets] [--skip-sync]
"""
from __future__ import annotations

import argparse
import os
import shutil
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
for _p in (REPO / 'tools', REPO / 'tools/nx'):
    sys.path.insert(0, str(_p))

import orient_data as od  # noqa: E402
from inject_npc_nx import (  # noqa: E402
    T_NONE, T_STRING, flatten_children, splice, sorted_slot, string_index,
    with_tail_pad,
)
from inject_map_nx import insert_wrapper  # noqa: E402
from map079_tree import (  # noqa: E402
    child_indices, load_tree, node_name, require_path, wrapper_desc,
)
from merge_skill_nx import build, intern, parse_nx  # noqa: E402

MAP7_IDS = od.ORIENT_MAP_IDS
THAI_IDS = od.THAI_MAP_IDS
LIB_IMAGES = tuple((parent.encode(), stem)
                   for parent, stem in od.LIB_INSERTS)
MOB_IDS = od.MOB_IDS
BACKUP_SUFFIX = '.pre-orient-backup'
NX_FILES = ('Map.nx', 'Npc.nx', 'Mob.nx', 'String.nx')


def entry_exists(nx, parent_idx, name_b):
    return any(node_name(nx, c) == name_b
               for c in child_indices(nx, parent_idx))


def insert_string_node(nx, parent_idx, name_b, entries):
    """Sorted-insert an entry node with string children under parent_idx."""
    strings, index = nx['strings'], string_index(nx['strings'])
    old_base = nx['nodes'][parent_idx][1]
    base = len(nx['nodes']) + 1
    subtree = [(intern(strings, index, k.encode()), base + i, 0, T_STRING,
                struct.pack('<II', intern(strings, index, v.encode('utf-8')),
                            0))
               for i, (k, v) in enumerate(sorted(entries))]
    insert = (intern(strings, index, name_b), base, len(entries), T_NONE,
              b'\0' * 8)
    at = sorted_slot(nx, parent_idx, name_b)
    nodes = splice(nx['nodes'], at, insert, subtree)
    parent_after = parent_idx + (1 if at <= parent_idx else 0)
    nm, ch, num, typ, un = nodes[parent_after]
    nodes[parent_after] = (nm, at if at == old_base else ch, num + 1, typ, un)
    nx['nodes'] = nodes


def insert_sorted_wrapper(nx, parent_idx, wrapper):
    """Insert a wrapper descriptor and repair a head-of-block splice.

    ``shifted`` bumps the parent's first-child pointer when the new node
    sorts before the current head; reset it so the new wrapper is included.
    """
    old_base = nx['nodes'][parent_idx][1]
    at, added = insert_wrapper(nx, parent_idx, wrapper)
    if at == old_base:
        nm, _, num, typ, un = nx['nodes'][parent_idx]
        nx['nodes'][parent_idx] = (nm, at, num, typ, un)
    return at, added


def inject_map_archive(nx, orient_dir):
    index = string_index(nx['strings'])
    holder, pixel_root = od.rgba_pixel_root(orient_dir)
    stats = []
    try:
        for category, ids in (('Map7', MAP7_IDS), ('Map5', THAI_IDS)):
            parent = require_path(nx, [b'Map', category.encode()])
            for mapid in ids:
                tree = load_tree(Path(orient_dir) / ('%d.json' % mapid))
                wrapper = wrapper_desc(tree, '%d.img' % mapid, nx['strings'],
                                       index, nx['bitmaps'], pixel_root)
                at, added = insert_sorted_wrapper(nx, parent, wrapper)
                stats.append(('%s/%d' % (category, mapid), at, added))
        for parent_name, image in LIB_IMAGES:
            parent = require_path(nx, [parent_name])
            tree = load_tree(Path(orient_dir) / ('%s.json' % image))
            wrapper = wrapper_desc(tree, '%s.img' % image, nx['strings'],
                                   index, nx['bitmaps'], pixel_root)
            at, added = insert_sorted_wrapper(nx, parent, wrapper)
            stats.append(('%s/%s' % (parent_name.decode(), image), at, added))
    finally:
        holder.cleanup()
    return stats


def inject_image_archive(nx, img_dir, ids):
    strings, index = nx['strings'], string_index(nx['strings'])
    injected = []
    for image_id in ids:
        tree = od.parse_img_fallback(Path(img_dir) / ('%d.img' % image_id))
        wrapper = od.image_wrapper_desc(tree, ('%d.img' % image_id).encode(),
                                        strings, index, nx['bitmaps'])
        at, added = insert_sorted_wrapper(nx, 0, wrapper)
        injected.append((image_id, at, added))
    return injected


def inject_string_archive(nx, map_names, npc_names, mob_names, npc_ids):
    stats = {'elin': 0, 'event': 0, 'npc': 0, 'mob': 0, 'skipped': []}
    for category, ids in ((b'elin', THAI_IDS), (b'event', MAP7_IDS)):
        for mapid in ids:
            parent = require_path(nx, [b'Map.img', category])
            name_b = str(mapid).encode()
            if entry_exists(nx, parent, name_b):
                continue
            insert_string_node(nx, parent, name_b, map_names[mapid])
            stats[category.decode()] += 1
    for wrapper, ids, names, key in ((b'Npc.img', None, npc_names, 'npc'),
                                     (b'Mob.img', MOB_IDS, mob_names, 'mob')):
        for image_id in (ids if ids is not None else npc_ids):
            parent = require_path(nx, [wrapper])
            name_b = str(image_id).encode()
            if entry_exists(nx, parent, name_b):
                continue
            if image_id not in names:
                stats['skipped'].append((wrapper.decode(), image_id))
                continue
            insert_string_node(nx, parent, name_b, names[image_id])
            stats[key] += 1
    return stats


# ------------------------------------------------------------------ file I/O

def refuse_existing_backups(assets_dir):
    existing = [assets_dir / (name + BACKUP_SUFFIX) for name in NX_FILES]
    existing = [p for p in existing if p.exists()]
    if existing:
        sys.exit('refusing to rerun; backups exist: %s'
                 % ', '.join(str(p) for p in existing))


def make_backups(assets_dir):
    backups = []
    for name in NX_FILES:
        src = assets_dir / name
        if not src.exists():
            sys.exit('missing asset %s' % src)
        bak = assets_dir / (name + BACKUP_SUFFIX)
        shutil.copy2(src, bak)
        backups.append(bak)
    return backups


def write_archive(path, nx, tail_pad):
    old_size = path.stat().st_size
    blob = build(nx)
    if tail_pad:
        blob = with_tail_pad(blob, nx)
    tmp = path.with_suffix('.nx.tmp')
    tmp.write_bytes(blob)
    os.replace(tmp, path)
    return old_size, len(blob)


def counts(nx):
    return (len(nx['nodes']), len(nx['strings']), len(nx['bitmaps']))


def sync_copies(assets_dir):
    desktop = REPO / 'desktop/resources/msweb/assets'
    synced = []
    for name in NX_FILES:
        if desktop.is_dir():
            shutil.copy2(assets_dir / name, desktop / name)
            synced.append(desktop / name)
    nxfiles_string = REPO / 'nxFiles/String.nx'
    if nxfiles_string.parent.is_dir():
        shutil.copy2(assets_dir / 'String.nx', nxfiles_string)
        synced.append(nxfiles_string)
    return synced


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--assets-dir', type=Path, default=REPO / 'assets')
    ap.add_argument('--skip-sync', action='store_true')
    args = ap.parse_args(argv)
    assets_dir = args.assets_dir

    refuse_existing_backups(assets_dir)
    map_names = od.map_name_entries()
    npc_names = od.npc_name_entries()
    mob_names = od.mob_name_entries()
    npc_ids = list(od.npc_img_ids())
    backups = make_backups(assets_dir)

    plan = (('Map.nx', False), ('Npc.nx', True), ('Mob.nx', True),
            ('String.nx', False))
    for name, tail_pad in plan:
        path = assets_dir / name
        old_size = path.stat().st_size
        nx = parse_nx(path)
        before = counts(nx)
        if name == 'Map.nx':
            stats = inject_map_archive(nx, od.ORIENT_DIR)
            detail = '  %d map/library wrappers (sorted slots %d..%d)' % (
                len(stats), stats[0][1], stats[-1][1])
        elif name == 'Npc.nx':
            stats = inject_image_archive(nx, od.CLIENT_DATA / 'Npc', npc_ids)
            detail = '  %d NPC wrappers (%d nodes)' % (
                len(stats), sum(s[2] for s in stats))
        elif name == 'Mob.nx':
            stats = inject_image_archive(nx, od.CLIENT_DATA / 'Mob', MOB_IDS)
            detail = '  %d mob wrappers (%d nodes)' % (
                len(stats), sum(s[2] for s in stats))
        else:
            stats = inject_string_archive(nx, map_names, npc_names,
                                          mob_names, npc_ids)
            detail = ('  elin +%d, event +%d, Npc.img +%d, Mob.img +%d, '
                      'skipped %s' % (stats['elin'], stats['event'],
                                      stats['npc'], stats['mob'],
                                      stats['skipped'] or 'none'))
        old_size, new_size = write_archive(path, nx, tail_pad)
        after = counts(nx)
        print('%s: %d -> %d bytes (+%d)' % (path, old_size, new_size,
                                            new_size - old_size))
        print('  nodes %d -> %d, strings %d -> %d, bitmaps %d -> %d'
              % (before[0], after[0], before[1], after[1],
                 before[2], after[2]))
        print(detail)

    if not args.skip_sync:
        print('synced:', ', '.join(str(p) for p in sync_copies(assets_dir)))
    print('backups:', ', '.join(str(p) for p in backups))


if __name__ == '__main__':
    main()
