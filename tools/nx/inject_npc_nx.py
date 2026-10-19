#!/usr/bin/env python3
"""Inject an 079MAX4 NPC (sprites + name strings) into client NX assets.

Reads the custom ``data/Npc/<id>.img`` canvas tree via the
``tools/extract_079_data.py`` parser, converts each canvas to an RGBA8888 NX
bitmap (LZ4 block) and splices a ``<id>.img`` subtree into ``assets/Npc.nx``
beside the existing per-image wrappers.  The name/line/func entry from
``wz/String.wz/Npc.img.xml`` is spliced under the ``Npc.img`` wrapper of
``assets/String.nx``.  Node/string/bitmap tables are rebuilt with the
NoLifeWzToNx writer layout from ``tools/nx/merge_skill_nx.py`` (52B header,
20B nodes, 1..16B section padding quirk).

Reader constraints honoured (src/nlnx/bitmap.cpp reads ``4*w*h`` bytes from
blob data, ignoring the u32 compressed length):

* every LZ4 block must satisfy ``clen + 16 <= 4*w*h`` so the client's fixed
  read window covers the whole compressed stream (checked, hard failure);
* trailing file padding is emitted so the final blob's window stays in-file
  (the pre-existing archive already overran EOF by 16 KiB; no worse here).

Pure functions only; stdlib plus the repo's own NX/079 helpers.

Usage:
    inject_npc_nx.py --npc-id 9270052
"""
from __future__ import annotations

import argparse
import bisect
import os
import shutil
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
for p in (REPO / 'tools', REPO / 'tools/nx'):
    sys.path.insert(0, str(p))

from extract_079_data import decode_pixels, parse_img  # noqa: E402
from merge_skill_nx import (  # noqa: E402
    HEADER, aligned, build, find_child, intern, parse_nx, string_data_len,
)

T_NONE, T_INT, T_REAL, T_STRING, T_VECTOR, T_BITMAP = 0, 1, 2, 3, 4, 5
IMG_DIR = (REPO / 'link_repos/.ref-079max4/079MAX4-2023年度版/'
           '怀旧岛079MAX4客户端(登录器在bin内)/data/Npc')
STRING_XML = REPO / 'link_repos/MapleStory-Server/wz/String.wz/Npc.img.xml'
PROP_TYPE = {'u16': T_INT, 'int': T_INT, 'f32': T_REAL, 'f64': T_REAL,
             'str': T_STRING, 'null': T_NONE}


def fail(msg):
    sys.exit('inject_npc_nx: ' + msg)


# ---------------------------------------------------------------- LZ4 block

def lz4_compress_py(src):
    """Greedy literal/match LZ4 block encoder (fallback without the module)."""
    def emit_len(out, value):
        while value >= 255:
            out.append(255)
            value -= 255
        out.append(value)

    def emit_seq(out, lit, mlen, offset):
        ll = len(lit)
        mtok = 0 if mlen == 0 else (15 if mlen - 4 >= 15 else mlen - 4)
        token = ((15 if ll >= 15 else ll) << 4) | mtok
        out.append(token)
        if ll >= 15:
            emit_len(out, ll - 15)
        out += lit
        if mlen:
            out += struct.pack('<H', offset)
            if mlen - 4 >= 15:
                emit_len(out, mlen - 4 - 15)

    n = len(src)
    table, out = {}, bytearray()
    anchor = pos = 0
    limit, matchlimit = n - 12, n - 5  # MFLIMIT / LASTLITERALS
    while pos < limit:  # MFLIMIT: the last 12 bytes start no match
        seq = bytes(src[pos:pos + 4])
        cand = table.get(seq)
        table[seq] = pos
        if cand is None or pos - cand > 0xFFFF or src[cand:cand + 4] != seq:
            pos += 1
            continue
        mlen = 4
        while pos + mlen < matchlimit and src[cand + mlen] == src[pos + mlen]:
            mlen += 1
        emit_seq(out, src[anchor:pos], mlen, pos - cand)
        for k in range(pos, pos + mlen):
            table[bytes(src[k:k + 4])] = k
        pos += mlen
        anchor = pos
    emit_seq(out, src[anchor:], 0, 0)
    return bytes(out)


