#!/usr/bin/env python3
"""Verify a Map.nx edited by remove_map_life_nx.py against its backup.

Every check prints a PASS/FAIL line; any FAIL exits non-zero.

 1. headers: identical except ``node_count``; identical file sizes;
    ``new.node_count == backup.node_count - removed`` (table row delta).
 2. non-node-table bytes equal: ``[0, node_offset)`` and
    ``[node_offset+20*old_count, EOF)`` compared via chunked sha256 (4 MiB).
 3. new table legality: ``child+num <= node_count`` for ``num>0``,
    ``name_id < string_count``, root reachability closure == node_count
    (compacted tables must not keep orphans).
 4. per ``mapid:npcid``: no ``id==npcid`` life entry remains; life entry
    count is backup-1; the remaining entries match the backup's remaining
    entries in order, node for node (name/type/union 8B/int/string values);
    the map's ``info`` subtree is node-for-node equal when present.
 5. seeded sample (random.Random(42), 8 imgs) of unmodified
    ``<mapid>.img`` trees: recursive structural digest (name string, type,
    union bytes, child digests in stored name-sorted order) equal.

Pure functions only; mmap + numpy node tables (see remove_map_life_nx).

Usage:
    verify_map_life_nx.py <new.nx> <backup.nx> <mapid:npcid> [...]
"""
from __future__ import annotations

import hashlib
import random
import struct
import sys

import numpy as np

from remove_map_life_nx import (entry_npc_id, find_child, load_nx,
                                parse_target, reachability, scalar_value)

T_INT, T_STRING = 1, 3
CHUNK = 4 << 20  # 4 MiB 分块哈希
SAMPLE_SEED = 42
SAMPLE_COUNT = 8


def record(out, label, ok, detail=''):
    out.append((label, bool(ok), detail))


# ---------------------------------------------------------------- checks 1-2

def check_headers(new, bak, out):
    hn, hb = new['header'], bak['header']
    fields_ok = all(hn[i] == hb[i] for i in (0, 2, 3, 4, 5, 6, 7, 8))
    record(out, 'header: all fields equal except node_count', fields_ok,
           'node_count new=%d backup=%d' % (hn[1], hb[1]))
    sn, sb = new['mm'].size(), bak['mm'].size()
    record(out, 'file sizes equal', sn == sb, '%d vs %d' % (sn, sb))
    delta = hb[1] - hn[1]
    rows = len(bak['nodes']) - len(new['nodes'])
    record(out, 'node_count delta == table row delta', rows == delta > 0,
           'removed %d nodes' % delta)


def region_digest(mm, start, end):
    h = hashlib.sha256()
    pos = start
    while pos < end:
        h.update(bytes(mm[pos:min(pos + CHUNK, end)]))
        pos += CHUNK
    return h.hexdigest()


def check_regions(new, bak, out):
    node_off = new['header'][2]
    old_end = node_off + 20 * len(bak['nodes'])
    size = new['mm'].size()
    # [0, node_off) 含 header；node_count(偏移 4..8) 是本次唯一预期改动，
    # 排除这 4 字节后逐字节比对（区域小，无需分块）。
    head_n = bytes(new['mm'][:node_off])
    head_b = bytes(bak['mm'][:node_off])
    same_head = (head_n[:4] == head_b[:4]
                 and head_n[8:] == head_b[8:])
    record(out, 'bytes [0,%d) identical except node_count' % node_off,
           same_head)
    dn = region_digest(new['mm'], old_end, size)
    db = region_digest(bak['mm'], old_end, size)
    record(out, 'bytes [%d,%d) identical (sha256, %d MiB chunks)'
           % (old_end, size, CHUNK >> 20), dn == db)


# ------------------------------------------------------------------- check 3

def check_table(new, out):
    nodes = new['nodes']
    count, scount = len(nodes), new['header'][3]
    num = nodes['num'].astype(np.int64)
    child = nodes['child'].astype(np.int64)
    live = num > 0
    record(out, 'table: child+num <= node_count (num>0)',
           bool((child[live] + num[live] <= count).all()))
    record(out, 'table: name_id < string_count',
           bool((nodes['name'] < scount).all()))
    mark = reachability(new, frozenset())
    reached = sum(mark)
    record(out, 'table: root closure == node_count (no orphans)',
           reached == count, 'reachable %d of %d' % (reached, count))


# ------------------------------------------------------------------- check 4

def img_and_life(nx, mapid):
    nodes, strings = nx['nodes'], nx['strings']
    group = find_child(nodes, strings, find_child(nodes, strings, 0, 'Map'),
                       'Map' + mapid[0])
    img = find_child(nodes, strings, group, mapid + '.img')
    return img, find_child(nodes, strings, img, 'life')


def equal_subtrees(a, b, ai, bi):
    """迭代成对 DFS：名字/类型/union 8B/标量值与孩子序列逐一相等。"""
    an, bn = a['nodes'], b['nodes']
    stack = [(ai, bi)]
    while stack:
        i, j = stack.pop()
        ni, nj = an[i], bn[j]
        if (int(ni['num']) != int(nj['num'])
                or int(ni['typ']) != int(nj['typ'])
                or a['strings'][ni['name']] != b['strings'][nj['name']]
                or bytes(ni['union']) != bytes(nj['union'])):
            return False
        typ = int(ni['typ'])
        if typ in (T_INT, T_STRING) and (
                scalar_value(an, a['strings'], i)
                != scalar_value(bn, b['strings'], j)):
            return False
        c0, n = int(ni['child']), int(ni['num'])
        d0 = int(nj['child'])
        for k in range(n):
            stack.append((c0 + k, d0 + k))
    return True


