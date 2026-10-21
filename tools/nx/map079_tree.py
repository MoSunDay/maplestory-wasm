#!/usr/bin/env python3
"""Shared helpers for the 079MAX4 free-market map injection.

Converts the pre-extracted ``map079`` tree JSONs (kinds: u16/int/f32/f64/str/
null/vector/uol/canvas/list/convex) into NX node descriptors and provides the
NX walking primitives (path lookup, per-child structural digests) reused by
``inject_map_nx.py`` and ``verify_map_nx.py``.

Canvas nodes become NX bitmap nodes whose blob is the LZ4 block of the
pre-extracted RGBA8888 ``pixels/*.bin`` (alpha in byte3, stride 4*w, zero
padded).  Reader constraint (src/nlnx/bitmap.cpp reads a fixed 4*w*h window
from the blob): ``clen + 16 <= 4*w*h``; the shipping v83 Map.nx itself keeps
twenty 1x1 bitmaps whose 5-byte block overruns the 4-byte window by one, so
tiny windows (<= 64 B) are allowed the same 1-byte slack.

Pure functions only; stdlib plus the repo's own NX helpers.
"""
from __future__ import annotations

import hashlib
import json
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
for _p in (REPO / 'tools', REPO / 'tools/nx'):
    sys.path.insert(0, str(_p))

from inject_npc_nx import (  # noqa: E402
    child_names, flatten_children, lz4_compress, lz4_compress_py, prop_desc,
    sort_kids, sorted_slot,
)
from merge_skill_nx import intern  # noqa: E402

T_NONE, T_INT, T_REAL, T_STRING, T_VECTOR, T_BITMAP = 0, 1, 2, 3, 4, 5
MAP079_DIR = REPO / 'link_repos/.ref-079max4/extracted/map079'
CONTAINER_KINDS = ('list', 'convex')


def fail(msg):
    sys.exit('map079_tree: ' + msg)


def load_tree(path):
    try:
        with open(path, encoding='utf-8') as fh:
            return json.load(fh)
    except OSError as err:
        fail('cannot read %s: %s' % (path, err))


# ------------------------------------------------ tree JSON -> NX descriptors

def lz4_seq_cost(llen, mlen):
    """Bytes of one LZ4 sequence (token [+ext] literals [+offset [+ext]])."""
    cost = 1 + llen
    if llen >= 15:
        cost += 1 + (1 if llen - 15 >= 255 else 0)
    if mlen:
        cost += 2
        if mlen - 4 >= 15:
            cost += 1 + (1 if mlen - 4 - 15 >= 255 else 0)
    return cost


def lz4_emit_seq(out, lit, mlen, offset):
    def ext(value):
        while value >= 255:
            out.append(255)
            value -= 255
        out.append(value)
    ll = len(lit)
    mtok = 0 if mlen == 0 else (15 if mlen - 4 >= 15 else mlen - 4)
    out.append(((15 if ll >= 15 else ll) << 4) | mtok)
    if ll >= 15:
        ext(ll - 15)
    out += lit
    if mlen:
        out += struct.pack('<H', offset)
        if mlen - 4 >= 15:
            ext(mlen - 4 - 15)


def lz4_compress_tiny(src):
    """Minimum-size LZ4 block for tiny inputs (DP over match boundaries).

    Keeps the unavoidable over-window blocks of 1-3 pixel canvases as short
    as the format allows (the shipping v83 archive already stores twenty
    1x1 canvases whose 5-byte block overruns the 4-byte window by one).
    """
    n = len(src)
    last_lit = n - 5   # LASTLITERALS: the final 5 bytes must be literals
    last_match = n - 12  # MFLIMIT: the last match must start 12 bytes before end
    best = [0] + [None] * n  # cost to reach a post-match boundary
    back = [None] * (n + 1)
    for b in range(n + 1):
        if best[b] is None:
            continue
        for llen in range(n - b + 1):
            p = b + llen
            head = lz4_seq_cost(llen, 0)
            cand = best[b] + head
            if p == n and (best[n] is None or cand < best[n]):
                best[n], back[n] = cand, (b, llen, 0, 0)
            if p > last_match:
                continue  # no match may start here (MFLIMIT)
            for q in range(p):
                m = 0
                while p + m < n and src[q + m] == src[p + m]:
                    m += 1
                m = min(m, last_lit - p)  # matches end before LASTLITERALS
                for mlen in range(4, m + 1):
                    end = p + mlen
                    cost = best[b] + lz4_seq_cost(llen, mlen)
                    if best[end] is None or cost < best[end]:
                        best[end], back[end] = cost, (b, llen, mlen, p - q)
    out = bytearray()
    chain = []
    cur = n
    while cur:
        chain.append(back[cur])
        cur = back[cur][0]
    for b, llen, mlen, offset in reversed(chain):
        lz4_emit_seq(out, src[b:b + llen], mlen, offset)
    return bytes(out)


def best_tiny_block(rgba):
    blocks = [lz4_compress(rgba), lz4_compress_py(rgba)]
    if len(rgba) <= 64:
        blocks.append(lz4_compress_tiny(rgba))
    return min(blocks, key=len)


