#!/usr/bin/env python3
"""Graft the three 079-missing Final Attack variants and unify FA counters.

079MAX4 dropped 1100003 (Fighter axe), 1200002 (Page blunt) and 1300003
(Spearman polearm) from ``assets/Skill.nx`` even though 1001005's
``finalAttack`` table still names them, and it also lost the FA trigger
tables of the Dawn Warrior 1st-job attacks 11001002/11001003.  The v83
archive still has all of them, so this tool:

* copies 1100003/1200002/1300003 (three icon bitmaps, hit, req, skillType,
  30 levels) from ``Skill.nx.v83-backup`` into their sorted slot under
  ``<img>/skill``, then replaces the v83 ``level/<n>`` child sets with the
  per-level scalars/vectors of the resident sibling (1100003 <- 1100002,
  1200002 <- 1200003, 1300003 <- 1300002): the 079MAX4 balance (level 30:
  prop 60 / damage 350) and the ``lt``/``rb`` hitbox vectors.  The graft's
  own icon/hit/req/skillType children are left alone;
* re-creates ``finalAttack`` (one entry 11101002 with 0=30, 1=40) under
  1100.img/skill/11001002 and .../11001003, preserving the other children;
* forces ``attackCount=2``/``mobCount=8`` on every level of the six warrior
  FA skills (three grafts + 1100002/1200003/1300002) and of the archer and
  Cygnus variants 3100001, 3200001, 11101002, 13101002.  The three ranged
  variants (3100001/3200001/13101002) also get ``bulletCount=2``: the client
  derives the damage-line count of an ``Attack::RANGED`` follow-up from
  ``bulletCount`` (``Skill.cpp`` ``apply_stats``), so without it the archer
  FA would still land a single line; 11101002 is the Dawn Warrior sword FA
  (``Attack::CLOSE``) and keeps the plain 2/8 pair.

Grafts reuse ``inject_skill_nx``'s descriptor clone, counters reuse
``patch_final_attack_counts_nx``'s sorted-block splice and the level
inheritance splices leaf runs (inserts missing keys, drops extras) so every
child block stays bytewise sorted (nlnx resolves children by binary search).
The archive is rebuilt with the reader's tail padding, re-parsed and
asserted (levels/counters, trigger tables, sibling-identical level sets,
icon bitmaps, untouched images byte-identical).
Idempotent: detected nodes, correct values and sibling-identical levels skip
the write; a JSON summary is printed and any failed assertion exits non-zero.
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import struct
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
for _p in (REPO / 'tools', REPO / 'tools/nx'):
    sys.path.insert(0, str(_p))

from inject_npc_nx import with_tail_pad  # noqa: E402
from inject_skill_nx import (  # noqa: E402
    T_BITMAP, T_INT, describe, graft, normalise_levels, source_ctx,
)
from level_children import (  # noqa: E402
    apply_counters, apply_level_sync, level_leaves, plan_counters,
    plan_level_sync, sorted_leaves,
)
from map079_tree import (  # noqa: E402
    child_indices, find_path, node_name, skill_digests,
)
from merge_skill_nx import build, parse_nx  # noqa: E402
from patch_final_attack_counts_nx import PROPS, level_indices  # noqa: E402

# (img id, skill id): missing from 079MAX4 but present in the v83 backup.
GRAFTS = ((110, '1100003'), (120, '1200002'), (130, '1300003'))
# Warrior FA skills 079MAX4 already ships; only their counters are enforced.
NATIVE = ((110, '1100002'), (120, '1200003'), (130, '1300002'))
# Graft -> resident sibling whose full per-level child set it inherits
# (the 079MAX4 rebalance: level 30 prop 60 / damage 350 plus lt/rb).
SIBLING = (((110, '1100003'), (110, '1100002')),
           ((120, '1200002'), (120, '1200003')),
           ((130, '1300003'), (130, '1300002')))
SIBLING_OF = {graft[1]: sib[1] for graft, sib in SIBLING}
# Archer / Cygnus FA skills that exist but omit the counters.
COUNTERS = ((310, '3100001'), (320, '3200001'), (1110, '11101002'),
            (1310, '13101002'))
# Ranged FA variants: Player::prepare_attack(true) returns Attack::RANGED for
# bow/crossbow (and the Cygnus bow), and Skill.cpp apply_stats then derives
# hitcount from bulletCount instead of attackCount.
RANGED = ((310, '3100001'), (320, '3200001'), (1310, '13101002'))
BULLET_PROPS = ((b'bulletCount', 2),)
# 11101002 is the Dawn Warrior sword FA (CLOSE) and stays on attackCount/2.
EXTRA_PROPS = {target: BULLET_PROPS for target in RANGED}
RANGED_IDS = frozenset(sid for _, sid in RANGED)
# 1st-job attacks that lost their FA trigger table in 079MAX4.
TRIGGERS = ((1100, '11001002'), (1100, '11001003'))
TRIGGER_ENTRY, TRIGGER_PROPS = '11101002', {'0': 30, '1': 40}
ICONS = (b'icon', b'iconDisabled', b'iconMouseOver')
EXTRA_KEYS = (b'hit', b'req', b'skillType')
LEVELS = 30
BACKUP_SUFFIX = '.pre-fa-variants-backup'
ALL_FA = GRAFTS + NATIVE + COUNTERS
TOUCHED = ALL_FA + TRIGGERS
CHANGED_IMAGES = frozenset('%d.img' % img for img, _ in TOUCHED)
WANT_TRIGGER = {TRIGGER_ENTRY: TRIGGER_PROPS}
NORMALISE = {name.decode(): value for name, value in PROPS}


def fail(msg):
    sys.exit('inject_final_attack_variants: ' + msg)


def skill_path(img, sid):
    """Node path of one skill subtree in a Skill.nx archive."""
    return (('%d.img' % img).encode(), b'skill', sid.encode())


def require(nx, path, what):
    idx = find_path(nx, path)
    if idx is None:
        fail('%s not found: %s' % (what, b'/'.join(path).decode()))
    return idx


def child(nx, idx, name):
    """Index of idx's direct child called name, or None."""
    return next((c for c in child_indices(nx, idx) if node_name(nx, c) == name), None)


