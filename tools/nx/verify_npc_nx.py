#!/usr/bin/env python3
"""Verify an NPC injected into assets/Npc.nx + String.nx by inject_npc_nx.py.

Checks (all must pass):
1. Npc.nx: the <id>.img wrapper exists; states/frames/size/delay/origin match
   both the 079 ``.img`` source and the server ``Npc.wz/<id>.img.xml`` tree.
2. Every new bitmap blob decodes (pure-python LZ4 block decoder, same
   semantics as the client's LZ4_decompress_fast) to exactly ``4*w*h`` bytes
   equal to the ARGB4444->RGBA8888 expansion of the source canvas.
3. Decoded pixels equal the reference PNGs when present.
4. Old content is intact: full node-table equality modulo the one inserted
   index, string/bitmap table prefixes, wrapper subtree snapshots, counts.
5. String.nx: the <id> entry under Npc.img matches the server XML; the rest
   of the archive is intact.
6. Reader-window constraints (src/nlnx/bitmap.cpp reads 4*w*h bytes from
   blob data): clen+16 <= 4*w*h per new blob and the final blob's window
   stays inside the file.

Usage:
    verify_npc_nx.py --npc-id 9270052
"""
from __future__ import annotations

import argparse
import struct
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
for p in (REPO / 'tools', REPO / 'tools/nx'):
    sys.path.insert(0, str(p))

from extract_079_data import argb4444_to_rgba, parse_img  # noqa: E402
from inject_npc_nx import (  # noqa: E402
    IMG_DIR, bmp_data_start, child_names, last_bitmap_dims,
)
from merge_skill_nx import find_child, parse_nx  # noqa: E402

SERVER_NPC_XML = REPO / 'link_repos/MapleStory-Server/wz/Npc.wz'
SERVER_STRING_XML = REPO / 'link_repos/MapleStory-Server/wz/String.wz/Npc.img.xml'
PNG_DIR = REPO / 'link_repos/.ref-079max4/extracted'
T_INT, T_STRING, T_VECTOR, T_BITMAP = 1, 3, 4, 5
RESULTS = []


def check(name, ok, detail=''):
    RESULTS.append(ok)
    print('%-4s %s%s' % ('PASS' if ok else 'FAIL', name,
                         (' -- ' + detail) if detail else ''))
    return ok


# ----------------------------------------------------------------- nx walking

def children(nx, idx):
    _, base, num, _, _ = nx['nodes'][idx]
    return [nx['nodes'][base + i] for i in range(num)]


def by_name(nx, idx):
    """Map child name -> node index for the children of `idx`."""
    _, base, num, _, _ = nx['nodes'][idx]
    return {nx['strings'][nx['nodes'][base + i][0]]: base + i for i in range(num)}


def frame_data(nx, idx):
    node = nx['nodes'][idx]
    bmpidx, w, h = struct.unpack('<IHH', node[4])
    delay = origin = None
    for kid in children(nx, idx):
        name, typ = nx['strings'][kid[0]], kid[3]
        if name == b'delay' and typ == T_INT:
            delay = struct.unpack('<q', kid[4])[0]
        elif name == b'origin' and typ == T_VECTOR:
            origin = struct.unpack('<ii', kid[4])
    blob = nx['bitmaps'][bmpidx]
    return {'w': w, 'h': h, 'delay': delay, 'origin': origin,
            'clen': struct.unpack('<I', blob[:4])[0], 'block': blob[4:]}


def img_frames(img_root):
    states = {}
    for state, child in img_root.get('children', []):
        if child['kind'] == 'list' and any(c['kind'] == 'canvas' for _, c in child['children']):
            states[state] = {name: c for name, c in child['children']}
    return states


# --------------------------------------------------------------- pure LZ4

