#!/usr/bin/env python3
"""Make Final Attack hit 2 mobs-8 on every level, on both data sides.

Targets (see FINAL_ATTACKS): 1100002 (Sword FA), 1200003 (BW FA),
1300002 (Spear FA).  Only level 30 ships ``attackCount=2``/``mobCount=8``;
levels 1-29 lack both keys, so the client defaults to 1 hit / 1 mob
(``SkillData.cpp``) and the server does the same
(``MapleStatEffect.getInt``), which silently caps early Final Attack.

* ``assets/Skill.nx`` -- splice the two int nodes into every level child
  block that misses them (nlnx resolves children with a bytewise binary
  search, so blocks stay sorted and contiguous), enforce stray values to
  2/8, bump the level counts and rebuild the archive.  A one-shot
  ``.pre-fa-counts-backup`` copy is kept.
* ``link_repos/MapleStory-Server/wz/Skill.wz/{110,120,130}.img.xml`` --
  string surgery on the minified XML: insert
  ``<int name="attackCount" value="2"/><int name="mobCount" value="8"/>``
  right before each level's ``</imgdir>`` (or rewrite an existing tag);
  every other byte is untouched.  Level 30 must already read 2/8.

Idempotent: levels already carrying 2/8 are no-ops, reruns write nothing.
Both sides are re-read and asserted after writing.

Usage:
    patch_final_attack_counts_nx.py [--repo-root <repo>] [--dry-run]
"""
from __future__ import annotations

import argparse
import bisect
import os
import re
import shutil
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
for _p in (REPO / 'tools', REPO / 'tools/nx'):
    sys.path.insert(0, str(_p))

from inject_npc_nx import with_tail_pad  # noqa: E402
from inject_skill_nx import T_INT  # noqa: E402
from map079_tree import child_indices, find_path, node_name  # noqa: E402
from merge_skill_nx import build, intern, parse_nx  # noqa: E402

FINAL_ATTACKS = ((110, 1100002), (120, 1200003), (130, 1300002))
ATTACK_COUNT, MOB_COUNT = b'attackCount', b'mobCount'
PROPS = ((ATTACK_COUNT, 2), (MOB_COUNT, 8))
LEVELS = 30
BACKUP_SUFFIX = '.pre-fa-counts-backup'
INT_TAG = '<int name="%s" value="%d"/>'
IMGDIR = re.compile(r'<imgdir\b[^>]*>|</imgdir>')
NAME = re.compile(r'name="([^"]*)"')
PROP_INT = {}
INT_MATCH = r'<int\s+name="%s"\s+value="(-?\d+)"\s*/>'


def prop_int(prop):
    """Compiled <int name="prop" value="N"/> matcher, cached per prop name."""
    key = prop.decode()
    pattern = PROP_INT.get(key)
    if pattern is None:
        pattern = PROP_INT[key] = re.compile(INT_MATCH % key)
    return pattern


def fail(msg):
    sys.exit('patch_final_attack_counts: ' + msg)


def backup_once(path):
    """Keep one pre-run copy; never overwrite an existing backup."""
    bak = Path(str(path) + BACKUP_SUFFIX)
    if bak.exists():
        return bak, False
    shutil.copy2(path, bak)
    return bak, True


# ------------------------------------------------------------------- NX side

def skill_path(img, sid):
    return (('%d.img' % img).encode(), b'skill', str(sid).encode())


def level_indices(nx, img, sid):
    """Node indices of the skill's 30 level children, validated 1..30."""
    idx = find_path(nx, skill_path(img, sid) + (b'level',))
    if idx is None:
        fail('%d.img/skill/%d/level not found' % (img, sid))
    out = list(child_indices(nx, idx))
    names = [node_name(nx, i) for i in out]
    want = {str(n).encode() for n in range(1, LEVELS + 1)}
    if set(names) != want or names != sorted(names):
        fail('%d: level children are not the sorted set 1..%d' % (sid, LEVELS))
    return out


def level_children(nx, lv):
    """Stored (sorted) order [(name, type, union)] of a level's children."""
    _, base, num, _, _ = nx['nodes'][lv]
    return [(node_name(nx, base + k),) + tuple(nx['nodes'][base + k][3:])
            for k in range(num)]