def check_life(new, bak, mapid, npcid, out):
    nodes = new['nodes']
    img, life = img_and_life(new, mapid)
    bimg, blife = img_and_life(bak, mapid)
    if life is None or blife is None:
        record(out, '%s: life node present in both files' % mapid, False)
        return
    c0, num = int(nodes[life]['child']), int(nodes[life]['num'])
    bc0, bnum = int(bak['nodes'][blife]['child']), \
        int(bak['nodes'][blife]['num'])
    left = [e for e in range(c0, c0 + num) if entry_npc_id(new, e) == npcid]
    record(out, '%s: no life entry with id %s remains' % (mapid, npcid),
           not left, '%d found' % len(left))
    removed = [e for e in range(bc0, bc0 + bnum)
               if entry_npc_id(bak, e) == npcid]
    record(out, '%s: backup had exactly 1 entry with id %s' % (mapid, npcid),
           len(removed) == 1, '%d found' % len(removed))
    record(out, '%s: life num %d == backup %d - 1' % (mapid, num, bnum),
           num == bnum - 1)
    keep = [e for e in range(bc0, bc0 + bnum) if e != removed[0]]
    pairs = list(zip(range(c0, c0 + num), keep))
    same = (len(pairs) == num and len(keep) == bnum - 1
            and all(equal_subtrees(new, bak, a, e) for a, e in pairs))
    record(out, '%s: remaining life entries equal backup (ordered)' % mapid,
           same)
    info = find_child(nodes, new['strings'], img, 'info')
    binfo = find_child(bak['nodes'], bak['strings'], bimg, 'info')
    if info is not None and binfo is not None:
        record(out, "%s: 'info' subtree equal (node-for-node)" % mapid,
               equal_subtrees(new, bak, info, binfo))
    else:
        record(out, "%s: 'info' presence consistent" % mapid,
               (info is None) == (binfo is None))


# ------------------------------------------------------------------- check 5

def digests(nx, roots):
    """结构摘要 blake2b(name_str, type, num, union, 孩子摘要按序)。
    NX 孩子按名字排序存储，故存储顺序即“排序后孩子”顺序。"""
    nodes, strings = nx['nodes'], nx['strings']
    memo = {}
    for root in roots:
        stack = [(root, False)]
        while stack:
            i, expanded = stack.pop()
            if i in memo:
                continue
            nd = nodes[i]
            c0, n = int(nd['child']), int(nd['num'])
            if not expanded:
                stack.append((i, True))
                for k in range(n):
                    if c0 + k not in memo:
                        stack.append((c0 + k, False))
                continue
            h = hashlib.blake2b(digest_size=16)
            h.update(strings[nd['name']].encode('utf-8'))
            h.update(struct.pack('<HH', int(nd['typ']), n))
            h.update(bytes(nd['union']))
            for k in range(n):
                h.update(memo[c0 + k])
            memo[i] = h.digest()
    return [memo[r] for r in roots]


def check_samples(new, bak, targets, out):
    nodes, strings = new['nodes'], new['strings']
    skip = {m for m, _ in targets}
    cands = []
    grp = find_child(nodes, strings, 0, 'Map')
    for g in range(int(nodes[grp]['child']),
                   int(nodes[grp]['child']) + int(nodes[grp]['num'])):
        gname = strings[nodes[g]['name']]
        if not gname.startswith('Map'):
            continue
        for e in range(int(nodes[g]['child']),
                       int(nodes[g]['child']) + int(nodes[g]['num'])):
            nm = strings[nodes[e]['name']]
            if nm.endswith('.img') and nm[:-4] not in skip:
                cands.append(nm[:-4])
    cands.sort()
    picks = random.Random(SAMPLE_SEED).sample(cands,
                                              min(SAMPLE_COUNT, len(cands)))

    def img_of(nx, mapid):
        return img_and_life(nx, mapid)[0]

    a = digests(new, [img_of(new, m) for m in picks])
    b = digests(bak, [img_of(bak, m) for m in picks])
    record(out, 'sample: %d unmodified imgs digest-equal (seed %d)'
           % (len(picks), SAMPLE_SEED), a == b, ', '.join(picks))


# ------------------------------------------------------------------- main

def main(argv=None):
    args = argv if argv is not None else sys.argv[1:]
    if len(args) < 3:
        sys.exit('usage: verify_map_life_nx.py <new.nx> <backup.nx> '
                 '<mapid:npcid> ...')
    from remove_map_life_nx import parse_target
    pairs = [parse_target(s) for s in args[2:]]
    new, bak = load_nx(args[0]), load_nx(args[1])
    out = []
    check_headers(new, bak, out)
    check_regions(new, bak, out)
    check_table(new, out)
    for mapid, npcid in pairs:
        check_life(new, bak, mapid, npcid, out)
    check_samples(new, bak, pairs, out)
    bad = 0
    for label, ok, detail in out:
        print('%s %s%s' % ('PASS' if ok else 'FAIL', label,
                           (' -- ' + detail) if detail else ''))
        bad += 0 if ok else 1
    print('%d/%d checks passed' % (len(out) - bad, len(out)))
    if bad:
        print('VERIFY FAILED')
        return 1
    print('ALL PASS')
    return 0


if __name__ == '__main__':
    sys.exit(main())