def int_values(nx, idx):
    """name -> value for every int child of a node."""
    return {node_name(nx, c): struct.unpack('<q', nx['nodes'][c][4])[0]
            for c in child_indices(nx, idx) if nx['nodes'][c][3] == T_INT}


def backup_once(path):
    """Keep one pre-run copy; never overwrite an existing backup."""
    bak = Path(str(path) + BACKUP_SUFFIX)
    if bak.exists():
        return bak, False
    shutil.copy2(path, bak)
    return bak, True


def trigger_table(nx, skill_idx):
    """{entry: {key: int}} of a finalAttack node, or None when absent."""
    fa = child(nx, skill_idx, b'finalAttack')
    if fa is None:
        return None
    return {node_name(nx, c).decode(): {k.decode(): v
                                       for k, v in int_values(nx, c).items()}
            for c in child_indices(nx, fa)}

# ------------------------------------------------------------------ grafting
def graft_skills(nx, src, targets):
    """Clone missing skill subtrees (counters normalised) into their slot."""
    out, ctx = [], source_ctx(nx, share_blobs=False)
    for img, sid in targets:
        parent = require(nx, skill_path(img, sid)[:2], 'skill parent')
        desc = describe(src, require(src, skill_path(img, sid), 'v83 skill'), ctx)
        added = graft(nx, parent, normalise_levels(desc, NORMALISE, ctx))
        out.append({'image': '%d.img' % img, 'skill': sid, 'nodes': added})
    return out


def graft_triggers(nx, src, targets):
    """Clone the v83 finalAttack table into the given 1st-job attacks."""
    out, ctx = [], source_ctx(nx, share_blobs=True)
    for img, sid in targets:
        parent = require(nx, skill_path(img, sid), 'trigger skill')
        path = skill_path(img, sid) + (b'finalAttack',)
        added = graft(nx, parent, describe(src, require(src, path, 'v83 table'), ctx))
        out.append({'image': '%d.img' % img, 'skill': sid, 'nodes': added})
    return out