def lz4_compress(data):
    """Prefer the C lz4 module; fall back to the pure encoder above."""
    try:
        import lz4.block
    except ImportError:
        return lz4_compress_py(data)
    return lz4.block.compress(data, mode='high_compression', store_size=False)


# ------------------------------------------------- 079 tree -> NX descriptors

def string_index(strings):
    return {s: i for i, s in enumerate(strings)}


def prop_union(child, strings, index):
    kind = child['kind']
    if kind in ('int', 'u16'):
        return PROP_TYPE[kind], struct.pack('<q', child['value'])
    if kind in ('f32', 'f64'):
        return PROP_TYPE[kind], struct.pack('<d', float(child['value']))
    if kind == 'str':
        sid = intern(strings, index, child['value'].encode('utf-8'))
        return T_STRING, struct.pack('<II', sid, 0)
    if kind == 'vector':
        return T_VECTOR, struct.pack('<ii', child['x'], child['y'])
    if kind == 'null':
        return T_NONE, b'\0' * 8
    fail('unsupported property kind %r' % kind)


def prop_desc(name, child, strings, index):
    typ, union = prop_union(child, strings, index)
    return (intern(strings, index, name.encode('utf-8')), typ, union, ())


def sort_kids(kids, strings):
    """nl::node::get_child binary-searches children, so they must be sorted
    bytewise by name (the NoLifeWzToNx writer convention)."""
    return tuple(sorted(kids, key=lambda kid: strings[kid[0]]))


def canvas_desc(name, canvas, strings, index, bitmaps):
    w, h = canvas['width'], canvas['height']
    rgba = decode_pixels(canvas)
    block = lz4_compress(rgba)
    if len(block) + 16 > 4 * w * h:
        fail('frame %s: LZ4 block %d B exceeds the %d B client read window'
             % (name, len(block), 4 * w * h))
    bitmaps.append(struct.pack('<I', len(block)) + block)
    union = struct.pack('<IHH', len(bitmaps) - 1, w, h)
    kids = sort_kids([prop_desc(pn, pc, strings, index)
                      for pn, pc in canvas.get('children', [])], strings)
    return (intern(strings, index, name.encode('utf-8')), T_BITMAP, union, kids)


def list_desc(name, child, strings, index, bitmaps):
    kids = sort_kids([desc_for(cname, grand, strings, index, bitmaps)
                      for cname, grand in child.get('children', [])], strings)
    return (intern(strings, index, name.encode('utf-8')), T_NONE, b'\0' * 8, kids)


def desc_for(name, child, strings, index, bitmaps):
    if child['kind'] == 'canvas':
        return canvas_desc(name, child, strings, index, bitmaps)
    if child['kind'] == 'list':
        return list_desc(name, child, strings, index, bitmaps)
    return prop_desc(name, child, strings, index)


def is_state_list(child):
    return child['kind'] == 'list' and any(
        grand['kind'] == 'canvas' for _, grand in child.get('children', []))


def npc_subtree(img_root, npc_id, strings, index, bitmaps):
    """Descriptors for the wrapper: info(non-canvas props) + canvas states."""
    info_children, states = [], []
    for name, child in img_root.get('children', []):
        if is_state_list(child):
            states.append(list_desc(name, child, strings, index, bitmaps))
        else:
            info_children.append((name, child))
    info = (list_desc('info', {'children': info_children}, strings, index, bitmaps)
            if info_children else None)
    kids = sort_kids(([info] if info else []) + states, strings)
    wrapper = (intern(strings, index, ('%d.img' % npc_id).encode()), T_NONE,
               b'\0' * 8, kids)
    return wrapper


