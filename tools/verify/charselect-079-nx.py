#!/usr/bin/env python3
"""Verify the 079MAX4 character-select injection in the client NX assets.

Read-only counterpart of ``tools/nx/inject_charselect_079_nx.py``.  It checks
that the three new ``Login.img`` children and ``Back/login079`` are bytewise
copies of the 079 source archives, that the surrounding parent node stays
sorted, and that the archive-wide node/bitmap counts grew by exactly the
number of copied descriptors.

The expected pre-injection node/bitmap counts are read from the
``*.pre-079charselect-backup`` files, so the same tool works for any client
asset pair produced by the injector.

Usage:
    charselect-079-nx.py [--assets-dir assets]
                         [--login-nx /tmp/nxconv/Login.nx]
                         [--back-nx /tmp/nxconv/login.nx]
                         [--only {ui,map,both}]
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'tools' / 'nx'))
from merge_skill_nx import (  # noqa: E402
    HEADER, T_AUDIO, T_BITMAP, T_STRING, parse_nx, u32,
)

UI_NODE_DELTA = 360
UI_BITMAP_DELTA = 91
MAP_NODE_DELTA = 633
MAP_BITMAP_DELTA = 160
NEW_UI_CHILDREN = (b'CharSelect079', b'frame079', b'selectWorld079')
MAGIC = 0x34474B50


def fail(message):
    sys.exit('charselect-079-nx: ' + message)


def header_counts(path):
    with path.open('rb') as handle:
        raw = handle.read(HEADER.size)
    if len(raw) != HEADER.size or u32(raw, 0) != MAGIC:
        fail('%s: not an NX archive' % path)
    _, nodes, _, strings, _, bitmaps, _, audios, _ = HEADER.unpack(raw)
    return {'nodes': nodes, 'strings': strings,
            'bitmaps': bitmaps, 'audios': audios}


def child_names(nx, idx):
    _, base, count, _, _ = nx['nodes'][idx]
    return [nx['strings'][nx['nodes'][base + i][0]] for i in range(count)]


def child(nx, idx, name):
    _, base, count, _, _ = nx['nodes'][idx]
    for i in range(count):
        candidate = base + i
        if nx['strings'][nx['nodes'][candidate][0]] == name:
            return candidate
    return None


def require(nx, idx, path):
    for part in path:
        idx = child(nx, idx, part)
        if idx is None:
            fail('missing node %r in path %r' % (part, path))
    return idx


def union_value(nx, idx):
    _, _, _, typ, union = nx['nodes'][idx]
    if typ == T_STRING:
        string_id = struct.unpack_from('<I', union, 0)[0]
        return nx['strings'][string_id]
    if typ == T_BITMAP:
        return struct.unpack('<IHH', union)
    if typ == T_AUDIO:
        return struct.unpack('<II', union)
    return union


def diff_kids(src, src_idx, dst, dst_idx, path):
    src_names = child_names(src, src_idx)
    dst_names = child_names(dst, dst_idx)
    if src_names != dst_names:
        fail('%r: children differ: %r != %r'
             % (path, src_names, dst_names))
    for name in src_names:
        diff_tree(src, child(src, src_idx, name),
                  dst, child(dst, dst_idx, name), path + (name,))


def diff_tree(src, src_idx, dst, dst_idx, path, root_name=None):
    src_name, _, _, src_type, _ = src['nodes'][src_idx]
    dst_name, _, _, dst_type, _ = dst['nodes'][dst_idx]
    want = root_name if root_name is not None else src['strings'][src_name]
    if want != dst['strings'][dst_name]:
        fail('%r: name %r != %r'
             % (path, want, dst['strings'][dst_name]))
    if src_type != dst_type:
        fail('%r: type %d != %d' % (path, src_type, dst_type))
    if src_type == T_BITMAP:
        src_blob = union_value(src, src_idx)
        dst_blob = union_value(dst, dst_idx)
        if src_blob[1:] != dst_blob[1:]:
            fail('%r: dimensions differ' % (path,))
        if src['bitmaps'][src_blob[0]] != dst['bitmaps'][dst_blob[0]]:
            fail('%r: bitmap blob differs' % (path,))
    elif src_type == T_STRING:
        if union_value(src, src_idx) != union_value(dst, dst_idx):
            fail('%r: string differs' % (path,))
    elif src_type == T_AUDIO:
        src_id, src_len = union_value(src, src_idx)
        dst_id, dst_len = union_value(dst, dst_idx)
        if src_len != dst_len or src['audios'][src_id] != dst['audios'][dst_id]:
            fail('%r: audio blob differs' % (path,))
    diff_kids(src, src_idx, dst, dst_idx, path)


def assert_sorted(nx, idx, label):
    names = child_names(nx, idx)
    if names != sorted(names):
        fail('%s children are not bytewise sorted' % label)


def check_counts(nx, backup, label, node_delta, bitmap_delta):
    if len(nx['nodes']) != backup['nodes'] + node_delta:
        fail('%s node count %d != %d + %d'
             % (label, len(nx['nodes']), backup['nodes'], node_delta))
    if len(nx['bitmaps']) != backup['bitmaps'] + bitmap_delta:
        fail('%s bitmap count %d != %d + %d'
             % (label, len(nx['bitmaps']), backup['bitmaps'], bitmap_delta))


def check_ui(ui_path, backup_path, login_nx):
    backup = header_counts(backup_path)
    src = parse_nx(str(login_nx))
    dst = parse_nx(str(ui_path))
    check_counts(dst, backup, 'UI.nx', UI_NODE_DELTA, UI_BITMAP_DELTA)

    login = require(dst, 0, [b'Login.img'])
    assert_sorted(dst, login, 'Login.img')
    names = child_names(dst, login)
    for name in NEW_UI_CHILDREN:
        if name not in names:
            fail('Login.img is missing %r' % name)

    for dst_name, src_path in (
        (b'frame079', [b'Common', b'frame']),
        (b'selectWorld079', [b'Common', b'selectWorld']),
    ):
        diff_tree(src, require(src, 0, src_path), dst,
                  child(dst, login, dst_name), (dst_name,), root_name=dst_name)
        print('OK UI Login.img/' + dst_name.decode())

    diff_tree(src, require(src, 0, [b'CharSelect']), dst,
              child(dst, login, b'CharSelect079'), (b'CharSelect079',),
              root_name=b'CharSelect079')
    print('OK UI Login.img/CharSelect079')


def check_map(map_path, backup_path, back_nx):
    backup = header_counts(backup_path)
    src = parse_nx(str(back_nx))
    dst = parse_nx(str(map_path))
    check_counts(dst, backup, 'Map.nx', MAP_NODE_DELTA, MAP_BITMAP_DELTA)

    back = require(dst, 0, [b'Back'])
    login079 = child(dst, back, b'login079')
    if login079 is None:
        fail('Map.nx Back/login079 is missing')
    assert_sorted(dst, back, 'Back')

    for name in (b'back', b'ani'):
        src_node = child(src, 0, name)
        dst_node = child(dst, login079, name)
        if src_node is None:
            if dst_node is not None:
                fail('unexpected Map.nx Back/login079/%r' % name)
            continue
        if dst_node is None:
            fail('Map.nx Back/login079 is missing %r' % name)
        diff_tree(src, src_node, dst, dst_node, (name,))
        print('OK Map Back/login079/' + name.decode())
    print('OK Map Back/login079')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--assets-dir', type=Path,
                        default=Path('assets'))
    parser.add_argument('--login-nx', type=Path,
                        default=Path('/tmp/nxconv/Login.nx'))
    parser.add_argument('--back-nx', type=Path,
                        default=Path('/tmp/nxconv/login.nx'))
    parser.add_argument('--only', choices=('ui', 'map', 'both'),
                        default='both')
    args = parser.parse_args(argv)

    assets = args.assets_dir
    suffix = '.pre-079charselect-backup'
    if args.only in ('ui', 'both'):
        check_ui(assets / 'UI.nx', assets / ('UI.nx' + suffix),
                 args.login_nx)
    if args.only in ('map', 'both'):
        check_map(assets / 'Map.nx', assets / ('Map.nx' + suffix),
                  args.back_nx)
    print('charselect-079-nx: all checks passed')


if __name__ == '__main__':
    main()