# Level counters and sibling level inheritance live in level_children.


def check_counters(nx, img, sid):
    """30 sorted levels: attackCount=2/mobCount=8, bulletCount=2 if ranged."""
    lvs = level_indices(nx, img, sid)
    props = tuple(PROPS) + tuple(EXTRA_PROPS.get((img, sid), ()))
    for lv in lvs:
        vals = int_values(nx, lv)
        for prop, value in props:
            if vals.get(prop) != value:
                fail('%s level %s: %s=%r, expected %d' % (
                    sid, node_name(nx, lv).decode(), prop.decode(),
                    vals.get(prop), value))
        if sid not in RANGED_IDS and b'bulletCount' in vals:
            fail('%s level %s: melee FA must not carry bulletCount' % (
                sid, node_name(nx, lv).decode()))
        names = [node_name(nx, c) for c in child_indices(nx, lv)]
        if names != sorted(names):
            fail('%s level %s: children unsorted' % (sid, node_name(nx, lv).decode()))


def check_graft(nx, src, img, sid):
    """Grafted skill: own icons/metadata, sibling-identical level children.

    The graft keeps the v83 subtree's icon bitmaps, hit/req/skillType and its
    own level nodes, but each ``level/<n>`` must carry exactly the resident
    sibling's children (079MAX4 balance: level 30 prop 60 / damage 350 plus
    the lt/rb hitbox vectors).
    """
    idx = require(nx, skill_path(img, sid), 'graft skill')
    src_idx = require(src, skill_path(img, sid), 'v83 skill')
    for name in EXTRA_KEYS + ICONS:
        if child(nx, idx, name) is None:
            fail('%s: graft lost %s' % (sid, name.decode()))
    if nx['nodes'][idx][2] != len(EXTRA_KEYS) + len(ICONS) + 1:
        fail('%s: unexpected children under the grafted skill' % sid)
    for name in ICONS:
        c, s = child(nx, idx, name), child(src, src_idx, name)
        if nx['nodes'][c][3] != T_BITMAP:
            fail('%s: %s is not a bitmap' % (sid, name.decode()))
        bmp, w, h = struct.unpack('<IHH', nx['nodes'][c][4])
        sbmp, sw, sh = struct.unpack('<IHH', src['nodes'][s][4])
        if (w, h) != (sw, sh) or not w or not h:
            fail('%s: %s is %dx%d, v83 has %dx%d'
                 % (sid, name.decode(), w, h, sw, sh))
        if bmp >= len(nx['bitmaps']) or nx['bitmaps'][bmp] != src['bitmaps'][sbmp]:
            fail('%s: %s blob differs from the v83 source' % (sid, name.decode()))
    curve = {}
    sibling = SIBLING_OF[sid]
    for lv in range(1, LEVELS + 1):
        level = (b'level', str(lv).encode())
        sib = require(nx, skill_path(img, sibling) + level, 'sibling level')
        got = require(nx, skill_path(img, sid) + level, 'level')
        if level_leaves(nx, got) != sorted_leaves(nx, sib):
            fail('%s level %d: %r does not match sibling %s'
                 % (sid, lv, level_leaves(nx, got), sibling))
        values = int_values(nx, got)
        for key in (b'prop', b'damage'):
            curve['%s_%d' % (key.decode(), lv)] = values[key]
    want = (curve['prop_1'], curve['prop_30'], curve['damage_1'], curve['damage_30'])
    if want != (2, 60, 105, 350):
        fail('%s: curve prop/damage %r does not match sibling %s'
             % (sid, want, sibling))
    for key in (b'lt', b'rb'):
        if child(nx, require(nx, skill_path(img, sid) + (b'level', b'30'),
                             'level 30'), key) is None:
            fail('%s level 30 lacks the %s hitbox vector' % (sid, key.decode()))
    return {'skill': sid, 'sibling': sibling,
            'prop_1': curve['prop_1'], 'prop_30': curve['prop_30'],
            'damage_1': curve['damage_1'], 'damage_30': curve['damage_30']}