def flatten_children(desc, nodes):
    """Append desc's children block (contiguous); return (first, count)."""
    kids = desc[3]
    first = len(nodes)
    nodes.extend([None] * len(kids))
    for i, kid in enumerate(kids):
        kfirst, kcount = flatten_children(kid, nodes)
        nodes[first + i] = (kid[0], kfirst, kcount, kid[1], kid[2])
    return first, len(kids)


# ------------------------------------------------------------ table splicing

def shifted(nodes, at):
    return [(nm, ch + 1 if ch >= at else ch, num, typ, un)
            for nm, ch, num, typ, un in nodes]


def splice(nodes, at, insert, subtree):
    """Insert one node at `at` (shifting the tail) and append a subtree."""
    base = len(nodes) + 1
    moved = shifted(nodes, at)
    placed = [(nm, ch + base, num, typ, un) for nm, ch, num, typ, un in subtree]
    return moved[:at] + [insert] + moved[at:] + placed


def bmp_data_start(buf):
    node_off = aligned(HEADER.size)
    str_table = aligned(node_off + 20 * len(buf['nodes']))
    str_data = aligned(str_table + 8 * len(buf['strings']))
    aud_table = aligned(str_data + string_data_len(buf['strings']))
    bmp_table = aligned(aud_table + 8 * len(buf['audios']))
    aud_data = aligned(bmp_table + 8 * len(buf['bitmaps']))
    return aligned(aud_data + sum(map(len, buf['audios'])))


def last_bitmap_dims(buf):
    want = len(buf['bitmaps']) - 1
    for _, _, _, typ, union in buf['nodes']:
        if typ == 5:
            idx, w, h = struct.unpack('<IHH', union)
            if idx == want:
                return w, h
    fail('no node references the final bitmap')


def with_tail_pad(out, buf):
    """Pad the file so the client's fixed read window covers the last blob."""
    start = bmp_data_start(buf) + sum(len(b) for b in buf['bitmaps'][:-1])
    w, h = last_bitmap_dims(buf)
    need = start + 4 * w * h + 8 - len(out)
    return out + b'\0' * need if need > 0 else out


def backup(path, npc_id):
    bak = path.with_name('%s.pre-%d-backup' % (path.name, npc_id))
    if bak.exists():
        print('backup exists, keeping original: %s' % bak)
        return bak
    shutil.copy2(path, bak)
    return bak


# ------------------------------------------------------------------- inject

def read_string_entry(xml_path, npc_id):
    import xml.etree.ElementTree as ET
    for imgdir in ET.parse(xml_path).getroot().findall('imgdir'):
        if imgdir.get('name') == str(npc_id):
            entries = [(e.get('name'), e.get('value')) for e in imgdir.findall('string')]
            if entries:
                return entries
    fail('%s: no string entry for %d' % (xml_path, npc_id))


def child_names(nx, idx):
    _, base, num, _, _ = nx['nodes'][idx]
    return [nx['strings'][nx['nodes'][base + i][0]] for i in range(num)]


def sorted_slot(nx, idx, name):
    """Node index where `name` belongs among `idx`'s sorted children."""
    _, base, num, _, _ = nx['nodes'][idx]
    return base + bisect.bisect_left(child_names(nx, idx), name)


def inject_npc(nx, npc_id, img_root):
    strings, index = nx['strings'], string_index(nx['strings'])
    bitmaps = nx['bitmaps']
    root = nx['nodes'][0]
    name = ('%d.img' % npc_id).encode()
    if find_child(nx, name) is not None:
        fail('Npc.nx already contains %d.img' % npc_id)
    wrapper = npc_subtree(img_root, npc_id, strings, index, bitmaps)
    subtree = []
    first, count = flatten_children(wrapper, subtree)
    at = sorted_slot(nx, 0, name)
    insert = (wrapper[0], first + len(nx['nodes']) + 1, count, T_NONE, b'\0' * 8)
    nodes = splice(nx['nodes'], at, insert, subtree)
    nodes[0] = (root[0], root[1], root[2] + 1, root[3], root[4])
    nx['nodes'] = nodes
    return count + 1, len(subtree)