def window_ok(clen, w, h):
    """Client read-window constraint, with the reference writer's tiny slack.

    src/nlnx/bitmap.cpp reads a fixed 4*w*h window; blocks must fit inside
    (plus margin) -- except tiny canvases, where LZ4's minimum encoding can
    overrun by a byte or two exactly like the shipping v83 archive's twenty
    1x1 bitmaps (5-byte blocks in 4-byte windows).
    """
    win = 4 * w * h
    if win > 64:
        return clen + 16 <= win
    return clen <= win + 2


def canvas_desc(name, canvas, strings, index, bitmaps, pixel_root):
    w, h = canvas['width'], canvas['height']
    rgba = Path(pixel_root, canvas['pixels']).read_bytes()
    if len(rgba) != 4 * w * h:
        fail('%s: %d bytes, expected %d' % (canvas['pixels'], len(rgba), 4 * w * h))
    block = lz4_compress(rgba)
    if not window_ok(len(block), w, h):
        fail('%s: LZ4 block %d B exceeds the %d B client read window'
             % (canvas['pixels'], len(block), 4 * w * h))
    bitmaps.append(struct.pack('<I', len(block)) + block)
    union = struct.pack('<IHH', len(bitmaps) - 1, w, h)
    kids = sort_kids([desc_for(pn, pc, strings, index, bitmaps, pixel_root)
                      for pn, pc in canvas_props(canvas)], strings)
    return (intern_name(name, strings, index), T_BITMAP, union, kids)


def canvas_props(canvas):
    """Canvas children as (name, node) pairs; input JSON stores plain dicts."""
    return [(c['name'], c) for c in canvas.get('children', [])]


def intern_name(name, strings, index):
    from merge_skill_nx import intern
    return intern(strings, index, name.encode('utf-8'))


def sort_keyed(name_id, kids):
    return name_id


def container_desc(name, child, strings, index, bitmaps, pixel_root):
    kids = sort_kids([desc_for(c['name'], c, strings, index, bitmaps, pixel_root)
                      for c in child.get('children', [])], strings)
    return (intern_name(name, strings, index), T_NONE, b'\0' * 8, kids)


def desc_for(name, child, strings, index, bitmaps, pixel_root):
    kind = child['kind']
    if kind == 'canvas':
        return canvas_desc(name, child, strings, index, bitmaps, pixel_root)
    if kind in CONTAINER_KINDS:
        return container_desc(name, child, strings, index, bitmaps, pixel_root)
    if kind == 'uol':
        fail('uol nodes are unsupported (none expected in map079 data)')
    return prop_desc(name, child, strings, index)


def wrapper_desc(tree, wrapper_name, strings, index, bitmaps, pixel_root):
    """Descriptor for the "<tree-name>.img" wrapper over the tree's children."""
    kids = sort_kids([desc_for(c['name'], c, strings, index, bitmaps, pixel_root)
                      for c in tree.get('children', [])], strings)
    return (intern_name(wrapper_name, strings, index), T_NONE, b'\0' * 8, kids)


def subtree_nodes(wrapper):
    """Flatten a wrapper descriptor into splicable node tuples."""
    placed = []
    _, count = flatten_children(wrapper, placed)
    return placed, count


# --------------------------------------------------------- NX walking helpers

def node_name(nx, idx):
    return nx['strings'][nx['nodes'][idx][0]]


def find_path(nx, names):
    """Resolve a bytes-name path from the root; None when absent."""
    idx = 0
    for name in names:
        idx = next((c for c in child_indices(nx, idx) if node_name(nx, c) == name), None)
        if idx is None:
            return None
    return idx


def child_indices(nx, idx):
    _, base, num, _, _ = nx['nodes'][idx]
    return range(base, base + num)


def require_path(nx, names):
    idx = find_path(nx, names)
    if idx is None:
        fail('%s: node not found' % '/'.join(n.decode() for n in names))
    return idx


def subtree_digest(nx, idx):
    """(node count, sha256) over the preorder (name, type, union) stream."""
    sha = hashlib.sha256()
    count = 0
    stack = [idx]
    while stack:
        cur = stack.pop()
        sid, base, num, typ, union = nx['nodes'][cur]
        name_b = nx['strings'][sid]
        sha.update(struct.pack('<IH', len(name_b), typ))
        sha.update(name_b)
        sha.update(union)
        count += 1
        if num:
            stack.extend(base + num - 1 - k for k in range(num))
    return count, sha.hexdigest()


def child_stats(nx, idx):
    """{child name: [subtree count, digest]} for spot-check baselines.

    First occurrence wins for duplicated sibling names (the original v83
    archive has a few), matching what ``find_path`` resolves.
    """
    stats = {}
    for c in child_indices(nx, idx):
        name = node_name(nx, c).decode('utf-8', 'replace')
        if name in stats:
            continue
        count, digest = subtree_digest(nx, c)
        stats[name] = [count, digest]
    return stats


# -------------------------------------------------------------- pure LZ4 read

def lz4_decompress(block, size):
    """Decode one LZ4 block to exactly `size` bytes (client semantics)."""
    if size == 0:
        return b''
    try:
        import lz4.block
        return lz4.block.decompress(block, uncompressed_size=size)
    except ImportError:
        pass
    out = bytearray()
    i, n = 0, len(block)
    while i < n and len(out) < size:
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
        if i >= n or len(out) >= size:
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
