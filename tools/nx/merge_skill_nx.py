#!/usr/bin/env python3
"""Merge NoLifeWzToNx per-image .nx outputs into one .nx archive.

Produces a single file whose root children are the per-image subtrees, named
"<stem>.img" (or an explicit child name when extracting from a larger archive).
The on-disk layout mirrors the vendored NoLifeWzToNx writer: 52-byte header,
20-byte nodes, u64-offset string/audio/bitmap tables, 0x10 section padding,
bitmap blobs prefixed with a u32 compressed length.

Input specs:
    path/to/000.nx            -> wrapper node "000.img"
    path/to/Data.nx::910.img  -> wrapper node "910.img" (subtree re-root)

Usage:
    merge_skill_nx.py -o Skill.nx spec [spec ...]
"""
import os
import struct
import sys

HEADER = struct.Struct('<IIQIQIQIQ')  # magic, node/str/bmp/audio counts + offsets
NODE = struct.Struct('<IIHH8s')       # name, children(first idx), num, type, union
MAGIC = 0x34474B50  # "PKG4"
T_STRING, T_BITMAP, T_AUDIO = 3, 5, 6


def fail(msg):
    sys.exit('merge_skill_nx: ' + msg)


def u16(buf, off):
    return struct.unpack_from('<H', buf, off)[0]


def u32(buf, off):
    return struct.unpack_from('<I', buf, off)[0]


def u64(buf, off):
    return struct.unpack_from('<Q', buf, off)[0]


def aligned(off):
    # Writer quirk: padding is always 1..16 bytes, never zero.
    return off + (0x10 - (off & 0xF))


def parse_nx(path):
    buf = open(path, 'rb').read()
    if len(buf) < HEADER.size or u32(buf, 0) != MAGIC:
        fail('%s: not an NX archive' % path)
    _, ncount, noff, scount, soff, bcount, boff, acount, aoff = \
        HEADER.unpack_from(buf, 0)
    strings = []
    for i in range(scount):
        doff = u64(buf, soff + 8 * i)
        slen = u16(buf, doff)
        strings.append(buf[doff + 2:doff + 2 + slen])
    bitmaps = []
    for i in range(bcount):
        doff = u64(buf, boff + 8 * i)
        clen = u32(buf, doff)
        bitmaps.append(buf[doff:doff + 4 + clen])
    nodes = []
    for i in range(ncount):
        name, children, num, typ = struct.unpack_from('<IIHH', buf, noff + 20 * i)
        union = buf[noff + 20 * i + 12:noff + 20 * i + 20]
        nodes.append((name, children, num, typ, union))
    audio_len = {}
    for name, children, num, typ, union in nodes:
        if typ == T_AUDIO:
            idx, length = struct.unpack('<II', union)
            prev = audio_len.get(idx)
            if prev is not None and prev != length:
                fail('%s: audio %d referenced with conflicting lengths'
                     % (path, idx))
            audio_len[idx] = length
    audios = []
    for i in range(acount):
        doff = u64(buf, aoff + 8 * i)
        nxt = u64(buf, aoff + 8 * (i + 1)) if i + 1 < acount else len(buf)
        length = audio_len.get(i, nxt - doff)
        audios.append(buf[doff:dff_clip(doff, length, buf)])
    return {'strings': strings, 'nodes': nodes,
            'bitmaps': bitmaps, 'audios': audios}


def dff_clip(doff, length, buf):
    return min(len(buf), doff + max(0, length))


def find_child(nx, name):
    root = nx['nodes'][0]
    for i in range(root[2]):
        idx = root[1] + i
        sid = nx['nodes'][idx][0]
        if nx['strings'][sid] == name:
            return idx
    return None


def intern(strings, index, s):
    if s not in index:
        index[s] = len(strings)
        strings.append(s)
    return index[s]