def inject_string(nx, npc_id, entries):
    strings, index = nx['strings'], string_index(nx['strings'])
    wrapper = find_child(nx, b'Npc.img')
    if wrapper is None:
        fail('String.nx: no Npc.img wrapper under root')
    npcw = nx['nodes'][wrapper]
    name = str(npc_id).encode()
    if name in child_names(nx, wrapper):
        fail('String.nx already contains %d' % npc_id)
    base = len(nx['nodes']) + 1
    subtree = [(intern(strings, index, key.encode()), base + i, 0, T_STRING,
                struct.pack('<II', intern(strings, index, value.encode('utf-8')), 0))
               for i, (key, value) in enumerate(sorted(entries))]
    insert = (intern(strings, index, name), base, len(entries),
              T_NONE, b'\0' * 8)
    at = sorted_slot(nx, wrapper, name)
    nodes = splice(nx['nodes'], at, insert, subtree)
    nodes[wrapper] = (npcw[0], npcw[1], npcw[2] + 1, npcw[3], npcw[4])
    nx['nodes'] = nodes


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--npc-id', type=int, required=True)
    ap.add_argument('--img', type=Path, default=None,
                    help='079 data/Npc/<id>.img (default: derived from --npc-id)')
    ap.add_argument('--string-xml', type=Path, default=STRING_XML)
    ap.add_argument('--assets-dir', type=Path, default=REPO / 'assets')
    args = ap.parse_args(argv)

    img_path = args.img or (IMG_DIR / ('%d.img' % args.npc_id))
    if not img_path.exists():
        fail('missing %s' % img_path)
    img_root = parse_img(img_path.read_bytes())
    entries = read_string_entry(args.string_xml, args.npc_id)

    npc_nx = args.assets_dir / 'Npc.nx'
    string_nx = args.assets_dir / 'String.nx'
    for path in (npc_nx, string_nx):
        backup(path, args.npc_id)

    old_size = npc_nx.stat().st_size
    nx = parse_nx(npc_nx)
    old_counts = (len(nx['nodes']), len(nx['strings']), len(nx['bitmaps']))
    wrappers, added = inject_npc(nx, args.npc_id, img_root)
    blob = with_tail_pad(build(nx), nx)
    tmp = npc_nx.with_suffix('.nx.tmp')
    tmp.write_bytes(blob)
    os.replace(tmp, npc_nx)
    print('%s: %d -> %d bytes (+%d)' % (npc_nx, old_size, len(blob),
                                        len(blob) - old_size))
    print('  nodes %d -> %d (+%d), strings %d -> %d (+%d), bitmaps %d -> %d (+%d)'
          % (old_counts[0], len(nx['nodes']), len(nx['nodes']) - old_counts[0],
             old_counts[1], len(nx['strings']), len(nx['strings']) - old_counts[1],
             old_counts[2], len(nx['bitmaps']), len(nx['bitmaps']) - old_counts[2]))
    print('  subtree: %d wrapper children, %d nodes appended' % (wrappers, added))

    old_size = string_nx.stat().st_size
    sx = parse_nx(string_nx)
    old_counts = (len(sx['nodes']), len(sx['strings']))
    inject_string(sx, args.npc_id, entries)
    blob = build(sx)
    tmp = string_nx.with_suffix('.nx.tmp')
    tmp.write_bytes(blob)
    os.replace(tmp, string_nx)
    print('%s: %d -> %d bytes (+%d)' % (string_nx, old_size, len(blob),
                                        len(blob) - old_size))
    print('  nodes %d -> %d (+%d), strings %d -> %d (+%d)'
          % (old_counts[0], len(sx['nodes']), len(sx['nodes']) - old_counts[0],
             old_counts[1], len(sx['strings']), len(sx['strings']) - old_counts[1]))
    print('  entry: %s' % ', '.join('%s=%r' % e for e in entries))


if __name__ == '__main__':
    main()
