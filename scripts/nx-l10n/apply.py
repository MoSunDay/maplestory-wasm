#!/usr/bin/env python3
"""Apply Chinese translations to String.nx name entries.

Rewrites the NX string table in place: new string data and a new offset
table are appended to the file, then the header's string_offset field is
patched. Node table and node name indices stay untouched, so every node
keeps resolving to (possibly translated) strings.

Usage: python3 apply.py <String.nx> [--dry-run]
"""
import re
import struct
import sys

from zh_cash_consume import ZH_CASH_CONSUME
from zh_eqp_etc import ZH_EQP_ETC
from zh_ins_map_misc import ZH_INS_MAP_MISC

ZH_ALL = {**ZH_CASH_CONSUME, **ZH_EQP_ETC, **ZH_INS_MAP_MISC}

# Node value types that carry a string-table index.
NODE_TYPE_STRING = 3


def read_header(data):
    magic, node_count, node_offset, string_count, string_offset = struct.unpack_from('<IIQIQ', data, 0)
    if magic != 0x34474B50:
        raise SystemExit('not a PKG4 NX file')
    return node_count, node_offset, string_count, string_offset


def read_string_at(data, offset):
    length, = struct.unpack_from('<H', data, offset)
    return data[offset + 2:offset + 2 + length].decode('utf-8')


def read_all_strings(data, count, table_offset):
    return [read_string_at(data, struct.unpack_from('<Q', data, table_offset + i * 8)[0]) for i in range(count)]


def node_at(data, node_offset, index):
    name_idx, first_child, child_count, ntype = struct.unpack_from('<IIHH', data, node_offset + index * 20)
    value, = struct.unpack_from('<I', data, node_offset + index * 20 + 12)
    return name_idx, first_child, child_count, ntype, value


def iter_nodes(data, node_count, node_offset):
    for i in range(node_count):
        yield node_at(data, node_offset, i)


def find_target_indices(data, node_count, node_offset, strings):
    """Map string-table index -> node path for every name-like string node."""
    targets = {}
    for name_idx, _, _, ntype, value in iter_nodes(data, node_count, node_offset):
        if ntype != NODE_TYPE_STRING:
            continue
        if strings[name_idx] in ('name', 'mapName', 'streetName'):
            targets[value] = strings[name_idx]
    return targets


def translate(english):
    text = english.strip()
    if text in ('MISSING NAME', 'test'):
        return None
    if text in ('d', 'e', 'f', 'E1-A', 'E3-A', 'BOSS', 'J.J.'):
        return None
    return ZH_ALL.get(text)


def build_replacements(data, node_count, node_offset, strings, name_indices):
    """Return {string_index: translated_bytes} plus unmatched latin entries."""
    stack = [(0, '')]
    replacements = {}
    untranslated = []
    while stack:
        index, prefix = stack.pop()
        name_idx, first_child, child_count, ntype, value = node_at(data, node_offset, index)
        name = strings[name_idx]
        path = f'{prefix}/{name}'
        if ntype == NODE_TYPE_STRING and name in ('name', 'mapName', 'streetName'):
            english = strings[value]
            chinese = translate(english)
            if chinese is not None:
                replacements[value] = chinese.encode('utf-8')
            elif re.search(r'[A-Za-z]', english) and english.strip() != 'MISSING NAME':
                untranslated.append((path, english))
        for c in range(first_child, first_child + child_count):
            stack.append((c, path))
    return replacements, untranslated


def rebuild(data, string_count, old_table_offset, replacements):
    """Append new string data + offset table; return patched file bytes."""
    strings = read_all_strings(data, string_count, old_table_offset)
    # The offset table stores absolute file offsets; string data lands
    # right after the current file, then the new offset table after it.
    blob_start = len(data)
    blob = bytearray()
    offsets = []
    for i, text in enumerate(strings):
        raw = replacements.get(i, text.encode('utf-8'))
        offsets.append(blob_start + len(blob))
        blob += struct.pack('<H', len(raw)) + raw
    table_offset = blob_start + len(blob)
    new_table = bytearray()
    for off in offsets:
        new_table += struct.pack('<Q', off)
    out = bytearray(data)
    out += blob
    out += new_table
    struct.pack_into('<Q', out, 20, table_offset)
    return bytes(out)


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    path = sys.argv[1]
    dry = '--dry-run' in sys.argv
    data = open(path, 'rb').read()
    node_count, node_offset, string_count, string_offset = read_header(data)
    strings = read_all_strings(data, string_count, string_offset)
    name_indices = find_target_indices(data, node_count, node_offset, strings)
    replacements, untranslated = build_replacements(data, node_count, node_offset, strings, name_indices)
    print(f'{path}: {len(replacements)} translated, {len(untranslated)} latin names left as-is')
    for path_name, english in untranslated[:40]:
        print(f'  ?? {path_name} | {english!r}')
    if dry:
        return
    out = rebuild(data, string_count, string_offset, replacements)
    with open(path + '.bak', 'wb') as f:
        f.write(data)
    with open(path, 'wb') as f:
        f.write(out)
    print(f'wrote {path} ({len(out)} bytes), backup at {path}.bak')


if __name__ == '__main__':
    main()
