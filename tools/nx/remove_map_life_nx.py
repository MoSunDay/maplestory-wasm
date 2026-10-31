#!/usr/bin/env python3
"""Remove map ``life`` NPC entries from a Map.nx archive in place.

For each ``<mapid>:<npcid>`` argument, the unique imgdir entry under
``Map/Map<mapid[0]>/<mapid>.img/life`` whose ``id`` child equals ``npcid``
(string- or int-typed values both match) is dropped: the surviving node
table is the reachability closure of the root with the target entries
skipped, renumbered densely.  Blocks already orphaned before the edit are
pruned by the same walk.  File size never changes -- header ``node_count``
is patched, the compacted table overwrites ``[node_offset,
node_offset+20*new_count)`` and the freed tail is zeroed.

Layout reference (tools/nx/merge_skill_nx.py, not imported to avoid full
blob loads): 52B header ``<IIQIQIQIQ``; 20B nodes ``<IIHH8s`` =
name_id, first_child, num, type, union.  Children of a node are the
contiguous range ``[child, child+num)`` in name-sorted order; the table is
a DAG, so deletion is decided per node by reachability, never per block.

Backup convention (tools/nx/inject_npc_nx.py): ``<file>.pre-<tag>-backup``
is written from the pristine file before any mutation; an existing backup
aborts the whole run.  ``--dry-run`` prints the plan only.

Pure functions only (no classes); mmap + numpy structured node table, peak
RSS well under 1 GB for ~8M nodes.

Usage:
    remove_map_life_nx.py <file.nx> <mapid>:<npcid> [<mapid>:<npcid> ...]
                          [--tag remove-cindy] [--dry-run]
"""
from __future__ import annotations

import argparse
import mmap
import os
import shutil
import struct
import sys

import numpy as np

HEADER = struct.Struct('<IIQIQIQIQ')  # magic, node_count, node_offset, ...
NODE_DTYPE = np.dtype([('name', '<u4'), ('child', '<u4'), ('num', '<u2'),
                       ('typ', '<u2'), ('union', '8B')])
MAGIC = 0x34474B50  # "PKG4"
T_INT, T_REAL, T_STRING = 1, 2, 3
CHUNK_NODES = 1 << 20  # 节点表分块写盘，控制峰值内存


def fail(msg):
    sys.exit('remove_map_life_nx: ' + msg)


def parse_args(argv):
    p = argparse.ArgumentParser(
        description='Remove NPC life entries from Map.nx (node compaction).')
    p.add_argument('nxfile', help='path to Map.nx')
    p.add_argument('targets', nargs='+', metavar='mapid:npcid',
                   help='e.g. 100000000:9200000 (repeatable)')
    p.add_argument('--tag', default='remove-cindy',
                   help='backup suffix: <file>.pre-<tag>-backup')
    p.add_argument('--dry-run', action='store_true',
                   help='print the plan without writing anything')
    return p.parse_args(argv)


# ------------------------------------------------------------------ reading

def load_nx(path):
    """mmap 只读打开；返回包含 header 视图/节点表/字符串表的 dict。"""
    fh = open(path, 'rb')
    mm = mmap.mmap(fh.fileno(), 0, access=mmap.ACCESS_READ)
    if mm.size() < HEADER.size or struct.unpack_from('<I', mm, 0)[0] != MAGIC:
        fail('%s: not an NX archive' % path)
    hdr = HEADER.unpack_from(mm, 0)
    _, ncount, noff = hdr[0], hdr[1], hdr[2]
    if noff + 20 * ncount > mm.size():
        fail('%s: node table exceeds file size' % path)
    nodes = np.frombuffer(mm, dtype=NODE_DTYPE, count=ncount, offset=noff)
    strings = read_strings(mm, hdr[3], hdr[4])
    return {'path': path, 'mm': mm, 'fh': fh, 'header': hdr,
            'nodes': nodes, 'strings': strings}


def read_strings(mm, scount, soff):
    out = []
    for i in range(scount):
        doff = struct.unpack_from('<Q', mm, soff + 8 * i)[0]
        slen = struct.unpack_from('<H', mm, doff)[0]
        out.append(bytes(mm[doff + 2:doff + 2 + slen]).decode('utf-8',
                                                              'replace'))
    return out


