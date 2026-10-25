#!/usr/bin/env python3
"""Give the three v83 ultimate skills a 15s cooldown on both data sides.

Targets (see ULTIMATES): Meteor Shower 2121007, Blizzard 2221007 and
Genesis 2321008.  Their original 30-level growth data is preserved; the
only change is a ``cooltime = 15`` property on every ``level`` child:

* ``assets/Skill.nx`` -- splice one int node into each level's child block
  at its bytewise slot (nlnx resolves children with a binary search, so the
  block must stay sorted and contiguous), bump the level counts and rebuild
  the archive.  A one-shot ``.pre-ultimate-cd-backup`` copy is kept.
* ``link_repos/MapleStory-Server/wz/Skill.wz/<job>.img.xml`` -- string
  surgery on the minified XML: insert ``<int name="cooltime" value="15"/>``
  right before each level's ``</imgdir>``; every other byte is untouched.

An existing cooltime is enforced to 15, so the tool is idempotent: reruns
write nothing and leave both sides unchanged.

Usage:
    patch_ultimate_cooldown_nx.py [--assets-dir assets]
        [--server-wz link_repos/MapleStory-Server/wz/Skill.wz] [--dry-run]
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
from wide_attack_common import read_text, write_text  # noqa: E402

ULTIMATES = ((212, 2121007), (222, 2221007), (232, 2321008))
COOLDOWN = 15
LEVELS = 30
COOLTIME = b'cooltime'
BACKUP_SUFFIX = '.pre-ultimate-cd-backup'
INT_TAG = '<int name="cooltime" value="%d"/>'
IMGDIR = re.compile(r'<imgdir\b[^>]*>|</imgdir>')
NAME = re.compile(r'name="([^"]*)"')
COOLTIME_INT = re.compile(r'<int\s+name="cooltime"\s+value="(-?\d+)"\s*/>')


def fail(msg):
    sys.exit('patch_ultimate_cooldown: ' + msg)


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


def plan_level(nx, lv):
    """('ok', None) | ('set', None) | ('insert', node index) for a level."""
    kids = level_children(nx, lv)
    names = [k[0] for k in kids]
    if names != sorted(names):
        fail('level node %d children are not bytewise sorted' % lv)
    for name, typ, union in kids:
        if name == COOLTIME:
            if typ != T_INT:
                fail('level node %d: cooltime is not an int' % lv)
            return ('ok' if struct.unpack('<q', union)[0] == COOLDOWN
                    else 'set'), None
    _, base, _, _, _ = nx['nodes'][lv]
    return 'insert', base + bisect.bisect_left(names, COOLTIME)


def set_cooltime(nx, lv):
    """Rewrite an existing cooltime int in place to COOLDOWN."""
    for c in child_indices(nx, lv):
        if node_name(nx, c) == COOLTIME:
            nm, ch, num, typ, union = nx['nodes'][c]
            if typ != T_INT:
                fail('level node %d: cooltime is not an int' % lv)
            nx['nodes'][c] = (nm, ch, num, T_INT, struct.pack('<q', COOLDOWN))
            return
    fail('level node %d has no cooltime to rewrite' % lv)


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


def shift_base(base, points):
    """Children remap: +1 per point strictly below base.

    A point equal to ``base`` is the front-insert case: the new node takes
    the old first slot, so the parent keeps its pointer value.
    """
    return base + bisect.bisect_left(points, base)


def splice_cooltime(nx, pairs):
    """Insert one cooltime int node per (level node, point), sorted by point."""
    points = [at for _, at in pairs]
    if points != sorted(points) or len(set(points)) != len(points):
        fail('insertion points are not unique and ascending')
    for lv, at in pairs:
        if at <= lv:
            fail('insertion point %d is not after level node %d' % (at, lv))
    check_insert_points(nx, points, [lv for lv, _ in pairs])
    strings = nx['strings']
    index = {s: i for i, s in enumerate(strings)}
    cool_sid = intern(strings, index, COOLTIME)
    new_node = (cool_sid, 0, 0, T_INT, struct.pack('<q', COOLDOWN))
    out, pi = [], 0
    for i, node in enumerate(nx['nodes']):
        while pi < len(points) and points[pi] == i:
            out.append(new_node)
            pi += 1
        out.append((node[0], shift_base(node[1], points)) + tuple(node[2:]))
    nx['nodes'] = out
    for lv, _ in pairs:
        i = shift_base(lv, points)  # points sit below the children blocks,
        nm, ch, num, typ, union = out[i]  # so level nodes never move
        out[i] = (nm, ch, num + 1, typ, union)


def verify_skill(check, img, sid, snaps):
    """Post-build comparison of every level against the pre-change state."""
    for lv, before in zip(level_indices(check, img, sid), snaps):
        after = level_children(check, lv)
        names = [k[0] for k in after]
        if names != sorted(names):
            fail('%d: level children unsorted after rebuild' % sid)
        want = {k[0]: (k[1], k[2]) for k in before if k[0] != COOLTIME}
        got = {k[0]: (k[1], k[2]) for k in after if k[0] != COOLTIME}
        if got != want:
            fail('%d: level children drifted after rebuild' % sid)
        if dict((k[0], (k[1], k[2])) for k in after).get(COOLTIME) != \
                (T_INT, struct.pack('<q', COOLDOWN)):
            fail('%d: cooltime != %d after rebuild' % (sid, COOLDOWN))


def patch_nx(skill_nx, dry):
    """Returns ((img, sid, levels, ok, set, inserted), ...)."""
    nx = parse_nx(skill_nx)
    report, snaps, pairs, sets = [], {}, [], []
    for img, sid in ULTIMATES:
        lvs = level_indices(nx, img, sid)
        snaps[(img, sid)] = [level_children(nx, lv) for lv in lvs]
        ok = fixed = 0
        for lv in lvs:
            act, at = plan_level(nx, lv)
            if act == 'insert':
                pairs.append((lv, at))
            elif act == 'set':
                sets.append(lv)
                fixed += 1
            else:
                ok += 1
        report.append((img, sid, len(lvs), ok, fixed,
                       sum(1 for lv, _ in pairs if lv in lvs)))
    for img, sid, levels, ok, fixed, ins in report:
        print('  NX %d.img/skill/%d: %d levels (ok %d, set %d, insert %d)'
              % (img, sid, levels, ok, fixed, ins))
    if not pairs and not sets:
        print('NX side: all levels already cooltime=%d; no-op' % COOLDOWN)
        return report
    if dry:
        print('NX side: dry-run, nothing written')
        return report
    bak, _ = backup_once(skill_nx)
    for lv in sets:
        set_cooltime(nx, lv)
    if pairs:
        splice_cooltime(nx, sorted(pairs, key=lambda p: p[1]))
    blob = with_tail_pad(build(nx), nx)
    tmp = Path(str(skill_nx) + '.tmp')
    tmp.write_bytes(blob)
    try:
        check = parse_nx(tmp)
        for img, sid in ULTIMATES:
            verify_skill(check, img, sid, snaps[(img, sid)])
    except SystemExit:
        tmp.unlink()
        raise
    os.replace(tmp, skill_nx)
    print('NX side: wrote %s (%d bytes, %d nodes), backup %s'
          % (skill_nx, len(blob), len(nx['nodes']), bak.name))
    return report


# ------------------------------------------------------------- server XML side

def imgdir_span(text, open_off):
    """(inner_start, close_off) of the imgdir opened at open_off."""
    depth, inner = 0, None
    for m in IMGDIR.finditer(text, open_off):
        closing = m.group(0).startswith('</')
        if inner is None:
            if closing:
                fail('imgdir tag expected at offset %d' % open_off)
            inner = m.end()
        depth += -1 if closing else 1
        if depth == 0:
            return inner, m.start()
    fail('unterminated <imgdir> at offset %d' % open_off)


def level_spans(text, sid):
    """[(name, inner_start, close_off)] of the skill's 30 level imgdirs."""
    open_m = re.search(r'<imgdir name="%d">' % sid, text)
    if open_m is None:
        fail('%d: no <imgdir name="%d"> in the XML' % (sid, sid))
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