def remap_union(union, typ, str_map, bmp_map, aud_map):
    # Orphaned nodes may carry stale bitmap/audio indices after pruning;
    # they are unreachable, so clamp them to the first table entry.
    if typ == T_STRING:
        return struct.pack('<II', str_map[u32(union, 0)], 0)
    if typ == T_BITMAP:
        idx, w, h = struct.unpack('<IHH', union)
        mapped = bmp_map[idx] if idx < len(bmp_map) else 0
        return struct.pack('<IHH', mapped, w, h)
    if typ == T_AUDIO:
        idx, length = struct.unpack('<II', union)
        mapped = aud_map[idx] if idx < len(aud_map) else 0
        return struct.pack('<II', mapped, length)
    return union



def prune(nx, root_idx):
    """Restrict a large archive to the subtree at root_idx.

    The node table is a DAG (frames share property children), so every node
    is kept and only the bitmap/audio blobs of unreachable subtrees are
    dropped; the node overhead is negligible. The chosen child is swapped
    into slot 0 for merge(), which nothing else references.
    """
    src = nx['nodes']
    seen = set()
    stack = [root_idx]
    while stack:
        i = stack.pop()
        if i in seen:
            continue
        seen.add(i)
        _, children, num, _, _ = src[i]
        for k in range(num):
            c = children + k
            if c < len(src):
                stack.append(c)
    used_bmp, used_aud = set(), set()
    for i in seen:
        _, _, _, typ, union = src[i]
        if typ == T_BITMAP:
            used_bmp.add(struct.unpack_from('<I', union, 0)[0])
        elif typ == T_AUDIO:
            used_aud.add(struct.unpack_from('<I', union, 0)[0])
    bmp_map = {}
    bitmaps = []
    for i, blob in enumerate(nx['bitmaps']):
        if i in used_bmp:
            bmp_map[i] = len(bitmaps)
            bitmaps.append(blob)
    aud_map = {}
    audios = []
    for i, blob in enumerate(nx['audios']):
        if i in used_aud:
            aud_map[i] = len(audios)
            audios.append(blob)
    nodes = []
    for i, (nm, children, num, typ, union) in enumerate(src):
        patched = union
        if i in seen:
            if typ == T_BITMAP:
                idx, w, h = struct.unpack('<IHH', union)
                patched = struct.pack('<IHH', bmp_map[idx], w, h)
            elif typ == T_AUDIO:
                idx, length = struct.unpack('<II', union)
                patched = struct.pack('<II', aud_map[idx], length)
        nodes.append((nm, children, num, typ, patched))
    nodes[0], nodes[root_idx] = nodes[root_idx], nodes[0]
    return {'strings': nx['strings'], 'nodes': nodes,
            'bitmaps': bitmaps, 'audios': audios}


def merge(sources):
    """sources: list of (path, wrapper_name, nx). Returns merged nx dict."""
    strings, index = [b''], {b'': 0}
    bitmaps, audios = [], []
    # Layout: node 0 is the root, nodes 1..N are the contiguous wrappers,
    # then each wrapper's remapped subtree follows in order.
    node_count = len(sources)
    # Root's children are the contiguous wrapper slots 1..node_count.
    nodes = [(0, 1, node_count, 0, b'\0' * 8)] + [None] * node_count
    for pos, (path, name, nx) in enumerate(sources, start=1):
        src = nx['nodes']
        base = len(nodes)
        str_map = [intern(strings, index, st) for st in nx['strings']]
        bmp_map = []
        for blob in nx['bitmaps']:
            bmp_map.append(len(bitmaps))
            bitmaps.append(blob)
        aud_map = []
        for blob in nx['audios']:
            aud_map.append(len(audios))
            audios.append(blob)
        node_map = {0: pos}  # source root -> wrapper slot
        for i in range(1, len(src)):
            node_map[i] = base + i - 1

        def child_index(children, num):
            # The writer leaves `children` pointing at the next free slot
            # for empty nodes (num == 0); readers never dereference it.
            if num == 0:
                return 0
            if children >= len(src):
                fail('%s: corrupt children index %d' % (path, children))
            return node_map[children]

        for i in range(1, len(src)):
            nm, children, num, typ, union = src[i]
            nodes.append((
                str_map[nm], child_index(children, num), num, typ,
                remap_union(union, typ, str_map, bmp_map, aud_map)))
        root = src[0]
        nodes[pos] = (intern(strings, index, name),
                      child_index(root[1], root[2]),
                      root[2], 0, b'\0' * 8)
    return {'strings': strings, 'nodes': nodes,
            'bitmaps': bitmaps, 'audios': audios}


