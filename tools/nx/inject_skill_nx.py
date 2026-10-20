#!/usr/bin/env python3
"""Graft the three 079-missing Berserker finishers into ``assets/Skill.nx``.

The 079 client ships ``111.img/skill`` without 1111003 (Panic:Sword),
1111004 (Panic:Axe) and 1111006 (Coma:Axe).  The v83 archive keeps their
complete subtrees (level 1..30, finish, hit, req, icons), so this tool copies
those subtrees out of ``assets/Skill.nx.v83-backup`` and inserts them under
the existing ``skill`` node:

* 1111003/1111004 -- level scalars are normalised to ``attackCount=6`` and
  ``mobCount=1`` for every level; every other key (damage/prop/time/mpCon/
  hs/req) keeps its v83 value.  The backup's 32 (Sword) and missing (Axe)
  attack counts are dirty data.
* 1111006 -- deep copy of the shipped 1111005 (Sword Coma) subtree, so both
  Coma variants share one set of level values and the same bitmap blobs.

This is a subtree-level graft: the ``111.img`` wrapper is never replaced, so
the 079-only skills 1111002/1111005/1111008 keep their exact subtrees.  New
nodes land at their sorted slot (nlnx resolves children with a bytewise
binary search) and the file tail is padded for the reader's fixed 4*w*h
bitmap window (see ``inject_npc_nx.py``).  The rebuilt archive is parsed back
and checked against the source values before it replaces ``Skill.nx``.

One-shot: a ``Skill.nx.pre-combo-backup`` copy is written first and reruns
are refused while it exists (restore it to start over).

Pure functions only; stdlib plus the repo's own NX helpers.

Usage:
    inject_skill_nx.py [--assets-dir assets] [--backup-nx PATH]
"""
from __future__ import annotations

import argparse
import os
import shutil
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
for _p in (REPO / 'tools', REPO / 'tools/nx'):
    sys.path.insert(0, str(_p))

from inject_npc_nx import flatten_children, sorted_slot, splice, with_tail_pad  # noqa: E402
from map079_tree import find_path, node_name, subtree_digest  # noqa: E402
from merge_skill_nx import build, intern, parse_nx  # noqa: E402

T_NONE, T_INT, T_STRING, T_VECTOR, T_BITMAP, T_AUDIO = 0, 1, 3, 4, 5, 6
IMG = b'111.img'
SKILL = b'skill'
PANIC = (1111003, 1111004)
COMA, COMA_SOURCE = 1111006, 1111005
KEPT = (1111002, 1111005, 1111008)
LEVELS = 30
NORMALISE = {'attackCount': 6, 'mobCount': 1}


def fail(msg):
    sys.exit('inject_skill_nx: ' + msg)


def backup(path):
    """One-shot guard: refuse to rerun while the pre-graft backup exists."""
    bak = path.with_name('%s.pre-combo-backup' % path.name)
    if bak.exists():
        fail('backup already exists, refusing to rerun: %s\n'
             '  (restore it over %s to start over)' % (bak, path.name))
    shutil.copy2(path, bak)
    return bak


def bump_num(nodes, idx, delta):
    name, ch, num, typ, union = nodes[idx]
    nodes[idx] = (name, ch, num + delta, typ, union)


def skill_path(*names):
    return (IMG, SKILL) + tuple(str(n).encode() if isinstance(n, int) else n
                                for n in names)


def require(nx, path, what):
    idx = find_path(nx, path)
    if idx is None:
        fail('%s not found: %s' % (what, b'/'.join(path).decode()))
    return idx


# ------------------------------------------------------------------ cloning

def source_ctx(dst, share_blobs):
    """Interning/mapping tables for one graft.

    Blob mapping is memoised per source index, so repeated frames reuse one
    blob.  Copying inside the same archive (1111006 from 1111005) reuses the
    existing blobs outright instead of duplicating ~2 MiB of sprites.
    """
    return {
        'strings': dst['strings'],
        'index': {s: i for i, s in enumerate(dst['strings'])},
        'bitmaps': dst['bitmaps'],
        'audios': dst['audios'],
        'bmp_memo': ({i: i for i in range(len(dst['bitmaps']))}
                     if share_blobs else {}),
        'aud_memo': ({i: i for i in range(len(dst['audios']))}
                     if share_blobs else {}),
    }


def remap_blob(src_table, dst_table, memo, idx):
    if idx not in memo:
        memo[idx] = len(dst_table)
        dst_table.append(src_table[idx])
    return memo[idx]


