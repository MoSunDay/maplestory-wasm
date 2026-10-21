#!/usr/bin/env python3
"""Inject the 079MAX4 annual-edition free market into client NX assets.

Splices the pre-extracted ``map079`` trees into ``assets/Map.nx``:

* ``Obj/chewchewIsland.img``   -- object library (1080 canvases), sorted insert;
* ``Back/chewchewIsland.img``  -- background library (185 canvases), sorted insert;
* ``Map/Map9/910000000.img``   -- replaced in place by the 079 map (info,
  miniMap 246x143, layers 0..8, ladderRope, 40 portals, reactor, 31 backs,
  271 footholds); the v83 subtree stays as unreachable orphan nodes, exactly
  like ``merge_skill_nx.prune`` treats replaced content.

``assets/String.nx`` gets the 079 names for ``Map.img/etc/910000000`` and
``910000001..910000022`` (streetName/mapName rewritten in place; other fields
such as mapDesc untouched, missing fields added).  910000023/024 have no 079
client names and are skipped.

nlnx resolves children with a bytewise binary search, so inserts keep the
sibling blocks sorted and new children blocks are emitted sorted.  Reader
constraints (fixed 4*w*h blob window, trailing padding) follow
``inject_npc_nx.py``.  One-shot: reruns are refused while the
``*.pre-079fm-style-backup`` backups exist.

Usage:
    inject_map_nx.py [--assets-dir assets] [--map079-dir DIR]
"""
from __future__ import annotations

import argparse
import os
import shutil
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from map079_tree import (  # noqa: E402
    MAP079_DIR, T_NONE, T_STRING, child_indices, find_path, load_tree,
    node_name, require_path, subtree_digest, subtree_nodes, wrapper_desc,
)
from inject_npc_nx import sorted_slot, splice, with_tail_pad  # noqa: E402
from merge_skill_nx import build, intern, parse_nx  # noqa: E402

FM_IDS = tuple(range(910000000, 910000023))
NAME_FIELDS = ('streetName', 'mapName')


def fail(msg):
    sys.exit('inject_map_nx: ' + msg)


def backup(path):
    """One-shot guard: refuse to rerun while the pre-injection backup exists."""
    bak = path.with_name('%s.pre-079fm-style-backup' % path.name)
    if bak.exists():
        fail('backup already exists, refusing to rerun: %s\n'
             '  (restore it over %s to start over)' % (bak, path.name))
    shutil.copy2(path, bak)
    return bak


# ------------------------------------------------------------- table surgery

def bump_num(nodes, idx, delta):
    nm, ch, num, typ, un = nodes[idx]
    nodes[idx] = (nm, ch, num + delta, typ, un)


def insert_wrapper(nx, parent_idx, wrapper):
    """Insert `wrapper` at its sorted slot among parent's children."""
    subtree, count = subtree_nodes(wrapper)
    at = sorted_slot(nx, parent_idx, nx['strings'][wrapper[0]])
    base = len(nx['nodes']) + 1
    insert = (wrapper[0], base, count, T_NONE, b'\0' * 8)
    nodes = splice(nx['nodes'], at, insert, subtree)
    bump_num(nodes, parent_idx, 1)
    nx['nodes'] = nodes
    return at, len(subtree) + 1


def replace_wrapper(nx, wrapper_idx, wrapper):
    """Re-point an existing wrapper node at a freshly appended subtree."""
    subtree, count = subtree_nodes(wrapper)
    base = len(nx['nodes'])
    placed = [(nm, ch + base, num, typ, un) for nm, ch, num, typ, un in subtree]
    nodes = nx['nodes'] + placed
    nm, _, _, typ, _ = nodes[wrapper_idx]
    if typ != T_NONE:
        fail('refusing to replace non-container wrapper node %d' % wrapper_idx)
    nodes[wrapper_idx] = (nm, base, count, T_NONE, b'\0' * 8)
    nx['nodes'] = nodes
    return len(subtree)


def set_string_field(sx, parent_idx, key, value):
    """Overwrite (or sorted-insert) a string child; True when inserted."""
    index = {s: i for i, s in enumerate(sx['strings'])}
    key_b = key.encode('utf-8')
    for c in child_indices(sx, parent_idx):
        if node_name(sx, c) == key_b:
            nm, ch, num, typ, un = sx['nodes'][c]
            sid = intern(sx['strings'], index, value.encode('utf-8'))
            sx['nodes'][c] = (nm, ch, num, T_STRING,
                              struct.pack('<II', sid, 0))
            return False
    subtree = [(intern(sx['strings'], index, key_b), 0, 0, T_STRING,
                struct.pack('<II', intern(sx['strings'], index,
                                          value.encode('utf-8')), 0))]
    at = sorted_slot(sx, parent_idx, key_b)
    nodes = splice(sx['nodes'], at, subtree[0], [])
    bump_num(nodes, parent_idx, 1)
    sx['nodes'] = nodes
    return True


