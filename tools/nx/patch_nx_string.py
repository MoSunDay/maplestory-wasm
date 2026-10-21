#!/usr/bin/env python3
"""Repoint a string node in an NX file to another existing string entry.

NX string nodes store a u32 index into the global string table. This tool
never grows or rewrites the string table: the target text must already be
present in the file (it then reuses that entry). That makes the edit an
8-byte, size-preserving write, which the assets-server picks up by mtime.

Usage:
    patch_nx_string.py <file.nx> <node/path> <new-text> [--expect old-text]
"""
import argparse
import struct
import sys

MAGIC = 0x34474B50  # PKG4
HEADER = struct.Struct('<IIQIQIQIQ')
NODE = struct.Struct('<IIHH8s')
TYPE_STRING = 3


def parse_header(data):
    (magic, node_count, node_offset, _string_count, string_offset,
     _bitmap_count, _bitmap_offset, _audio_count, _audio_offset) = HEADER.unpack_from(data, 0)
    if magic != MAGIC:
        raise ValueError('not an NX file (magic %#x)' % magic)
    return node_count, node_offset, string_offset


def read_node(data, node_offset, index):
    return NODE.unpack_from(data, node_offset + NODE.size * index)


def string_at(data, string_offset, index):
    (offset,) = struct.unpack_from('<Q', data, string_offset + 8 * index)
    (length,) = struct.unpack_from('<H', data, offset)
    return data[offset + 2:offset + 2 + length].decode('utf-8')


def resolve_path(data, node_offset, string_offset, path):
    index = 0
    for part in path:
        _name, children, num, typ, union = read_node(data, node_offset, index)
        found = None
        for i in range(children, children + num):
            name, _c, _n, _t, _u = read_node(data, node_offset, i)
            if string_at(data, string_offset, name) == part:
                found = i
                break
        if found is None:
            raise KeyError('node not found: ' + '/'.join(path))
        index = found
    return index


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('nxfile')
    parser.add_argument('path')
    parser.add_argument('value')
    parser.add_argument('--expect', default=None)
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args(argv)

    with open(args.nxfile, 'rb') as handle:
        data = bytearray(handle.read())
    _node_count, node_offset, string_offset = parse_header(data)
    header_fields = HEADER.unpack_from(data, 0)
    string_count = header_fields[3]

    table = [string_at(data, string_offset, i) for i in range(string_count)]
    target = next((i for i, text in enumerate(table) if text == args.value), None)
    if target is None:
        raise SystemExit('target text not present in string table: %r' % args.value)

    index = resolve_path(data, node_offset, string_offset, args.path.split('/'))
    base = node_offset + NODE.size * index
    _name, _children, _num, typ, union = read_node(data, node_offset, index)
    if typ != TYPE_STRING:
        raise SystemExit('node is not a string (type %d)' % typ)
    (current,) = struct.unpack_from('<I', union, 0)
    if args.expect is not None and table[current] != args.expect:
        raise SystemExit('unexpected current value %r (wanted %r)' % (table[current], args.expect))

    print('%s: %r -> %r (index %d -> %d)' % (args.path, table[current], args.value, current, target))
    if args.dry_run or current == target:
        return 0
    offset = base + NODE.size - len(union)
    with open(args.nxfile, 'r+b') as handle:
        handle.seek(offset)
        handle.write(struct.pack('<Q', target))
    return 0


if __name__ == '__main__':
    sys.exit(main())
