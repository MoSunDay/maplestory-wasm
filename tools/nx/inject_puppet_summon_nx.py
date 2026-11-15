#!/usr/bin/env python3
"""Graft the 079-missing Ranger Puppet (3111002) into ``assets/Skill.nx``.

The 079MAX4 client ships ``311.img/skill`` without 3111002 (Puppet), so the
skill book cannot load it: ``SkillData`` sees no ``level`` table, masterlevel
resolves to 0 and ``Skill::can_use`` rejects every cast with ``FBR_OTHER``.
The v83 archive keeps the complete subtree (20 levels, ``summon`` frames,
``action``=alert2, icons), matching the linked server's ``311.img.xml`` which
fully supports the stationary puppet summon.  This tool copies the subtree out
of ``assets/Skill.nx.v83-backup`` and inserts it under the existing ``skill``
node, following the same subtree-graft machinery as ``inject_skill_nx.py``.

One-shot: a ``Skill.nx.pre-puppet-summon-backup`` copy is written first and
reruns are refused while it exists (restore it to start over).

Usage:
    inject_puppet_summon_nx.py [--assets-dir assets]
"""
from __future__ import annotations

import argparse
import os
import shutil
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
for _p in (REPO / 'tools', REPO / 'tools/nx'):
    sys.path.insert(0, str(_p))

from inject_npc_nx import flatten_children, sorted_slot, splice, with_tail_pad  # noqa: E402
from map079_tree import find_path, node_name, subtree_digest  # noqa: E402
from merge_skill_nx import build, parse_nx  # noqa: E402
from inject_skill_nx import (describe, graft, normalise_levels, require,  # noqa: E402
                             scalars, source_ctx)

IMG = b'311.img'
SKILL = b'skill'
PUPPET = 3111002
LEVELS = 20
# 079-missing int that the 079 dataset writes explicitly on every bow skill
# level (Arrow Bomb/Hurricane/Phoenix all carry bulletConsume=0); without it
# the loader defaults bulletcost to bulletcount=1 and Skill::can_use demands
# an arrow for a summon cast (FBR_BULLETCOST).
NORMALISE = {'bulletConsume': 0}
# 079 siblings that must stay byte-identical through the graft.
KEPT = (3110000, 3110001, 3111004, 3111006)


def skill_path(*names):
    return (IMG, SKILL) + tuple(str(n).encode() if isinstance(n, int) else n
                                for n in names)


def backup(path):
    bak = path.with_name('%s.pre-puppet-summon-backup' % path.name)
    if bak.exists():
        sys.exit('inject_puppet_summon_nx: backup already exists, refusing '
                 'to rerun: %s\n  (restore it over %s to start over)'
                 % (bak, path.name))
    shutil.copy2(path, bak)
    return bak


def check(nx, src, kept_before):
    """Verify the graft: skill present, levels/summon intact, siblings safe."""
    puppet = require(nx, skill_path(PUPPET), 'grafted puppet')
    base, num = nx['nodes'][puppet][1], nx['nodes'][puppet][2]
    kids = {node_name(nx, c) for c in range(base, base + num)}
    for key in (b'level', b'summon', b'action', b'icon'):
        if key not in kids:
            sys.exit('inject_puppet_summon_nx: grafted %d lacks %s'
                     % (PUPPET, key.decode()))
    level = require(nx, skill_path(PUPPET, b'level'), 'level')
    if nx['nodes'][level][2] != LEVELS:
        sys.exit('inject_puppet_summon_nx: %d has %d levels, expected %d'
                 % (PUPPET, nx['nodes'][level][2], LEVELS))
    for lv in range(1, LEVELS + 1):
        path = skill_path(PUPPET, b'level', lv)
        want = scalars(src, require(src, path, 'backup level'))
        got = scalars(nx, require(nx, path, 'graft level'))
        for key, value in want.items():
            if key.decode() in NORMALISE:
                continue
            if got.get(key) != value:
                sys.exit('inject_puppet_summon_nx: %d level %d: %s=%r, '
                         'backup has %r' % (PUPPET, lv, key.decode(),
                                            got.get(key), value))
        for key, value in NORMALISE.items():
            if got.get(key.encode()) != value:
                sys.exit('inject_puppet_summon_nx: %d level %d: %s=%r, '
                         'expected %r' % (PUPPET, lv, key, got.get(key.encode()),
                                          value))
    for kept in KEPT:
        digest = subtree_digest(nx, require(nx, skill_path(kept), 'kept'))
        if digest != kept_before[kept]:
            sys.exit('inject_puppet_summon_nx: sibling %d changed' % kept)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets-dir', default='assets')
    args = parser.parse_args()

    skill_nx = REPO / args.assets_dir / 'Skill.nx'
    backup_nx = REPO / args.assets_dir / 'Skill.nx.v83-backup'
    for path in (skill_nx, backup_nx):
        if not path.is_file():
            sys.exit('inject_puppet_summon_nx: missing %s' % path)

    nx = parse_nx(skill_nx)
    src = parse_nx(backup_nx)
    skill_idx = require(nx, skill_path(), 'skill')
    if find_path(nx, skill_path(PUPPET)) is not None:
        sys.exit('inject_puppet_summon_nx: Skill.nx already contains %d; '
                 'restore the backup first' % PUPPET)
    require(src, skill_path(PUPPET), 'backup puppet')

    old_size = skill_nx.stat().st_size
    kept_before = {s: subtree_digest(nx, require(nx, skill_path(s), 'kept'))
                   for s in KEPT}

    bak = backup(skill_nx)
    ctx = source_ctx(nx, share_blobs=False)
    desc = describe(src, require(src, skill_path(PUPPET), 'backup'), ctx)
    desc = normalise_levels(desc, NORMALISE, ctx)
    added = graft(nx, skill_idx, desc)
    print('  grafted %d: %d nodes' % (PUPPET, added))

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


if __name__ == '__main__':
    main()