def map079_names(map079_dir):
    """{mapid: (streetName, mapName)} from the 079 String/Map.img tree."""
    tree = load_tree(Path(map079_dir) / 'Map.json')
    etc = next((c for c in tree['children'] if c['name'] == 'etc'), None)
    if etc is None:
        fail('Map.json: no etc section')
    names = {}
    for entry in etc['children']:
        fields = {c['name']: c.get('value') for c in entry.get('children', [])
                  if c['kind'] == 'str'}
        if all(f in fields for f in NAME_FIELDS):
            names[int(entry['name'])] = tuple(fields[f] for f in NAME_FIELDS)
    missing = [i for i in FM_IDS if i not in names]
    if missing:
        fail('Map.json: no 079 names for %s' % missing)
    return names


# --------------------------------------------------------------------- main

def write_nx(path, blob):
    tmp = path.with_suffix('.nx.tmp')
    tmp.write_bytes(blob)
    os.replace(tmp, path)


def counts(nx):
    return len(nx['nodes']), len(nx['strings']), len(nx['bitmaps'])


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--assets-dir', type=Path, default=Path('assets'))
    ap.add_argument('--map079-dir', type=Path, default=MAP079_DIR)
    args = ap.parse_args(argv)
    map079 = Path(args.map079_dir)

    map_nx = args.assets_dir / 'Map.nx'
    string_nx = args.assets_dir / 'String.nx'
    names = map079_names(map079)
    baks = [backup(p) for p in (map_nx, string_nx)]

    old_size, old_counts = map_nx.stat().st_size, None
    nx = parse_nx(map_nx)
    old_counts = counts(nx)

    fm_idx = require_path(nx, [b'Map', b'Map9', b'910000000.img'])
    old_fm = subtree_digest(nx, fm_idx)[0]
    tree = load_tree(map079 / '910000000.json')
    added_fm = replace_wrapper(nx, fm_idx, wrapper_desc(
        tree, '910000000.img', nx['strings'],
        {s: i for i, s in enumerate(nx['strings'])}, nx['bitmaps'], map079))

    obj_idx = require_path(nx, [b'Obj'])
    if find_path(nx, [b'Obj', b'chewchewIsland.img']) is not None:
        fail('Map.nx already contains Obj/chewchewIsland.img')
    tree = load_tree(map079 / 'chewchewIsland.json')
    index = {s: i for i, s in enumerate(nx['strings'])}
    at_obj, added_obj = insert_wrapper(nx, obj_idx, wrapper_desc(
        tree, 'chewchewIsland.img', nx['strings'], index, nx['bitmaps'], map079))

    back_idx = require_path(nx, [b'Back'])
    if find_path(nx, [b'Back', b'chewchewIsland.img']) is not None:
        fail('Map.nx already contains Back/chewchewIsland.img')
    tree = load_tree(map079 / 'Back.chewchewIsland.json')
    index = {s: i for i, s in enumerate(nx['strings'])}
    at_back, added_back = insert_wrapper(nx, back_idx, wrapper_desc(
        tree, 'chewchewIsland.img', nx['strings'], index, nx['bitmaps'], map079))

    blob = with_tail_pad(build(nx), nx)
    write_nx(map_nx, blob)
    new_counts = counts(nx)
    print('%s: %d -> %d bytes (+%d)' % (map_nx, old_size, len(blob),
                                        len(blob) - old_size))
    print('  nodes %d -> %d (+%d), strings %d -> %d (+%d), bitmaps %d -> %d (+%d)'
          % (old_counts[0], new_counts[0], new_counts[0] - old_counts[0],
             old_counts[1], new_counts[1], new_counts[1] - old_counts[1],
             old_counts[2], new_counts[2], new_counts[2] - old_counts[2]))
    print('  Map9/910000000.img: %d orphaned v83 nodes, %d new nodes'
          % (old_fm, added_fm))
    print('  Obj insert at node slot %d (+%d nodes), Back insert at slot %d (+%d)'
          % (at_obj, added_obj, at_back, added_back))

    old_size = string_nx.stat().st_size
    sx = parse_nx(string_nx)
    old_counts = (len(sx['nodes']), len(sx['strings']))
    etc_idx = require_path(sx, [b'Map.img', b'etc'])
    inserted = 0
    for mapid in FM_IDS:
        idx = find_path(sx, [b'Map.img', b'etc', str(mapid).encode()])
        if idx is None:
            fail('String.nx: Map.img/etc has no %d' % mapid)
        for field, value in zip(NAME_FIELDS, names[mapid]):
            inserted += set_string_field(sx, idx, field, value)
    blob = build(sx)
    write_nx(string_nx, blob)
    print('%s: %d -> %d bytes (+%d)' % (string_nx, old_size, len(blob),
                                        len(blob) - old_size))
    print('  nodes %d -> %d (+%d), strings %d -> %d (+%d), %d field(s) added'
          % (old_counts[0], len(sx['nodes']), len(sx['nodes']) - old_counts[0],
             old_counts[1], len(sx['strings']), len(sx['strings']) - old_counts[1],
             inserted))
    print('backups: %s' % ', '.join(str(b) for b in baks))


if __name__ == '__main__':
    main()