def find_child(nodes, strings, parent, name):
    """在按名字排序的孩子区间内二分查找（nlnx 语义）。"""
    c0, num = int(nodes[parent]['child']), int(nodes[parent]['num'])
    lo, hi = 0, num
    while lo < hi:
        mid = (lo + hi) // 2
        if strings[nodes[c0 + mid]['name']] < name:
            lo = mid + 1
        else:
            hi = mid
    if lo < num and strings[nodes[c0 + lo]['name']] == name:
        return c0 + lo
    return None


def scalar_value(nodes, strings, idx):
    """解析 int/real/string 节点为 Python 值；其余类型返回 None。"""
    nd = nodes[idx]
    typ, raw = int(nd['typ']), bytes(nd['union'])
    if typ == T_INT:
        return struct.unpack('<q', raw)[0]
    if typ == T_REAL:
        return struct.unpack('<d', raw)[0]
    if typ == T_STRING:
        return strings[struct.unpack('<I', raw[:4])[0]]
    return None


# ------------------------------------------------------------------ locate

def parse_target(spec):
    mapid, sep, npcid = spec.partition(':')
    if not sep or not mapid.isdigit() or not npcid.isdigit():
        fail('bad target %r (want <mapid>:<npcid>, digits only)' % spec)
    return mapid, npcid


def entry_npc_id(nx, entry):
    """life 条目的 id 值（统一转字符串），无 id 子节点返回 None。"""
    nid = find_child(nx['nodes'], nx['strings'], entry, 'id')
    if nid is None:
        return None
    val = scalar_value(nx['nodes'], nx['strings'], nid)
    return None if val is None else str(val)


def locate_target(nx, mapid, npcid):
    """定位 mapid 的 life 下 id==npcid 的唯一条目；数量不为 1 即报错。"""
    nodes, strings = nx['nodes'], nx['strings']
    group = find_child(nodes, strings, find_child(nodes, strings, 0, 'Map'),
                       'Map' + mapid[0])
    if group is None:
        fail('%s: no Map%s group' % (mapid, mapid[0]))
    img = find_child(nodes, strings, group, mapid + '.img')
    if img is None:
        fail('%s: no %s.img' % (mapid, mapid))
    life = find_child(nodes, strings, img, 'life')
    if life is None:
        fail('%s: no life node' % mapid)
    c0, num = int(nodes[life]['child']), int(nodes[life]['num'])
    hits = [e for e in range(c0, c0 + num) if entry_npc_id(nx, e) == npcid]
    if len(hits) != 1:
        fail('%s: expected exactly 1 life entry with id %s, found %d'
             % (mapid, npcid, len(hits)))
    return {'mapid': mapid, 'npcid': npcid, 'img': img, 'life': life,
            'life_num': num, 'entry': hits[0],
            'entry_name': strings[nodes[hits[0]]['name']]}


# ------------------------------------------------------------- reachability

def _lists(nx):
    if 'child_list' not in nx:  # 缓存 Python 列表视图，加速 DFS 标量访问
        nx['child_list'] = nx['nodes']['child'].tolist()
        nx['num_list'] = nx['nodes']['num'].tolist()
    return nx['child_list'], nx['num_list']


def reachability(nx, skip):
    """从节点 0 迭代 DFS 标记；skip 内的节点不进入（即视为删除）。"""
    child, num = _lists(nx)
    mark = bytearray(len(child))
    mark[0] = 1
    stack = [0]
    while stack:
        i = stack.pop()
        for j in range(child[i], child[i] + num[i]):
            if j in skip or mark[j]:
                continue
            mark[j] = 1
            stack.append(j)
    return mark


def subtree_closure(nx, root):
    """root 子树的完整可达闭包（“仅该 entry 可达”语义）。"""
    child, num = _lists(nx)
    mark = bytearray(len(child))
    mark[root] = 1
    stack = [root]
    while stack:
        i = stack.pop()
        for j in range(child[i], child[i] + num[i]):
            if not mark[j]:
                mark[j] = 1
                stack.append(j)
    return mark


# ------------------------------------------------------------------- plan