def check_triggers(nx, img, sid):
    """finalAttack must hold exactly 11101002 with ints 0=30 and 1=40."""
    idx = require(nx, skill_path(img, sid), 'trigger skill')
    table = trigger_table(nx, idx)
    if table != WANT_TRIGGER:
        fail('%s: finalAttack is %r, expected %r' % (sid, table, WANT_TRIGGER))


def check_untouched(nx, before):
    """Everything outside the edited subtrees must be structurally identical."""
    now = skill_digests(nx, CHANGED_IMAGES)
    if set(now['images']) != set(before['images']):
        fail('the image list changed: %r'
             % (set(now['images']) ^ set(before['images']),))
    checked = 0
    for image, digest in before['images'].items():
        if image not in CHANGED_IMAGES:
            if now['images'][image] != digest:
                fail('%s changed although this tool never touches it' % image)
            checked += 1
    for image, skills in before['skills'].items():
        now_skills = now['skills'][image]
        added = {sid for img, sid in TOUCHED if '%d.img' % img == image}
        if set(now_skills) - set(skills) - added:
            fail('%s gained unexpected skills' % image)
        for sid, digest in skills.items():
            if sid in added:
                continue
            if now_skills.get(sid) != digest:
                fail('%s/skill/%s changed although not touched' % (image, sid))
            checked += 1
    return checked


def verify(nx, src, before):
    """Assert every contract on the (re-read) patched archive."""
    for img, sid in ALL_FA:
        check_counters(nx, img, sid)
    for img, sid in TRIGGERS:
        check_triggers(nx, img, sid)
    curves = [check_graft(nx, src, img, sid) for img, sid in GRAFTS]
    return {'counters': len(ALL_FA), 'triggers': len(TRIGGERS),
            'grafts': len(GRAFTS), 'ranged': len(RANGED),
            'untouched_digests': check_untouched(nx, before),
            'curves': curves}

# --------------------------------------------------------------------- main
def report_rows(rows):
    """Print one plan/result row per target skill."""
    for r in rows:
        extra = [prop for prop in r['props'] if prop not in NORMALISE]
        print('  NX %s/skill/%s: ok %d, set %d, insert %d%s' % (
            r['image'], r['skill'], r['ok'], r['set'], r['insert'],
            ' [%s=2]' % ','.join(extra) if extra else ''))