def xml_edits(text, sid):
    """(edits, (inserted, fixed, ok)); edits are (start, end, replacement)."""
    edits, stats = [], [0, 0, 0]
    for _, inner, close in level_spans(text, sid):
        found = COOLTIME_INT.search(text, inner, close)
        if found is None:
            edits.append((close, close, INT_TAG % COOLDOWN))
            stats[0] += 1
        elif int(found.group(1)) != COOLDOWN:
            edits.append((found.start(), found.end(), INT_TAG % COOLDOWN))
            stats[1] += 1
        else:
            stats[2] += 1
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
    """Returns ((img, sid, levels, ok, set, inserted), ...)."""
    report = []
    for img, sid in ULTIMATES:
        path = Path(server_wz) / ('%d.img.xml' % img)
        if not path.exists():
            fail('missing %s' % path)
        text = read_text(path)
        edits, (ins, fixed, ok) = xml_edits(text, sid)
        report.append((img, sid, ok + fixed + ins, ok, fixed, ins))
        print('  XML %s/%d: %d levels (ok %d, set %d, insert %d)'
              % (path.name, sid, ok + fixed + ins, ok, fixed, ins))
        if not edits:
            continue
        if dry:
            print('  XML %s: dry-run, nothing written' % path.name)
            continue
        bak, _ = backup_once(path)
        write_text(path, apply_edits(text, edits))
        print('  XML %s: wrote %d edit(s) (+%d bytes), backup %s'
              % (path.name, len(edits),
                 len(read_text(path)) - len(text), bak.name))
    return report


def main(argv=None):
    ap = argparse.ArgumentParser(
        description='Add cooltime=%d to the ultimate skills' % COOLDOWN)
    ap.add_argument('--assets-dir', type=Path, default=REPO / 'assets')
    ap.add_argument('--server-wz', type=Path,
                    default=REPO / 'link_repos/MapleStory-Server/wz/Skill.wz')
    ap.add_argument('--dry-run', action='store_true')
    args = ap.parse_args(argv)

    print('Ultimate cooldowns (cooltime=%d): %s'
          % (COOLDOWN, ', '.join(str(sid) for _, sid in ULTIMATES)))
    print('NX side:')
    nx_report = patch_nx(args.assets_dir / 'Skill.nx', args.dry_run)
    print('Server XML side:')
    xml_report = patch_xml(args.server_wz, args.dry_run)

    for side, rep in (('NX', nx_report), ('XML', xml_report)):
        for img, sid, levels, ok, fixed, ins in rep:
            if levels != LEVELS:
                fail('%s %d: expected %d levels, saw %d'
                     % (side, sid, LEVELS, levels))
    total = len(ULTIMATES) * LEVELS
    print('summary: %d skills x %d levels = %d per side '
          '(NX inserts %d, XML inserts %d)'
          % (len(ULTIMATES), LEVELS, total,
             sum(r[5] for r in nx_report), sum(r[5] for r in xml_report)))


if __name__ == '__main__':
    main()