def build_plan(nx, alive_mark):
    """稠密重编号 + 重写存活节点的 child/num 字段（全 numpy 向量化）。"""
    nodes = nx['nodes']
    count = len(nodes)
    alive_u8 = np.frombuffer(alive_mark, dtype=np.uint8)
    alive_idx = np.flatnonzero(alive_u8)
    remap = np.full(count, -1, dtype=np.int64)
    remap[alive_idx] = np.arange(alive_idx.size, dtype=np.int64)
    cum = np.zeros(count + 1, dtype=np.int64)
    np.cumsum((remap == -1).astype(np.int64), out=cum[1:])
    # num=0 的空节点 child 字段可能存有哨兵值（如 node_count），clamp 到
    # 界内便于统一索引；这些行随后被 ``new_num == 0`` 分支整体置零。
    child0 = np.minimum(nodes['child'].astype(np.int64), count - 1)
    num0 = nodes['num'].astype(np.int64)
    dead_in = cum[child0 + num0] - cum[child0]  # 孩子区间内被删孩子数
    new_num = num0 - dead_in
    new_child = remap[child0]
    hole = (new_child == -1) & (new_num > 0)
    if hole.any():
        # 首孩子被删（life 首条目情形）：取该位置之后第一个存活节点的新索引
        new_child[hole] = np.searchsorted(alive_idx, child0[hole],
                                          side='right')
    new_child[new_num == 0] = 0
    return {'alive_idx': alive_idx, 'remap': remap, 'new_child': new_child,
            'new_num': new_num, 'dead_in': dead_in,
            'new_count': int(alive_idx.size)}


def verify_plan(nx, plan, targets, alive_mark, base_mark):
    """写盘前断言：剪枝范围正确且新孩子区间保持连续。"""
    nodes = nx['nodes']
    alive_bool = np.frombuffer(alive_mark, dtype=np.uint8) != 0
    deleted_bool = ~alive_bool
    # 1) 丢失孩子的存活节点恰为各目标 life，且各恰 1 个
    touched = np.flatnonzero((plan['dead_in'] > 0) & alive_bool).tolist()
    want = sorted(t['life'] for t in targets)
    if touched != want:
        fail('internal: alive parents losing children %s, expected the %d '
             'target life nodes' % (touched[:16], len(want)))
    for t in targets:
        if int(plan['dead_in'][t['life']]) != 1:
            fail('internal: life of %s loses %d children (want 1)'
                 % (t['mapid'], int(plan['dead_in'][t['life']])))
    # 2) 被删节点 ⊆ 各目标完整子树闭包的并集 ∪ 原本就不可达的孤儿块
    covered = np.frombuffer(base_mark, dtype=np.uint8) == 0
    for t in targets:
        covered |= np.frombuffer(subtree_closure(nx, t['entry']),
                                 dtype=np.uint8) != 0
    stray = np.flatnonzero(deleted_bool & ~covered)
    if stray.size:
        fail('internal: %d deleted nodes outside target subtrees/orphans, '
             'e.g. %s' % (stray.size, stray[:8].tolist()))
    # 3) 存活父节点的新孩子区间必须稠密连续（首/末存活孩子重编号衔接）
    alive_idx, remap = plan['alive_idx'], plan['remap']
    count = len(nodes)
    # num=0 的空节点 child 字段可能存有哨兵值（如 node_count），clamp 到
    # 界内便于统一索引；这些行随后被 ``new_num == 0`` 分支整体置零。
    child0 = np.minimum(nodes['child'].astype(np.int64), count - 1)
    num0 = nodes['num'].astype(np.int64)
    sel = alive_bool & (plan['new_num'] > 0)
    c, n0, n1 = child0[sel], num0[sel], plan['new_num'][sel]
    first = np.where(alive_bool[c], c,
                     alive_idx[np.searchsorted(alive_idx, c, side='right')])
    last = alive_idx[np.searchsorted(alive_idx, c + n0, side='left') - 1]
    # 连续性以删减后的 new_num 为准（区间可能含被剔孩子/孤儿节点）
    ok = ((remap[first] == plan['new_child'][sel])
          & (remap[last] == remap[first] + n1 - 1)
          & (remap[first] + n1 <= plan['new_count']))
    if not ok.all():
        bad = np.flatnonzero(sel)[~ok][:8].tolist()
        fail('internal: non-contiguous rewritten children for %d nodes, '
             'e.g. %s' % (int((~ok).sum()), bad))