def string_data_len(strings):
    total = 0
    for s in strings:
        total += len(s) + 2 + (len(s) & 1)
    return total


def build(buf):
    out = bytearray()
    node_off = aligned(HEADER.size)
    str_table_off = aligned(node_off + 20 * len(buf['nodes']))
    str_data_off = aligned(str_table_off + 8 * len(buf['strings']))
    aud_table_off = aligned(str_data_off + string_data_len(buf['strings']))
    bmp_table_off = aligned(aud_table_off + 8 * len(buf['audios']))
    aud_data_off = aligned(bmp_table_off + 8 * len(buf['bitmaps']))
    bmp_data_off = aligned(aud_data_off + sum(map(len, buf['audios'])))
    total = bmp_data_off + sum(map(len, buf['bitmaps']))
    out.extend(b'\0' * total)
    HEADER.pack_into(out, 0, MAGIC, len(buf['nodes']), node_off,
                     len(buf['strings']), str_table_off,
                     len(buf['bitmaps']), bmp_table_off,
                     len(buf['audios']), aud_table_off)
    for i, (name, children, num, typ, union) in enumerate(buf['nodes']):
        NODE.pack_into(out, node_off + 20 * i, name, children, num, typ, union)
    cur = str_data_off
    for s in buf['strings']:
        struct.pack_into('<Q', out, str_table_off, cur)
        str_table_off += 8
        struct.pack_into('<H', out, cur, len(s))
        out[cur + 2:cur + 2 + len(s)] = s
        cur += len(s) + 2 + (len(s) & 1)
    cur = aud_data_off
    for blob in buf['audios']:
        struct.pack_into('<Q', out, aud_table_off, cur)
        aud_table_off += 8
        out[cur:cur + len(blob)] = blob
        cur += len(blob)
    cur = bmp_data_off
    for blob in buf['bitmaps']:
        struct.pack_into('<Q', out, bmp_table_off, cur)
        bmp_table_off += 8
        out[cur:cur + len(blob)] = blob
        cur += len(blob)
    return out


def parse_spec(spec):
    if '::' in spec:
        path, child = spec.rsplit('::', 1)
        return path, child.encode()
    path = spec.rstrip('/')
    stem = os.path.basename(path)
    if not stem.endswith('.nx'):
        fail('%s: expected .nx input' % spec)
    return path, stem[:-3].encode() + b'.img'


def main(argv):
    if len(argv) < 4 or argv[1] != '-o':
        fail('usage: merge_skill_nx.py -o OUTPUT.nx INPUT.nx[::CHILD] ...')
    output = argv[2]
    sources = []
    for spec in argv[3:]:
        path, wrapper = parse_spec(spec)
        nx = parse_nx(path)
        if '::' in spec:
            root = find_child(nx, wrapper)
            if root is None:
                fail('%s: no child %r' % (path, wrapper))
            if root != 0:
                nx = prune(nx, root)
        sources.append((path, wrapper, nx))
    sources.sort(key=lambda s: s[1])
    merged = merge(sources)
    blob = build(merged)
    with open(output, 'wb') as fh:
        fh.write(blob)
    print('wrote %s: %d nodes, %d strings, %d bitmaps, %d audios, %.1f MiB'
          % (output, len(merged['nodes']), len(merged['strings']),
             len(merged['bitmaps']), len(merged['audios']), len(blob) / 1048576.0))


if __name__ == '__main__':
    main(sys.argv)
