#!/usr/bin/env python3
"""Inject the 079MAX4 character-select art into the client NX assets.

``assets/UI.nx`` gets three new children under ``Login.img`` (the modern
world-select/login assets stay untouched):

* ``CharSelect079`` -- the complete 079 ``CharSelect`` subtree (charInfo,
  charInfo1, BtSelect/BtNew/BtDelete, page arrows, platforms, ...);
* ``frame079``      -- the 800x600 login window frame (origin 400,300);
* ``selectWorld079``-- the 150x57 world banner board.  Both are the 079
  canvas nodes renamed in place (same shape as ``Common/frame``).

``assets/Map.nx`` gets ``Back/login079`` with the 079 ``back`` (38 parallax
layers) and ``ani`` subtrees.  UICharSelect composes the layers at (400,300)
so the 800x600 design canvas matches the original client.

The node/bitmap/string tables are copied from the pre-converted NX archives
(``NoLifeWzToNx -c Login.img`` / ``-c login.img``).  Children stay bytewise
sorted because nlnx resolves names with a binary search.  One-shot: reruns
are refused while the ``*.pre-079charselect-backup`` files exist.

Usage:
    inject_charselect_079_nx.py [--assets-dir assets]
                                [--login-nx /tmp/nxconv/Login.nx]
                                [--back-nx /tmp/nxconv/login.nx]
"""
from __future__ import annotations

import argparse
import bisect
import os
import shutil
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from inject_npc_nx import (  # noqa: E402
    child_names, flatten_children, splice, with_tail_pad,
)
from map079_tree import (  # noqa: E402
    T_BITMAP, T_NONE, T_STRING, find_path, require_path,
)
from merge_skill_nx import T_AUDIO, build, intern, parse_nx  # noqa: E402


def fail(msg):
    sys.exit('inject_charselect_079: ' + msg)


def write_nx(path, blob):
    tmp = path.with_suffix('.nx.tmp')
    tmp.write_bytes(blob)
    os.replace(tmp, path)


def nx_desc(nx, idx, strings, index, bitmaps, audios):
    """Copy one source node (and its subtree) into target table descriptors."""
    name, base, num, typ, union = nx['nodes'][idx]
    name = intern(strings, index, nx['strings'][name])
    if typ == T_STRING:
        sid = struct.unpack_from('<I', union, 0)[0]
        union = struct.pack('<II',
                            intern(strings, index, nx['strings'][sid]), 0)
    elif typ == T_BITMAP:
        bid, w, h = struct.unpack('<IHH', union)
        union = struct.pack('<IHH', len(bitmaps), w, h)
        bitmaps.append(nx['bitmaps'][bid])
    elif typ == T_AUDIO:
        aid, length = struct.unpack('<II', union)
        union = struct.pack('<II', len(audios), length)
        audios.append(nx['audios'][aid])
    kids = [nx_desc(nx, c, strings, index, bitmaps, audios)
            for c in range(base, base + num)]
    kids.sort(key=lambda kid: strings[kid[0]])
    return (name, typ, union, tuple(kids))


def container(name, kids, strings, index):
    kids = tuple(sorted(kids, key=lambda kid: strings[kid[0]]))
    return (intern(strings, index, bytes(name)), T_NONE, b'\0' * 8, kids)


def rename(desc, name, strings, index):
    return (intern(strings, index, bytes(name)),) + tuple(desc[1:])


def insert_node(nx, parent_idx, desc):
    """Splice a node descriptor itself (plus subtree) into a parent."""
    subtree = []
    _, count = flatten_children(desc, subtree)
    at = nx['nodes'][parent_idx][1] + bisect.bisect_left(
        child_names(nx, parent_idx), nx['strings'][desc[0]])
    base = len(nx['nodes']) + 1
    insert = (desc[0], base, count, desc[1], desc[2])
    nodes = splice(nx['nodes'], at, insert, subtree)
    nm, ch, num, typ, un = nodes[parent_idx]
    nodes[parent_idx] = (nm, ch, num + 1, typ, un)
    nx['nodes'] = nodes
    return len(subtree) + 1


def backup(path):
    bak = path.with_name(path.name + '.pre-079charselect-backup')
    if bak.exists():
        fail('backup already exists, refusing to rerun: %s\n'
             '  (restore it over %s to start over)' % (bak, path.name))
    shutil.copy2(path, bak)
    return bak


def inject_ui(assets, login_nx):
    ui_path = assets / 'UI.nx'
    backup(ui_path)
    target = parse_nx(ui_path)
    source = parse_nx(login_nx)
    strings, index = target['strings'], {s: i for i, s in enumerate(target['strings'])}
    bitmaps, audios = target['bitmaps'], target['audios']
    login = require_path(target, [b'Login.img'])

    common = require_path(source, [b'Common'])
    for name, src_name in ((b'frame079', b'frame'),
                           (b'selectWorld079', b'selectWorld')):
        node = require_path(source, [b'Common', src_name])
        desc = nx_desc(source, node, strings, index, bitmaps, audios)
        insert_node(target, login, rename(desc, name, strings, index))

    src_cs = require_path(source, [b'CharSelect'])
    _, base, num, _, _ = source['nodes'][src_cs]
    kids = [nx_desc(source, c, strings, index, bitmaps, audios)
            for c in range(base, base + num)]
    insert_node(target, login,
                container(b'CharSelect079', kids, strings, index))

    before = ui_path.stat().st_size
    print('UI.nx: parsed, rebuilding (%d bytes)' % before, flush=True)
    blob = with_tail_pad(build(target), target)
    write_nx(ui_path, blob)
    print('UI.nx: %d -> %d bytes' % (before, len(blob)), flush=True)


def inject_map(assets, back_nx):
    map_path = assets / 'Map.nx'
    backup(map_path)
    target = parse_nx(map_path)
    source = parse_nx(back_nx)
    strings, index = target['strings'], {s: i for i, s in enumerate(target['strings'])}
    bitmaps, audios = target['bitmaps'], target['audios']
    back_parent = require_path(target, [b'Back'])

    back = require_path(source, [b'back'])
    ani = find_path(source, [b'ani'])
    kids = [nx_desc(source, back, strings, index, bitmaps, audios)]
    if ani is not None:
        kids.append(nx_desc(source, ani, strings, index, bitmaps, audios))
    insert_node(target, back_parent,
                container(b'login079', kids, strings, index))

    before = map_path.stat().st_size
    print('Map.nx: parsed, rebuilding (%d bytes)' % before, flush=True)
    blob = with_tail_pad(build(target), target)
    write_nx(map_path, blob)
    print('Map.nx: %d -> %d bytes' % (before, len(blob)), flush=True)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--assets-dir', type=Path, default=Path('assets'))
    ap.add_argument('--login-nx', type=Path,
                    default=Path('/tmp/nxconv/Login.nx'))
    ap.add_argument('--only', choices=('ui', 'map', 'both'), default='both',
                    help='inject one archive at a time (lower peak memory)')
    ap.add_argument('--back-nx', type=Path,
                    default=Path('/tmp/nxconv/login.nx'))
    args = ap.parse_args(argv)
    if not args.login_nx.is_file() or not args.back_nx.is_file():
        fail('missing converted 079 NX inputs (see --login-nx/--back-nx)')
    if args.only in ('ui', 'both'):
        inject_ui(args.assets_dir, args.login_nx)
    if args.only in ('map', 'both'):
        inject_map(args.assets_dir, args.back_nx)


if __name__ == '__main__':
    main()