def lz4_decode(block):
    out = bytearray()
    i, n = 0, len(block)
    while i < n:
        token = block[i]
        i += 1
        length = token >> 4
        if length == 15:
            while True:
                b = block[i]
                i += 1
                length += b
                if b != 255:
                    break
        out += block[i:i + length]
        i += length
        if i >= n:
            break
        offset = block[i] | (block[i + 1] << 8)
        i += 2
        mlen = (token & 15) + 4
        if token & 15 == 15:
            while True:
                b = block[i]
                i += 1
                mlen += b
                if b != 255:
                    break
        start = len(out) - offset
        for k in range(mlen):
            out.append(out[start + k])
    return bytes(out)


# ------------------------------------------------------------------- PNG

def png_rgba(path):
    """RGBA bytes of a PNG (PIL, or a filter-0 fallback for our own writer)."""
    try:
        from PIL import Image
        with Image.open(path) as im:
            return im.convert('RGBA').tobytes()
    except ImportError:
        pass
    import zlib
    data, pos, idat, (w, h, depth, ctype) = path.read_bytes(), 8, b'', (0, 0, 0, 0)
    while pos < len(data):
        length = struct.unpack_from('>I', data, pos)[0]
        tag = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + length]
        if tag == b'IHDR':
            w, h, depth, ctype = struct.unpack('>IIBB', body[:10])
        elif tag == b'IDAT':
            idat += body
        pos += 12 + length
    assert (depth, ctype) == (8, 6), 'not RGBA8'
    raw, stride, out = zlib.decompress(idat), w * 4, bytearray()
    for y in range(h):
        ftype = raw[y * (stride + 1)]
        row = raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)]
        assert ftype == 0, 'unsupported filter %d' % ftype
        out += row
    return bytes(out)


# ----------------------------------------------------------- server XML

def xml_frames(xml_path, npc_id):
    """states -> list of {w,h,delay,origin} per canvas, from the server XML."""
    root = ET.parse(xml_path).getroot()
    states = {}
    for imgdir in root.findall('imgdir'):
        if imgdir.get('name') == 'info':
            continue
        frames = []
        for canvas in imgdir.findall('canvas'):
            props = {p.get('name'): p for p in canvas}
            frames.append({
                'w': int(canvas.get('width')), 'h': int(canvas.get('height')),
                'delay': int(props['delay'].get('value')) if 'delay' in props else None,
                'origin': (int(props['origin'].get('x')), int(props['origin'].get('y')))
                          if 'origin' in props else None,
            })
        states[imgdir.get('name')] = frames
    return states


def xml_entry(xml_path, npc_id):
    for imgdir in ET.parse(xml_path).getroot().findall('imgdir'):
        if imgdir.get('name') == str(npc_id):
            return {e.get('name'): e.get('value') for e in imgdir.findall('string')}
    return {}


# ------------------------------------------------------------------ compare

def snap(nx, idx):
    """Comparable recursive snapshot of a subtree (ignores raw indices)."""
    name, base, num, typ, union = nx['nodes'][idx]
    return (nx['strings'][name], num, typ, union,
            tuple(snap(nx, base + i) for i in range(num)))


def table_intact(label, old, new, at, added, edited=()):
    """`edited`: old indices with intentional changes (already in `new`)."""
    if len(new) != len(old) + added:
        return check('%s node count %d -> %d (+%d expected)' % (label, len(old), len(new), added),
                     False)
    for i, node in enumerate(old):
        j = i if i < at else i + 1
        expect = new[j] if i in edited else (
            node[0], node[1] + 1 if node[1] >= at else node[1],
            node[2], node[3], node[4])
        if new[j] != expect:
            return check('%s node table intact' % label, False,
                         'first diff at old index %d' % i)
    return check('%s node table intact (%d nodes, +%d)' % (label, len(old), added), True)