# ----------------------------------------------------------------- report

def report(nx, plan, targets, base_mark, alive_mark, dry_run):
    old_count = len(nx['nodes'])
    new_count = plan['new_count']
    deleted = old_count - new_count
    base_dead = np.frombuffer(base_mark, dtype=np.uint8) == 0
    deleted_bool = np.frombuffer(alive_mark, dtype=np.uint8) == 0
    per = []
    union = np.zeros(old_count, dtype=bool)
    for t in targets:
        cl = np.frombuffer(subtree_closure(nx, t['entry']),
                           dtype=np.uint8) != 0
        union |= cl
        per.append((t, int(cl.sum())))
    pruned = int((deleted_bool & base_dead).sum())
    removed_live = deleted - pruned
    print('targets: %d map(s), npc ids %s'
          % (len(targets), ','.join(sorted({t['npcid'] for t in targets}))))
    for t, size in per:
        print('  %s: life entry %r -> delete %d nodes, life num %d -> %d'
              % (t['mapid'], t['entry_name'], size, t['life_num'],
                 t['life_num'] - 1))
    print('deleted nodes: %d (target subtrees %d + pre-existing orphans %d)'
          % (deleted, removed_live, pruned))
    print('node_count: %d -> %d (node table tail %d bytes zeroed)'
          % (old_count, new_count, 20 * deleted))
    if dry_run:
        print('dry-run: nothing written (no backup, no tmp file)')


# ------------------------------------------------------------------ write

def write_compacted(nx, plan, tag):
    """备份 -> 临时副本整文件拷贝 -> 原地改 header/节点表 -> 原子替换。"""
    path = nx['path']
    old_count, node_off = nx['header'][1], nx['header'][2]
    bak = '%s.pre-%s-backup' % (path, tag)
    if os.path.exists(bak):
        fail('backup already exists, refusing to run: %s' % bak)
    tmp = '%s.tmp-remove-life' % path
    if os.path.exists(tmp):
        os.unlink(tmp)
    new_nodes = nx['nodes'][plan['alive_idx']]  # 花式索引即拷贝，可写字段
    new_nodes['child'] = plan['new_child'][plan['alive_idx']].astype('<u4')
    new_nodes['num'] = plan['new_num'][plan['alive_idx']].astype('<u2')
    shutil.copyfile(path, bak)
    shutil.copyfile(path, tmp)
    with open(tmp, 'r+b') as fh:
        fh.seek(4)  # header: 仅改 node_count，其余字段与文件大小不动
        fh.write(struct.pack('<I', plan['new_count']))
        fh.seek(node_off)
        for s in range(0, plan['new_count'], CHUNK_NODES):
            fh.write(new_nodes[s:s + CHUNK_NODES].tobytes())
        fh.seek(node_off + 20 * plan['new_count'])
        fh.write(b'\0' * (20 * (old_count - plan['new_count'])))
        fh.flush()
        os.fsync(fh.fileno())
    os.replace(tmp, path)
    print('backup: %s' % bak)
    print('wrote %s: node_count %d -> %d, size unchanged (%d bytes)'
          % (path, old_count, plan['new_count'], os.path.getsize(path)))


# ------------------------------------------------------------------- main

def main(argv=None):
    args = parse_args(argv if argv is not None else sys.argv[1:])
    pairs = [parse_target(s) for s in args.targets]
    seen = set()
    for mapid, _ in pairs:
        if mapid in seen:
            fail('duplicate mapid %s' % mapid)
        seen.add(mapid)
    nx = load_nx(args.nxfile)
    targets = [locate_target(nx, m, n) for m, n in pairs]
    base_mark = reachability(nx, frozenset())   # 不剔除任何节点的基线闭包
    alive_mark = reachability(nx, {t['entry'] for t in targets})
    plan = build_plan(nx, alive_mark)
    verify_plan(nx, plan, targets, alive_mark, base_mark)
    report(nx, plan, targets, base_mark, alive_mark, args.dry_run)
    if not args.dry_run:
        write_compacted(nx, plan, args.tag)
    return 0


if __name__ == '__main__':
    sys.exit(main())