def plan_level(nx, lv, props=PROPS):
    """Per-prop plan: ('ok', None) | ('set', None) | ('insert', point).

    ``props`` defaults to the fixed ``attackCount``/``mobCount`` pair; callers
    that need an extra per-level int (the ranged Final Attack ``bulletCount``)
    pass their own (prop, value) sequence.
    """
    kids = level_children(nx, lv)
    names = [k[0] for k in kids]
    if names != sorted(names):
        fail('level node %d children are not bytewise sorted' % lv)
    _, base, _, _, _ = nx['nodes'][lv]
    plan = []
    for prop, value in props:
        current = next((k for k in kids if k[0] == prop), None)
        if current is None:
            plan.append(('insert', base + bisect.bisect_left(names, prop)))
        elif current[1] != T_INT:
            fail('level node %d: %s is not an int' % (lv, prop.decode()))
        elif struct.unpack('<q', current[2])[0] == value:
            plan.append(('ok', None))
        else:
            plan.append(('set', None))
    return plan


def set_prop(nx, lv, prop, value):
    """Rewrite an existing prop int in place to value."""
    for c in child_indices(nx, lv):
        if node_name(nx, c) == prop:
            nm, ch, num, typ, union = nx['nodes'][c]
            if typ != T_INT:
                fail('level node %d: %s is not an int' % (lv, prop.decode()))
            nx['nodes'][c] = (nm, ch, num, T_INT, struct.pack('<q', value))
            return
    fail('level node %d has no %s to rewrite' % (lv, prop.decode()))


def check_insert_points(nx, points, parents):
    """No foreign children block may straddle or start at a point."""
    nodes = nx['nodes']
    parent_set = set(parents)
    starts = {}
    for i, (_, ch, num, _, _) in enumerate(nodes):
        if num:
            starts.setdefault(ch, []).append(i)
    for at in points:
        for i in starts.get(at, ()):
            if i not in parent_set:
                fail('node %d block starts at insertion point %d' % (i, at))
    for ch, owners in starts.items():
        if any(i in parent_set for i in owners):
            continue
        num = nodes[owners[0]][2]
        lo = bisect.bisect_right(points, ch)
        hi = bisect.bisect_left(points, ch + num)
        if hi > lo:
            fail('node %d block [%d,%d) straddles an insertion point'
                 % (owners[0], ch, ch + num))


def shift_base(base, points, counts=None):
    """Children remap: +1 per leaf inserted strictly below base.

    ``counts`` holds the number of leaves emitted at each point (all 1 when a
    point carries a single insert).  A point equal to ``base`` is the
    front-insert case: the new leaves take the old first slots, so the parent
    keeps its pointer value.
    """
    k = bisect.bisect_left(points, base)
    return base + (k if counts is None else sum(counts[:k]))


def splice_props(nx, inserts):
    """Insert (level, name, value, point) nodes; returns (points, counts).

    The prop names are interned from the inserts themselves, so callers may
    splice any per-level int (``bulletCount`` included) without touching the
    fixed ``PROPS`` pair.  Several ints may share one insertion point -- a
    level that misses ``attackCount`` and ``bulletCount`` inserts both into
    the same slot -- so each point carries a name-sorted run of leaves and the
    parent keeps pointing at the run's first leaf.
    """
    strings = nx['strings']
    index = {s: i for i, s in enumerate(strings)}
    runs = {}
    for lv, name, value, at in inserts:
        if at <= lv:
            fail('insertion point %d is not after level node %d' % (at, lv))
        runs.setdefault(at, []).append((name, struct.pack('<q', value)))
    points = sorted(runs)
    check_insert_points(nx, points, [lv for lv, _, _, _ in inserts])
    by_point = {at: sorted(((intern(strings, index, name), union)
                            for name, union in leaves),
                           key=lambda leaf: strings[leaf[0]])
                for at, leaves in runs.items()}
    counts = [len(by_point[at]) for at in points]
    out, pi = [], 0
    for i, node in enumerate(nx['nodes']):
        if pi < len(points) and points[pi] == i:
            out.extend((name_id, 0, 0, T_INT, union)
                       for name_id, union in by_point[i])
            pi += 1
        out.append((node[0], shift_base(node[1], points, counts))
                   + tuple(node[2:]))
    nx['nodes'] = out
    return points, counts