def verify_npc(nx, old, npc_id, img_root, xml_path, png_dir):
    wrapper = bsearch_child(nx, 0, ('%d.img' % npc_id).encode())
    check('Npc.nx has %d.img wrapper (client binary search)' % npc_id,
          wrapper is not None and find_child(nx, ('%d.img' % npc_id).encode()) == wrapper)
    check('root children sorted with %d.img in place' % npc_id,
          child_names(nx, 0) == sorted(child_names(nx, 0)))
    states = by_name(nx, wrapper)
    check('wrapper children sorted (info + states %s)' % sorted(xml_frames(xml_path, npc_id)),
          sorted(states) == sorted([b'info'] + [s.encode() for s in xml_frames(xml_path, npc_id)])
          and list(states) == sorted(states))

    src_tree, ok_px, ok_png, nframes = img_frames(img_root), 0, 0, 0
    for state, frames in sorted(xml_frames(xml_path, npc_id).items()):
        state_idx = states[state.encode()]
        names = child_names(nx, state_idx)
        check('state %s children sorted' % state, names == sorted(names))
        for i, want in enumerate(frames):
            idx = bsearch_child(nx, state_idx, str(i).encode())
            data = frame_data(nx, idx) if idx is not None else {
                'w': 0, 'h': 0, 'delay': None, 'origin': None, 'clen': 0, 'block': b''}
            decoded = lz4_decode(data['block'])
            ok = (idx is not None and nx['nodes'][idx][3] == T_BITMAP
                  and bsearch_child(nx, idx, b'delay') is not None
                  and bsearch_child(nx, idx, b'origin') is not None
                  and (data['w'], data['h']) == (want['w'], want['h'])
                  and data['delay'] == want['delay']
                  and data['origin'] == want['origin']
                  and len(decoded) == 4 * data['w'] * data['h']
                  and data['clen'] + 16 <= 4 * data['w'] * data['h'])
            check('frame %s/%d %dx%d delay=%s origin=%s clen=%d lz4->%dB' % (
                state, i, data['w'], data['h'], data['delay'], data['origin'],
                data['clen'], len(decoded)), ok)
            nframes += 1
            canvas = src_tree[state][str(i)]
            expect = argb4444_to_rgba(canvas['pixels'], canvas['width'], canvas['height'])
            ok_px += decoded == expect
            png = png_dir / ('%s.%d.png' % (state, i))
            if png.exists():
                ok_png += png_rgba(png) == expect
    check('all %d frames decode to the source ARGB4444 expansion' % nframes,
          ok_px == nframes)
    check('all frames match the reference PNGs pixel-for-pixel', ok_png == nframes)
    return nframes


def verify_npc_tables(nx, old, npc_id):
    root_old = old['nodes'][0]
    at = find_child(nx, ('%d.img' % npc_id).encode())
    added = len(nx['nodes']) - len(old['nodes'])
    table_intact('Npc.nx', old['nodes'], nx['nodes'], at, added, edited=(0,))
    old_names = child_names(old, 0)
    check('Npc.nx root children %d -> %d, sorted slot for %d.img' % (
        root_old[2], nx['nodes'][0][2], npc_id),
        nx['nodes'][0][1] == root_old[1] and nx['nodes'][0][2] == root_old[2] + 1
        and child_names(nx, 0) == sorted(old_names + [('%d.img' % npc_id).encode()])
        and child_names(nx, 0)[at - root_old[1]] == ('%d.img' % npc_id).encode())
    check('Npc.nx strings appended only (%d -> %d)' % (len(old['strings']), len(nx['strings'])),
          nx['strings'][:len(old['strings'])] == old['strings'])
    check('Npc.nx bitmaps appended only (%d -> %d)' % (len(old['bitmaps']), len(nx['bitmaps'])),
          nx['bitmaps'][:len(old['bitmaps'])] == old['bitmaps'])
    for probe in (b'0002000.img', b'9030000.img', b'9977777.img'):
        check('%s subtree unchanged' % probe.decode(),
              snap(old, find_child(old, probe)) == snap(nx, find_child(nx, probe)))


def verify_window(nx, path):
    start = bmp_data_start(nx)
    file_size = path.stat().st_size
    off = start + sum(len(b) for b in nx['bitmaps'][:-1])
    w, h = last_bitmap_dims(nx)
    check('final blob read window inside file (%d <= %d)' % (off + 4 * w * h, file_size),
          off + 4 * w * h <= file_size)