def report_sync_rows(rows):
    """Print one level-inheritance row per grafted skill."""
    for r in rows:
        print('  NX %s/skill/%s <- %s: %d/%d levels copied' % (
            r['image'], r['skill'], r['sibling'], r['levels'], r['of']))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--assets-dir', type=Path, default=REPO / 'assets')
    ap.add_argument('--v83-backup', type=Path, default=None)
    ap.add_argument('--dry-run', action='store_true')
    args = ap.parse_args(argv)

    started = time.time()
    skill_nx = Path(args.assets_dir) / 'Skill.nx'
    backup_nx = args.v83_backup or Path(args.assets_dir) / 'Skill.nx.v83-backup'
    if not skill_nx.exists() or not backup_nx.exists():
        fail('missing %s or %s' % (skill_nx, backup_nx))

    print('Final Attack variants: %s + %s' % (skill_nx.name, backup_nx.name))
    old_size = skill_nx.stat().st_size
    nx = parse_nx(skill_nx)
    src = parse_nx(backup_nx)
    before = skill_digests(nx, CHANGED_IMAGES)

    missing = [(i, s) for i, s in GRAFTS if find_path(nx, skill_path(i, s)) is None]
    redo = []
    for img, sid in TRIGGERS:
        table = trigger_table(nx, require(nx, skill_path(img, sid), 'trigger skill'))
        if table is None:
            redo.append((img, sid))
        elif table != WANT_TRIGGER:
            fail('%s: finalAttack is %r, expected %r; restore %s first'
                 % (sid, table, WANT_TRIGGER, skill_nx.name))
    for img, sid in missing:
        require(src, skill_path(img, sid), 'v83 skill')

    summary = {'tool': 'inject_final_attack_variants_nx', 'archive': str(skill_nx),
               'bytes_before': old_size, 'grafted_skills': [], 'counters': [],
               'grafted_triggers': [], 'backup': None,
               'ranged': [sid for _, sid in RANGED]}

    if not missing and not redo:
        rows, _, _, _ = plan_counters(nx, ALL_FA, EXTRA_PROPS)
        sync_rows, edits = plan_level_sync(nx, SIBLING)
        if not any(row['set'] + row['insert'] for row in rows) and not edits:
            summary.update(status='noop', bytes_after=old_size, counters=rows,
                           level_sync=sync_rows)
            summary['verified'] = verify(nx, src, before)
            summary['elapsed_s'] = round(time.time() - started, 2)
            print('  archive already patched; nothing to write')
            report_rows(rows)
            report_sync_rows(sync_rows)
            print('summary ' + json.dumps(summary, sort_keys=True))
            return

    if missing:
        summary['grafted_skills'] = graft_skills(nx, src, missing)
        for row in summary['grafted_skills']:
            print('  graft %s/skill/%s: %d nodes'
                  % (row['image'], row['skill'], row['nodes']))
    if redo:
        summary['grafted_triggers'] = graft_triggers(nx, src, redo)
        for row in summary['grafted_triggers']:
            print('  graft %s/skill/%s/finalAttack: %d nodes'
                  % (row['image'], row['skill'], row['nodes']))

    sync_rows, edits = plan_level_sync(nx, SIBLING)
    summary['level_sync'] = sync_rows
    report_sync_rows(sync_rows)
    synced = apply_level_sync(nx, edits)

    rows, inserts, sets, per_level = plan_counters(nx, ALL_FA, EXTRA_PROPS)
    summary['counters'] = rows
    report_rows(rows)

    if args.dry_run:
        apply_counters(nx, inserts, sets, per_level)
        summary.update(status='dry-run', bytes_after=old_size)
        summary['verified'] = verify(nx, src, before)
        summary['elapsed_s'] = round(time.time() - started, 2)
        print('  dry-run: nothing written')
        print('summary ' + json.dumps(summary, sort_keys=True))
        return

    bak, created = backup_once(skill_nx)
    summary['backup'] = {'path': str(bak), 'created': created}
    print('  backup %s (%s)' % (bak.name, 'created' if created else 'kept'))

    spliced = apply_counters(nx, inserts, sets, per_level)
    blob = with_tail_pad(build(nx), nx)
    tmp = Path(str(skill_nx) + '.tmp')
    tmp.write_bytes(blob)
    try:
        verified = verify(parse_nx(tmp), src, before)
    except SystemExit:
        tmp.unlink()
        raise
    os.replace(tmp, skill_nx)

    summary.update(status='patched', bytes_after=len(blob), spliced=spliced,
                   overwritten=len(sets), synced=synced, verified=verified,
                   elapsed_s=round(time.time() - started, 2))
    print('  wrote %s: %d -> %d bytes (%+d), %d counter ints spliced, '
          '%d levels synced' % (skill_nx, old_size, len(blob),
                                len(blob) - old_size, spliced, synced))
    for c in verified['curves']:
        print('  curve %s <- %s: prop %d->%d, damage %d->%d (sibling levels)'
              % (c['skill'], c['sibling'], c['prop_1'], c['prop_30'],
                 c['damage_1'], c['damage_30']))
    print('  verified: %d FA skills x %d levels at %d/%d, %d ranged FA also '
          'at bulletCount=%d, %d trigger tables, %d grafted icon sets, %d '
          'untouched digests'
          % (verified['counters'], LEVELS, NORMALISE['attackCount'],
             NORMALISE['mobCount'], verified['ranged'],
             BULLET_PROPS[0][1], verified['triggers'], verified['grafts'],
             verified['untouched_digests']))
    print('summary ' + json.dumps(summary, sort_keys=True))


if __name__ == '__main__':
    main()