def bump_level_counts(nx, points, per_level, counts=None):
    """Each level's child count += its inserts, remapped by shift_base."""
    for lv, count in per_level.items():
        i = shift_base(lv, points, counts)
        nm, ch, num, typ, union = nx['nodes'][i]
        nx['nodes'][i] = (nm, ch, num + count, typ, union)


def verify_skill(check, img, sid, snaps):
    """Post-build comparison of every level against the pre-change state."""
    want_all = {name: (T_INT, struct.pack('<q', value))
                for name, value in PROPS}
    for lv, before in zip(level_indices(check, img, sid), snaps):
        after = level_children(check, lv)
        names = [k[0] for k in after]
        if names != sorted(names):
            fail('%d: level children unsorted after rebuild' % sid)
        before_map = {k[0]: (k[1], k[2]) for k in before}
        for name, want in want_all.items():
            got = dict((k[0], (k[1], k[2])) for k in after).get(name)
            if got != want:
                fail('%d: %s drift %r -> %r' % (sid, name.decode(), got, want))
            if name in before_map and before_map[name] != want:
                fail('%d: %s already-correct value was rewritten' % (sid, name))
        for k in after:
            if k[0] not in want_all and before_map.get(k[0]) != (k[1], k[2]):
                fail('%d: level children drifted after rebuild' % sid)


def patch_nx(skill_nx, dry):
    """Rewrite every missing/wrong attackCount/mobCount on levels 1..30."""
    nx = parse_nx(skill_nx)
    report, snaps, inserts, sets, per_level = [], {}, [], [], {}
    for img, sid in FINAL_ATTACKS:
        lvs = level_indices(nx, img, sid)
        snaps[(img, sid)] = [level_children(nx, lv) for lv in lvs]
        ok = fixed = inserted = 0
        for lv in lvs:
            count_here = 0
            for (act, at), (prop, value) in zip(plan_level(nx, lv), PROPS):
                if act == 'insert':
                    inserts.append((lv, prop, value, at))
                    count_here += 1
                elif act == 'set':
                    sets.append((lv, prop, value))
                    fixed += 1
                else:
                    ok += 1
            if count_here:
                per_level[lv] = per_level.get(lv, 0) + count_here
                inserted += count_here
        report.append((img, sid, ok, fixed, inserted))
    for img, sid, ok, fixed, ins in report:
        print('  NX %d.img/skill/%d: ok %d, set %d, insert %d'
              % (img, sid, ok, fixed, ins))
    if not inserts and not sets:
        print('NX side: all levels already 2/8; no-op')
        return report
    if dry:
        print('NX side: dry-run, nothing written')
        return report
    backup_once(skill_nx)
    for lv, prop, value in sets:
        set_prop(nx, lv, prop, value)
    points, counts = [], []
    if inserts:
        points, counts = splice_props(nx, inserts)
    bump_level_counts(nx, points, per_level, counts)
    blob = with_tail_pad(build(nx), nx)
    tmp = Path(str(skill_nx) + '.tmp')
    tmp.write_bytes(blob)
    try:
        check = parse_nx(tmp)
        for img, sid in FINAL_ATTACKS:
            verify_skill(check, img, sid, snaps[(img, sid)])
    except SystemExit:
        tmp.unlink()
        raise
    os.replace(tmp, skill_nx)
    print('NX side: wrote %s (%d bytes, %d nodes)'
          % (skill_nx, len(blob), len(nx['nodes'])))
    return report


# ------------------------------------------------------------------ XML side

def imgdir_span(text, open_off):
    """(inner_start, close_off) of the imgdir opened at open_off."""
    depth, inner = 0, None
    for m in IMGDIR.finditer(text, open_off):
        closing = m.group(0).startswith('</')
        if inner is None:
            if closing:
                fail('imgdir tag expected at offset %d' % open_off)
            inner = m.end()
            depth = 1
            continue
        depth += -1 if closing else 1
        if depth == 0:
            return inner, m.start()
    fail('unterminated imgdir at offset %d' % open_off)


