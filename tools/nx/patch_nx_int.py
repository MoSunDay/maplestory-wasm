#!/usr/bin/env python3
"""In-place integer patcher for NX files built by this repo's toolchain.

Walks the node tree by path and rewrites one integer node's 8-byte union
(int64 LE, high dword zero) without touching any other structure. The
assets-server detects size/mtime changes and reloads automatically.

Usage:
    patch_nx_int.py <file.nx> <path/to/node> <new-value> [--expect N]

Example:
    patch_nx_int.py assets/Skill.nx 000.img/skill/0001002/level/1/cooltime 3600 --expect 60
"""
import argparse
import struct
import sys

MAGIC = 0x34474B50  # PKG4
HEADER = struct.Struct('<IIQIQIQIQ')  # 52 bytes
NODE = struct.Struct('<IIHH8s')       # 20 bytes
TYPE_INTEGER = 1


def parse_header(data):
    (magic, node_count, node_offset, _string_count, string_offset,
     _bitmap_count, _bitmap_offset, _audio_count, _audio_offset) = HEADER.unpack_from(data, 0)
    if magic != MAGIC:
        raise ValueError('not an NX file (magic %#x)' % magic)
    return node_count, node_offset, string_offset


def read_node(data, node_offset, index):
    base = node_offset + NODE.size * index
    name, children, num, typ, union = NODE.unpack_from(data, base)
    return base, name, children, num, typ, union


def node_name(data, string_offset, name_index):
    string_pos = string_offset + 8 * name_index
    (name_off,) = struct.unpack_from('<Q', data, string_pos)
    (length,) = struct.unpack_from('<H', data, name_off)
    return data[name_off + 2:name_off + 2 + length].decode('utf-8')


def find_child(data, node_offset, string_offset, children, num, wanted):
    for i in range(children, children + num):
        _, name, _, _, _, _ = read_node(data, node_offset, i)
        if node_name(data, string_offset, name) == wanted:
            return i
    return None


def resolve_path(data, node_offset, string_offset, path):
    index = 0
    for part in path:
        _, _, children, num, _, _ = read_node(data, node_offset, index)
        child = find_child(data, node_offset, string_offset, children, num, part)
        if child is None:
            raise KeyError('node not found: ' + '/'.join(path))
        index = child
    return index


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('nxfile')
    parser.add_argument('path')
    parser.add_argument('value', type=int)
    parser.add_argument('--expect', type=int, default=None,
                        help='fail if the current value differs')
    args = parser.parse_args(argv)

    with open(args.nxfile, 'r+b') as handle:
        data = handle.read()
        node_count, node_offset, string_offset = parse_header(data)
        index = resolve_path(data, node_offset, string_offset, args.path.split('/'))
        base, _name, _children, _num, typ, union = read_node(data, node_offset, index)
        if typ != TYPE_INTEGER:
            raise ValueError('node is not an integer (type %d)' % typ)
        (current,) = struct.unpack_from('<q', union, 0)
        if args.expect is not None and current != args.expect:
            raise ValueError('current value %d != expected %d' % (current, args.expect))
        handle.seek(base + 12)
        handle.write(struct.pack('<q', args.value))
        print('patched %s: %s %d -> %d' % (args.nxfile, args.path, current, args.value))
    return 0


if __name__ == '__main__':
    sys.exit(main())
