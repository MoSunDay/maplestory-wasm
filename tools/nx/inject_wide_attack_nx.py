#!/usr/bin/env python3
"""Inject the twelve wide-range fourth-job attacks into Skill.nx/String.nx.

The target subtrees are cloned from an existing source skill in the same
archive, then their top-level buff/charge children are stripped and their
thirty source levels are replaced by ten identical manifest levels.  This
keeps the shipped hit/effect/ball/action animations while making every new
skill a plain damage attack.

Usage:
    inject_wide_attack_nx.py [--dry-run] [--assets-dir assets]
                             [--manifest tools/nx/wide_attack_manifest.json]
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

from inject_npc_nx import sorted_slot, splice, with_tail_pad  # noqa: E402
from inject_skill_nx import (  # noqa: E402
    T_NONE, T_INT, T_STRING, T_VECTOR,
    describe, graft, int_child, rename, source_ctx,
)
from map079_tree import find_path, node_name  # noqa: E402
from merge_skill_nx import build, intern, parse_nx  # noqa: E402
from wide_attack_common import (  # noqa: E402
    LEVELS, load_manifest, xml_escape,
)

SKILL = b'skill'
DROP_TOP = {
    b'invisible', b'skillType', b'dot', b'dotTime', b'dotInterval',
    b'subTime', b'forceCon', b'charge', b'CHARGE',
}
LEVEL_KEYS = {
    'attackCount', 'bulletCount', 'bulletConsume', 'cooltime', 'damage',
    'hs', 'lt', 'mad', 'mobCount', 'mpCon', 'range', 'rb',
}
INT_KEYS = {'attackCount', 'bulletCount', 'bulletConsume', 'cooltime',
            'damage', 'mad', 'mobCount', 'mpCon', 'range'}
VECTOR_KEYS = {'lt', 'rb'}


def fail(msg):
    sys.exit('inject_wide_attack_nx: ' + msg)


def node_values(nx, idx):
    """Child name -> int/string/vector value for one NX node."""
    base, num = nx['nodes'][idx][1], nx['nodes'][idx][2]
    out = {}
    for child in range(base, base + num):
        typ, union = nx['nodes'][child][3], nx['nodes'][child][4]
        if typ == T_INT:
            out[node_name(nx, child)] = struct.unpack('<q', union)[0]
        elif typ == T_STRING:
            out[node_name(nx, child)] = nx['strings'][
                struct.unpack_from('<I', union, 0)[0]]
        elif typ == T_VECTOR:
            out[node_name(nx, child)] = struct.unpack('<ii', union)
    return out


def skill_path(target):
    return (target['img'].encode(), SKILL, str(target['id']).encode())


def source_path(target):
    return (target['source_img'].encode(), SKILL,
            str(target['source_id']).encode())


def string_key(target):
    return ('%07d' % int(target['id'])).encode()


def int_desc(ctx, key, value):
    return (intern(ctx['strings'], ctx['index'], key.encode()), T_INT,
            struct.pack('<q', value), ())


def vector_desc(ctx, key, value):
    return (intern(ctx['strings'], ctx['index'], key.encode()), T_VECTOR,
            struct.pack('<ii', int(value[0]), int(value[1])), ())


def string_desc(ctx, key, value):
    return (intern(ctx['strings'], ctx['index'], key.encode()), T_STRING,
            struct.pack('<II', intern(ctx['strings'], ctx['index'],
                                      value.encode('utf-8')), 0), ())


def level_kids(ctx, target):
    stats = target['stats']
    children = []
    for key in sorted(LEVEL_KEYS):
        if key == 'hs':
            children.append(string_desc(ctx, key, 'h1'))
        elif key in VECTOR_KEYS and key in stats:
            children.append(vector_desc(ctx, key, stats[key]))
        elif key in INT_KEYS and key in stats:
            children.append(int_desc(ctx, key, int(stats[key])))
    return tuple(children)


def new_levels(ctx, target):
    kids = tuple(
        (intern(ctx['strings'], ctx['index'], str(level).encode()), T_NONE,
         b'\0' * 8, level_kids(ctx, target))
        for level in LEVELS
    )
    # nlnx resolves children by bytewise binary search, so keep "1,10,2,...".
    kids = tuple(sorted(kids, key=lambda kid: ctx['strings'][kid[0]]))
    return (intern(ctx['strings'], ctx['index'], b'level'), T_NONE,
            b'\0' * 8, kids)


def new_action(ctx, action):
    kids = tuple(string_desc(ctx, str(index), value)
                 for index, value in enumerate(action))
    return (intern(ctx['strings'], ctx['index'], b'action'), T_NONE,
            b'\0' * 8, kids)


def expected_entries(target):
    name = target['name']
    desc = target['desc']
    stats = target['stats']
    # Magic skills scale via mad% (no damage stat); the rest use damage%.
    if target['kind'] == 'magic':
        text = ('Deals %d%% magic damage %d time(s) to %d enemies. '
                'MP %d, cooldown %ds.'
                % (int(stats['mad']), int(stats['attackCount']),
                   int(stats['mobCount']), int(stats['mpCon']),
                   int(stats['cooltime'])))
    else:
        text = ('Deals %d%% damage %d time(s) to %d enemies. MP %d, '
                'cooldown %ds.'
                % (int(stats['damage']), int(stats['attackCount']),
                   int(stats['mobCount']), int(stats['mpCon']),
                   int(stats['cooltime'])))
    entries = {'name': name, 'desc': desc}
    entries.update({'h%d' % level: text for level in LEVELS})
    return entries


def transform(ctx, desc, target):
    name, typ, union, kids = desc
    kept = []
    for kid in kids:
        key = ctx['strings'][kid[0]]
        if key in DROP_TOP or key in (b'level', b'masterLevel', b'action'):
            continue
        kept.append(kid)
    if 'action' in target:
        kept.append(new_action(ctx, [target['action']]))
    else:
        kept.extend(kid for kid in kids
                    if ctx['strings'][kid[0]] == b'action')
    kept.append(int_desc(ctx, 'masterLevel', 10))
    kept.append(new_levels(ctx, target))
    kept.sort(key=lambda kid: ctx['strings'][kid[0]])
    return (name, typ, union, tuple(kept))


def check_target(nx, target, ctx=None):
    idx = find_path(nx, skill_path(target))
    if idx is None:
        fail('missing target %d' % target['id'])
    top = node_values(nx, idx)
    for bad in DROP_TOP:
        if bad in top:
            fail('%d still has %s' % (target['id'], bad.decode()))
    if int_child(nx, idx, b'masterLevel') != 10:
        fail('%d masterLevel != 10' % target['id'])
    if 'action' in target:
        action = find_path(nx, skill_path(target) + (b'action',))
        if action is None:
            fail('%d has no action' % target['id'])
        action_vals = node_values(nx, action)
        if action_vals.get(b'0') != target['action'].encode():
            fail('%d action/0 != %s' % (target['id'], target['action']))
        if action_vals.get(b'0') == b'alert2':
            fail('%d uses alert2' % target['id'])
    level = find_path(nx, skill_path(target) + (b'level',))
    if level is None:
        fail('%d has no levels' % target['id'])
    base, num = nx['nodes'][level][1], nx['nodes'][level][2]
    names = {node_name(nx, base + i).decode() for i in range(num)}
    if names != {str(i) for i in LEVELS}:
        fail('%d level names are not 1..10' % target['id'])
    for i in range(num):
        values = node_values(nx, base + i)
        want = target['stats']
        for key in LEVEL_KEYS - {'hs'}:
            if key not in want:
                continue
            expected = (tuple(want[key]) if key in VECTOR_KEYS
                        else want[key])
            if values.get(key.encode()) != expected:
                fail('%d level %s %s mismatch' % (
                    target['id'], key, node_name(nx, base + i).decode()))
    return idx


def existing_state(skill_nx, string_nx, targets):
    have_skill = [find_path(skill_nx, skill_path(t)) is not None
                  for t in targets]
    wrapper = find_path(string_nx, (b'Skill.img',))
    have_string = []
    for target in targets:
        have_string.append(wrapper is not None and
                           find_path(string_nx, (b'Skill.img',
                                                 string_key(target))) is not None)
    return have_skill, have_string


def inject_string(string_nx, target):
    """Insert one String.img skill entry; returns nothing (mutates nodes)."""
    wrapper = find_path(string_nx, (b'Skill.img',))
    if wrapper is None:
        fail('String.nx has no Skill.img')
    name = string_key(target)
    base, num = string_nx['nodes'][wrapper][1], string_nx['nodes'][wrapper][2]
    if any(node_name(string_nx, base + i) == name for i in range(num)):
        fail('String.nx already contains %s' % name.decode())
    strings, index = string_nx['strings'], {
        value: i for i, value in enumerate(string_nx['strings'])}
    from inject_npc_nx import flatten_children
    entries = sorted(expected_entries(target).items())
    kids = tuple(
        (intern(strings, index, key.encode()), T_STRING,
         struct.pack('<II', intern(strings, index, value.encode('utf-8')), 0),
         ())
        for key, value in entries
    )
    desc = (intern(strings, index, name), T_NONE, b'\0' * 8, kids)
    subtree = []
    first, count = flatten_children(desc, subtree)
    at = sorted_slot(string_nx, wrapper, name)
    insert = (desc[0], first + len(string_nx['nodes']) + 1, count,
              T_NONE, b'\0' * 8)
    nodes = splice(string_nx['nodes'], at, insert, subtree)
    nodes[wrapper] = (nodes[wrapper][0], nodes[wrapper][1],
                      nodes[wrapper][2] + 1, nodes[wrapper][3],
                      nodes[wrapper][4])
    string_nx['nodes'] = nodes


def verify_string(string_nx, targets):
    wrapper = find_path(string_nx, (b'Skill.img',))
    if wrapper is None:
        fail('String.nx lost Skill.img')
    for target in targets:
        idx = find_path(string_nx, (b'Skill.img', string_key(target)))
        if idx is None:
            fail('String.nx lost %s' % string_key(target).decode())
        values = node_values(string_nx, idx)
        for key, value in expected_entries(target).items():
            if values.get(key.encode()) != value.encode('utf-8'):
                fail('%d String.%s mismatch' % (target['id'], key))


def backup(path):
    bak = path.with_name(path.name + '.pre-wide-attack-backup')
    if bak.exists():
        fail('backup already exists, refusing to overwrite: %s\n'
             '  restore it over %s to start over' % (bak, path.name))
    shutil.copy2(path, bak)
    return bak


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--assets-dir', type=Path, default=REPO / 'assets')
    ap.add_argument('--manifest', type=Path,
                    default=REPO / 'tools/nx/wide_attack_manifest.json')
    ap.add_argument('--dry-run', action='store_true')
    args = ap.parse_args(argv)

    targets = load_manifest(args.manifest)
    skill_nx = Path(args.assets_dir) / 'Skill.nx'
    string_nx = Path(args.assets_dir) / 'String.nx'
    if not skill_nx.exists() or not string_nx.exists():
        fail('missing %s or %s' % (skill_nx, string_nx))

    nx = parse_nx(skill_nx)
    sx = parse_nx(string_nx)
    have_skill, have_string = existing_state(nx, sx, targets)
    if all(have_skill) and all(have_string):
        for target in targets:
            check_target(nx, target)
        verify_string(sx, targets)
        print('wide attacks already present and consistent; no-op')
        return
    if any(have_skill) or any(have_string):
        fail('partial wide-attack state detected; restore both backups first')

    for target in targets:
        if find_path(nx, source_path(target)) is None:
            fail('source %d not found in %s' % (
                target['source_id'], target['source_img']))
    if args.dry_run:
        for target in targets:
            print('dry-run %d <- %s/%d (%s, %s)'
                  % (target['id'], target['source_img'], target['source_id'],
                     target['kind'], target['name']))
        print('dry-run: no files written')
        return

    skill_bak = backup(skill_nx)
    string_bak = backup(string_nx)
    ctx = source_ctx(nx, share_blobs=True)
    # Every target has its own job wrapper; graft under the wrapper's skill.
    for target in targets:
        parent = find_path(nx, (target['img'].encode(), SKILL))
        if parent is None:
            fail('%s has no skill node' % target['img'])
        src = find_path(nx, source_path(target))
        desc = describe(nx, src, ctx)
        desc = transform(ctx, desc, target)
        desc = rename(desc, str(target['id']).encode(), ctx)
        added = graft(nx, parent, desc)
        print('  grafted %d: %d nodes' % (target['id'], added))

    for target in targets:
        check_target(nx, target)
        inject_string(sx, target)
    verify_string(sx, targets)

    for path, data, pad in ((skill_nx, nx, True), (string_nx, sx, False)):
        blob = with_tail_pad(build(data), data) if pad else build(data)
        tmp = path.with_name(path.name + '.tmp')
        tmp.write_bytes(blob)
        try:
            parse_nx(tmp)
        except SystemExit:
            tmp.unlink()
            raise
        os.replace(tmp, path)
        print('wrote %s (%d bytes)' % (path, len(blob)))

    print('backups: %s, %s' % (skill_bak.name, string_bak.name))


if __name__ == '__main__':
    main()