def level_spans(text, sid):
    """[(name, inner, close)] for the 1..30 level imgdirs of skill sid."""
    open_m = re.search(r'<imgdir\b[^>]*name="%d">' % sid, text)
    if open_m is None:
        fail('%d: no skill imgdir in the XML' % sid)
    s_inner, s_close = imgdir_span(text, open_m.start())
    lvl_open = text.find('<imgdir name="level">', s_inner, s_close)
    if lvl_open < 0:
        fail('%d: no level imgdir inside the skill' % sid)
    l_inner, l_close = imgdir_span(text, lvl_open)
    out, cur, depth = [], None, 0
    for m in IMGDIR.finditer(text, l_inner, l_close):
        if m.group(0).startswith('</'):
            depth -= 1
            if depth == 0 and cur is not None:
                out.append(cur + (m.start(),))
                cur = None
        else:
            if depth == 0:
                found = NAME.search(m.group(0))
                cur = (found.group(1) if found else '', m.end())
            depth += 1
    if [n for n, _, _ in out] != [str(n) for n in range(1, LEVELS + 1)]:
        fail('%d: level imgdirs are not 1..%d' % (sid, LEVELS))
    return out


def xml_edits(text, sid, props=PROPS):
    """(edits, (inserted, fixed, ok)); edits are (start, end, replacement).

    ``props`` defaults to the fixed ``attackCount``/``mobCount`` pair; callers
    may pass extra per-level ints (the archer/Cygnus FA ``bulletCount``) and
    the counters in ``stats`` then span every prop of every level.
    """
    edits, stats = [], [0, 0, 0]
    for _, inner, close in level_spans(text, sid):
        tags = []
        for prop, value in props:
            found = prop_int(prop).search(text, inner, close)
            if found is None:
                tags.append(INT_TAG % (prop.decode(), value))
                stats[0] += 1
            elif int(found.group(1)) != value:
                edits.append((found.start(), found.end(),
                              INT_TAG % (prop.decode(), value)))
                stats[1] += 1
            else:
                stats[2] += 1
        if tags:
            edits.append((close, close, ''.join(tags)))
    return edits, tuple(stats)


def apply_edits(text, edits):
    """Apply non-overlapping (start, end, repl) edits back-to-front."""
    out, last = text, len(text) + 1
    for start, end, repl in sorted(edits, reverse=True):
        if start < 0 or end < start or end > last:
            fail('bad or overlapping XML edit at %d' % start)
        out = out[:start] + repl + out[end:]
        last = start
    return out


def patch_xml(server_wz, dry):
    """Insert/fix the two int tags on every level of the three XMLs."""
    for img, sid in FINAL_ATTACKS:
        path = Path(server_wz) / ('%d.img.xml' % img)
        if not path.exists():
            fail('missing %s' % path)
        text = path.read_text(encoding='utf-8')
        edits, (ins, fixed, ok) = xml_edits(text, sid)
        print('  XML %d.img.xml/%d: ok %d, set %d, insert %d'
              % (img, sid, ok, fixed, ins * 2 if ins else 0))
        if not edits:
            print('XML side %d: all levels already 2/8; no-op' % sid)
            continue
        if dry:
            print('XML side %d: dry-run, nothing written' % sid)
            continue
        backup_once(path)
        path.write_text(apply_edits(text, edits), encoding='utf-8')
        reread, (r_ins, r_fixed, r_ok) = xml_edits(
            path.read_text(encoding='utf-8'), sid)
        if reread or r_ins or r_fixed:
            fail('%d: XML verify failed (%d pending edits)' % (sid, len(reread)))
        print('XML side %d: %d levels verified 2/8' % (sid, r_ok // 2))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--repo-root', default=str(REPO))
    ap.add_argument('--dry-run', action='store_true')
    args = ap.parse_args()
    root = Path(args.repo_root)
    skill_nx = root / 'assets' / 'Skill.nx'
    server_wz = root / 'link_repos' / 'MapleStory-Server' / 'wz' / 'Skill.wz'
    print('Final Attack 2-hits/8-mobs patch (levels 1..%d)' % LEVELS)
    patch_nx(skill_nx, args.dry_run)
    patch_xml(server_wz, args.dry_run)
    print('done')


if __name__ == '__main__':
    main()