def describe(src, idx, ctx):
    """Clone src's subtree at idx into (name, type, union, kids) descriptors."""
    sid, base, num, typ, union = src['nodes'][idx]
    kids = tuple(describe(src, base + k, ctx) for k in range(num))
    name = intern(ctx['strings'], ctx['index'], src['strings'][sid])
    if typ == T_STRING:
        value = src['strings'][struct.unpack_from('<I', union, 0)[0]]
        union = struct.pack('<II', intern(ctx['strings'], ctx['index'], value), 0)
    elif typ == T_BITMAP:
        bmp, w, h = struct.unpack('<IHH', union)
        union = struct.pack('<IHH', remap_blob(src['bitmaps'], ctx['bitmaps'],
                                               ctx['bmp_memo'], bmp), w, h)
    elif typ == T_AUDIO:
        aud, length = struct.unpack('<II', union)
        union = struct.pack('<II', remap_blob(src['audios'], ctx['audios'],
                                              ctx['aud_memo'], aud), length)
    return (name, typ, union, kids)


def scalar(ctx, key, value):
    return (intern(ctx['strings'], ctx['index'], key), T_INT,
            struct.pack('<q', value), ())


def rename(desc, name, ctx):
    """Point a cloned descriptor at a new (interned) node name."""
    sid = intern(ctx['strings'], ctx['index'], name)
    return (sid,) + desc[1:]


def override_kids(kids, overrides, ctx):
    """Replace/insert int children, keeping the block sorted by name."""
    wanted = {k.encode(): v for k, v in overrides.items()}
    kept = tuple(k for k in kids if ctx['strings'][k[0]] not in wanted)
    added = tuple(scalar(ctx, k, v) for k, v in wanted.items())
    return tuple(sorted(kept + added, key=lambda k: ctx['strings'][k[0]]))


def normalise_levels(desc, overrides, ctx):
    """Apply per-level int overrides to the ``level`` branch of a skill."""
    name, typ, union, kids = desc
    out = []
    for kid in kids:
        if ctx['strings'][kid[0]] != b'level':
            out.append(kid)
            continue
        levels = tuple((lv[0], lv[1], lv[2],
                         override_kids(lv[3], overrides, ctx)) for lv in kid[3])
        out.append((kid[0], kid[1], kid[2], levels))
    return (name, typ, union, tuple(out))


# ------------------------------------------------------------- table surgery

def splice_guard(nodes, at, parent_idx):
    """Only the target block may straddle `at`; shared blocks would corrupt."""
    for i, (_, ch, num, _, _) in enumerate(nodes):
        if i != parent_idx and num and ch < at < ch + num:
            fail('node %d children [%d,%d) straddle the insertion point %d'
                 % (i, ch, ch + num, at))


def graft(nx, skill_idx, desc):
    """Insert one skill subtree at its sorted slot under the skill node."""
    subtree = []
    first, count = flatten_children(desc, subtree)
    at = sorted_slot(nx, skill_idx, nx['strings'][desc[0]])
    if at <= skill_idx:
        fail('refusing to splice inside the skill node itself')
    splice_guard(nx['nodes'], at, skill_idx)
    base = len(nx['nodes']) + 1
    insert = (desc[0], base + first, count, desc[1], desc[2])
    nodes = splice(nx['nodes'], at, insert, subtree)
    bump_num(nodes, skill_idx, 1)
    nx['nodes'] = nodes
    return len(subtree) + 1


# ----------------------------------------------------------------- checking

def int_child(nx, idx, key):
    base, num = nx['nodes'][idx][1], nx['nodes'][idx][2]
    for c in range(base, base + num):
        if node_name(nx, c) == key:
            typ, union = nx['nodes'][c][3], nx['nodes'][c][4]
            if typ != T_INT:
                fail('%s is not an int' % key.decode())
            return struct.unpack('<q', union)[0]
    return None


def scalars(nx, idx):
    """Name -> value for the int/string children of a node."""
    base, num = nx['nodes'][idx][1], nx['nodes'][idx][2]
    out = {}
    for c in range(base, base + num):
        typ, union = nx['nodes'][c][3], nx['nodes'][c][4]
        if typ == T_INT:
            out[node_name(nx, c)] = struct.unpack('<q', union)[0]
        elif typ == T_STRING:
            out[node_name(nx, c)] = nx['strings'][struct.unpack_from('<I', union, 0)[0]]
    return out


def check_normalised(nx, skill_id):
    """Every level must carry the two normalised Panic scalars."""
    level = require(nx, skill_path(skill_id, b'level'), 'level')
    base, num = nx['nodes'][level][1], nx['nodes'][level][2]
    if num != LEVELS:
        fail('%d has %d levels, expected %d' % (skill_id, num, LEVELS))
    for c in range(base, base + num):
        for key, value in NORMALISE.items():
            got = int_child(nx, c, key.encode())
            if got != value:
                fail('%d level %s: %s=%r, expected %r'
                     % (skill_id, node_name(nx, c).decode(), key, got, value))