def verify_string(sx, so, npc_id):
    entry = find_child(sx, b'Npc.img')
    old_wrapper = find_child(so, b'Npc.img')
    name = str(npc_id).encode()
    node = bsearch_child(sx, entry, name)
    check('String.nx Npc.img has %d entry (client binary search)' % npc_id,
          node is not None and child_by_name(sx, entry, name) == node)
    got = {k.decode(): sx['strings'][struct.unpack('<I', sx['nodes'][i][4][:4])[0]]
           .decode('utf-8')
           for k, i in by_name(sx, node).items()} if node is not None else {}
    want = xml_entry(SERVER_STRING_XML, npc_id)
    check('String.nx %d entry == server XML' % npc_id, got == want,
          '%r vs %r' % (got, want))
    check('entry children sorted', node is not None
          and child_names(sx, node) == sorted(child_names(sx, node)))
    oldw = so['nodes'][old_wrapper]
    table_intact('String.nx', so['nodes'], sx['nodes'], node,
                 len(sx['nodes']) - len(so['nodes']), edited=(old_wrapper,))
    check('String.nx Npc.img child count %d -> %d (+1), sorted' % (
        oldw[2], sx['nodes'][entry][2]),
        sx['nodes'][entry][2] == oldw[2] + 1
        and child_names(sx, entry) == sorted(child_names(so, old_wrapper) + [name]))
    check('String.nx strings appended only',
          sx['strings'][:len(so['strings'])] == so['strings'])
    for probe in (b'10000', b'9030000', b'9999999'):
        a = child_by_name(so, old_wrapper, probe)
        b = child_by_name(sx, entry, probe)
        check('String.nx Npc.img/%s unchanged' % probe.decode(),
              a is not None and b is not None and snap(so, a) == snap(sx, b))


def child_by_name(nx, parent, name):
    _, base, num, _, _ = nx['nodes'][parent]
    for i in range(num):
        if nx['strings'][nx['nodes'][base + i][0]] == name:
            return base + i
    return None


def bsearch_child(nx, parent, name):
    """Replica of nl::node::get_child: unsigned byte-wise binary search."""
    _, base, num, _, _ = nx['nodes'][parent]
    lo, count = base, num
    while count:
        half = count >> 1
        mid = lo + half
        s = nx['strings'][nx['nodes'][mid][0]]
        if s < name:
            lo = mid + 1
            count -= half + 1
        elif s > name:
            count = half
        else:
            return mid
    return None


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--npc-id', type=int, required=True)
    ap.add_argument('--assets-dir', type=Path, default=REPO / 'assets')
    ap.add_argument('--old-npc', type=Path, default=None)
    ap.add_argument('--old-string', type=Path, default=None)
    ap.add_argument('--png-dir', type=Path, default=None)
    args = ap.parse_args(argv)
    npc_id = args.npc_id
    old_npc = args.old_npc or args.assets_dir / ('Npc.nx.pre-%d-backup' % npc_id)
    old_string = args.old_string or args.assets_dir / ('String.nx.pre-%d-backup' % npc_id)
    png_dir = args.png_dir or (PNG_DIR / str(npc_id))

    nx = parse_nx(args.assets_dir / 'Npc.nx')
    old = parse_nx(old_npc)
    img_root = parse_img((IMG_DIR / ('%d.img' % npc_id)).read_bytes())
    verify_npc(nx, old, npc_id, img_root, SERVER_NPC_XML / ('%d.img.xml' % npc_id), png_dir)
    verify_npc_tables(nx, old, npc_id)
    verify_window(nx, args.assets_dir / 'Npc.nx')

    verify_string(parse_nx(args.assets_dir / 'String.nx'), parse_nx(old_string), npc_id)

    print('\n%d/%d checks passed' % (sum(RESULTS), len(RESULTS)))
    return 0 if all(RESULTS) else 1


if __name__ == '__main__':
    sys.exit(main())