def check_backup_values(nx, src, skill_id):
    """All non-normalised level scalars must still match the v83 backup."""
    for lv in range(1, LEVELS + 1):
        path = skill_path(skill_id, b'level', lv)
        want = scalars(src, require(src, path, 'backup level'))
        got = scalars(nx, require(nx, path, 'graft level'))
        for key, value in want.items():
            if key.decode() in NORMALISE:
                continue
            if got.get(key) != value:
                fail('%d level %d: %s=%r, backup has %r'
                     % (skill_id, lv, key.decode(), got.get(key), value))


def check(nx, src, kept_before):
    """Validate the rebuilt archive against the backup and the 079 subtrees."""
    skill = require(nx, skill_path(), 'skill')
    base, num = nx['nodes'][skill][1], nx['nodes'][skill][2]
    names = [node_name(nx, c) for c in range(base, base + num)]
    if names != sorted(names):
        fail('skill children are not name-sorted: %s' % names)
    for skill_id in PANIC + (COMA,):
        if str(skill_id).encode() not in names:
            fail('missing grafted node %d' % skill_id)
    for skill_id in PANIC:
        check_normalised(nx, skill_id)
        check_backup_values(nx, src, skill_id)
    coma = subtree_digest(nx, require(nx, skill_path(COMA, b'level'), 'level'))
    source = subtree_digest(nx, require(nx, skill_path(COMA_SOURCE, b'level'), 'level'))
    if coma != source:
        fail('1111006 level data diverges from 1111005: %r != %r' % (coma, source))
    for skill_id in KEPT:
        now = subtree_digest(nx, require(nx, skill_path(skill_id), 'kept'))
        if now != kept_before[skill_id]:
            fail('%d subtree changed during the graft' % skill_id)


# --------------------------------------------------------------------- main

def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--assets-dir', type=Path, default=REPO / 'assets')
    ap.add_argument('--backup-nx', type=Path, default=None)
    args = ap.parse_args(argv)

    skill_nx = Path(args.assets_dir) / 'Skill.nx'
    backup_nx = args.backup_nx or Path(args.assets_dir) / 'Skill.nx.v83-backup'
    if not skill_nx.exists() or not backup_nx.exists():
        fail('missing %s or %s' % (skill_nx, backup_nx))

    nx = parse_nx(skill_nx)
    src = parse_nx(backup_nx)
    skill_idx = require(nx, skill_path(), 'skill')
    for skill_id in PANIC + (COMA,):
        if find_path(nx, skill_path(skill_id)) is not None:
            fail('Skill.nx already contains %d; restore the backup first' % skill_id)
    for skill_id in PANIC:
        require(src, skill_path(skill_id), 'backup skill')

    old_size = skill_nx.stat().st_size
    old_counts = (len(nx['nodes']), len(nx['strings']), len(nx['bitmaps']))
    kept_before = {s: subtree_digest(nx, require(nx, skill_path(s), 'kept'))
                   for s in KEPT}

    bak = backup(skill_nx)
    ctx = source_ctx(nx, share_blobs=False)
    for skill_id in PANIC:
        desc = describe(src, require(src, skill_path(skill_id), 'backup'), ctx)
        desc = normalise_levels(desc, NORMALISE, ctx)
        added = graft(nx, skill_idx, desc)
        print('  grafted %d: %d nodes' % (skill_id, added))

    ctx = source_ctx(nx, share_blobs=True)
    coma_src = require(nx, skill_path(COMA_SOURCE), '1111005')
    desc = rename(describe(nx, coma_src, ctx), str(COMA).encode(), ctx)
    added = graft(nx, skill_idx, desc)
    print('  grafted %d: %d nodes (copy of %d)' % (COMA, added, COMA_SOURCE))

    blob = with_tail_pad(build(nx), nx)
    tmp = skill_nx.with_suffix('.nx.tmp')
    tmp.write_bytes(blob)
    try:
        check(parse_nx(tmp), src, kept_before)
    except SystemExit:
        tmp.unlink()
        raise
    os.replace(tmp, skill_nx)

    print('wrote %s: %d -> %d bytes (+%d), backup %s'
          % (skill_nx, old_size, len(blob), len(blob) - old_size, bak.name))
    print('  nodes %d -> %d, strings %d -> %d, bitmaps %d -> %d'
          % (old_counts[0], len(nx['nodes']), old_counts[1], len(nx['strings']),
             old_counts[2], len(nx['bitmaps'])))
    print('  verified: attackCount=%d mobCount=%d for 1111003/1111004, '
          '1111006 == 1111005' % (NORMALISE['attackCount'], NORMALISE['mobCount']))


if __name__ == '__main__':
    main()
