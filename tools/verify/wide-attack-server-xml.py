#!/usr/bin/env python3
"""Verify the 12 wide-attack skills in the MapleStory-Server wz XML dump.

The client ships the wide attacks in ``assets/Skill.nx`` / ``String.nx``; the
server reads the same tree from ``<server>/Skill.wz/<job>.img.xml`` and
``<server>/String.wz/Skill.img.xml``.  A skill that only exists on one side
either never shows up in the UI or is rejected by the server's skill loader,
so this probe audits the XML side against ``tools/nx/wide_attack_manifest.json``
(the same manifest the NX side is generated from):

* every ``<job>.img.xml`` keeps its skill node for the target id, with
  ``level`` children strictly ordered ``1``..``10`` in document order;
* each level repeats the manifest attackCount/mobCount/mpCon/cooltime,
  lt/rb vector and range, plus damage — or mad > 0 for the three magic
  skills, which must not carry damage — and bulletCount/bulletConsume for
  the three ranged skills (no field mismatch);
* the target has ``hit`` and neither ``invisible`` nor ``skillType``, its
  action/0 is a non-empty string that is not ``alert2``, and the optional
  ``weapon`` stays at the source skill's requirement;
* ``String.wz/Skill.img.xml`` has ``Skill.img/<id>`` with the manifest name
  and desc plus non-empty h1..h10;
* the original source skills (1121008, 1311003, 5201002, ...) still exist in
  their own ``<img>.img.xml`` with their hit animation.

Pure functions only; stdlib ElementTree.

Usage:
    wide-attack-server-xml.py [--server WZ_DIR] [--manifest MANIFEST]
"""
from __future__ import annotations

import argparse
import json
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEFAULT_SERVER = Path('/data/maplestory-wasm/link_repos/MapleStory-Server/wz')
DEFAULT_MANIFEST = REPO / 'tools/nx/wide_attack_manifest.json'

# Target weapon requirements, copied from each target's source skill.  The
# node is optional (absent/0 means Weapon::NONE), but when present it must
# match the source so the client's requirement check keeps working.
EXPECTED_WEAPON = {
    1321011: 43,  # spear, from 1311003
    3121011: 45,  # bow, from 3121003
    3221009: 46,  # crossbow, from 3221001
    4121010: 47,  # claw, from 4121008
    4221009: 33,  # dagger, from 4221007
}
LEVELS = tuple(range(1, 11))


def child_named(node, name):
    """First direct child with a matching name attribute, else None."""
    if node is None:
        return None
    for kid in node:
        if kid.get('name') == name:
            return kid
    return None


def int_child(node, name):
    kid = child_named(node, name)
    if kid is None or kid.tag != 'int':
        return None
    try:
        return int(kid.get('value', ''))
    except (TypeError, ValueError):
        return None


def vector_child(node, name):
    kid = child_named(node, name)
    if kid is None or kid.tag != 'vector':
        return None
    try:
        return (int(kid.get('x', '')), int(kid.get('y', '')))
    except (TypeError, ValueError):
        return None


def string_child(node, name):
    kid = child_named(node, name)
    if kid is None or kid.tag != 'string':
        return None
    return kid.get('value', '')


def int_or(node, name, fallback):
    value = int_child(node, name)
    return fallback if value is None else value


def safe_parse(path):
    """Returns (root, None) or (None, error message)."""
    try:
        return ET.parse(path).getroot(), None
    except OSError as err:
        return None, 'cannot read %s: %s' % (path, err)
    except ET.ParseError as err:
        return None, 'cannot parse %s: %s' % (path, err)


def skill_tree(root, entry):
    """<imgdir skill>/<imgdir id> for one manifest entry."""
    return child_named(child_named(root, 'skill'), str(entry['id']))


def level_mismatches(level, stats):
    """Every stat in `stats` that one <imgdir name=N> level gets wrong."""
    problems = []
    for key in ('attackCount', 'damage', 'mobCount', 'mpCon', 'cooltime',
                'range'):
        if key not in stats:
            # Magic levels scale via mad; damage must stay absent.
            if int_child(level, key) is not None:
                problems.append('%s must be absent' % key)
            continue
        want = stats[key]
        got = int_child(level, key)
        if got != want:
            problems.append('%s want %s got %s' % (key, want, got))
    for key in ('lt', 'rb'):
        want = tuple(stats[key])
        got = vector_child(level, key)
        if got != want:
            problems.append('%s want %s got %s' % (key, want, got))
    want_mad = stats.get('mad', 0)
    got_mad = int_or(level, 'mad', 0)
    if got_mad != want_mad or (want_mad > 0 and got_mad <= 0):
        problems.append('mad want %s got %s' % (want_mad, got_mad))
    want_bullets = stats.get('bulletCount', 1)
    got_bullets = int_or(level, 'bulletCount', 1)
    if got_bullets != want_bullets:
        problems.append('bulletCount want %s got %s'
                        % (want_bullets, got_bullets))
    want_cost = stats.get('bulletConsume', want_bullets)
    got_cost = int_or(level, 'bulletConsume', want_bullets)
    if got_cost != want_cost:
        problems.append('bulletConsume want %s got %s' % (want_cost, got_cost))
    return problems


def check_action(skill, entry, tag):
    """action/0 must be a non-empty string and never the source alert stance."""
    action = child_named(skill, 'action')
    want = entry.get('action')
    if action is None:
        if want:
            return [(False, '%s has action/0 == %s (missing)' % (tag, want))]
        return [(True, '%s has no action child (allowed)' % tag)]
    value = string_child(action, '0')
    results = [
        (bool(value), '%s action/0 is a non-empty string' % tag),
        (value != 'alert2', '%s action/0 is not alert2' % tag),
    ]
    if want:
        results.append((value == want, '%s action/0 == %s' % (tag, want)))
    return results


def check_weapon(skill, entry, tag):
    want = EXPECTED_WEAPON.get(entry['id'])
    got = int_child(skill, 'weapon')
    if want is None:
        return [(got in (None, 0), '%s weapon absent/0' % tag)]
    if got is None:
        return [(True, '%s weapon absent (optional, source %d)'
                 % (tag, want))]
    return [(got == want, '%s weapon == %s' % (tag, want))]


def skill_tag(entry):
    return '%d %s' % (entry['id'], entry['name'])


def check_skill_xml(root, entry):
    """All Skill.wz checks for one target; returns [(ok, message)]."""
    tag = skill_tag(entry)
    results = []
    skill = skill_tree(root, entry)
    results.append((skill is not None,
                    '%s exists at %s/skill' % (tag, entry['img'])))
    if skill is None:
        return results

    levels = child_named(skill, 'level')
    names = [kid.get('name') for kid in list(levels)] if levels is not None \
        else []
    ordered = names == [str(level) for level in LEVELS]
    results.append((ordered, '%s level children are 1..10 in order%s'
                    % (tag, '' if ordered else ' (got %r)' % names)))

    stats = entry['stats']
    for level in LEVELS:
        index = level - 1
        if index >= len(names):
            results.append((False, '%s level %d is missing' % (tag, level)))
            continue
        problems = level_mismatches(list(levels)[index], stats)
        results.append((not problems, '%s level %d stats match the manifest%s'
                        % (tag, level,
                           '' if not problems else ' (%s)'
                           % '; '.join(problems))))

    hit = child_named(skill, 'hit')
    results.append((hit is not None, '%s has hit' % tag))
    results.append((child_named(skill, 'invisible') is None,
                    '%s has no invisible' % tag))
    results.append((child_named(skill, 'skillType') is None,
                    '%s has no skillType' % tag))
    results.extend(check_action(skill, entry, tag))
    results.extend(check_weapon(skill, entry, tag))
    return results


def check_source_xml(root, entry):
    """The pre-existing source skill must survive the graft."""
    tag = 'source %d' % entry['source_id']
    source = child_named(child_named(root, 'skill'), str(entry['source_id']))
    results = [(source is not None,
                '%s exists at %s/skill' % (tag, entry['source_img']))]
    if source is not None:
        results.append((child_named(source, 'hit') is not None,
                        '%s keeps hit' % tag))
    return results


def check_string_xml(root, entry):
    """String.wz Skill.img/<7-digit id> name/desc/h1..h10."""
    skill_id = '%07d' % entry['id']
    tag = 'String.img/%s' % skill_id
    node = child_named(root, skill_id)
    results = [(node is not None, '%s exists' % tag)]
    if node is None:
        return results

    name = string_child(node, 'name')
    desc = string_child(node, 'desc')
    results.append((name == entry['name'],
                    '%s name == %s' % (tag, entry['name'])))
    results.append((desc == entry['desc'],
                    '%s desc matches the manifest' % tag))
    missing = ['h%d' % level for level in LEVELS
               if not string_child(node, 'h%d' % level)]
    results.append((not missing, '%s has non-empty h1..h10%s'
                    % (tag, '' if not missing
                       else ' (missing %s)' % ','.join(missing))))
    return results


def load_manifest(path):
    try:
        with open(path, encoding='utf-8') as handle:
            manifest = json.load(handle)
    except (OSError, ValueError) as err:
        print('wide-attack-server-xml: cannot read manifest %s: %s'
              % (path, err), file=sys.stderr)
        return None
    if not isinstance(manifest, list) or not manifest:
        print('wide-attack-server-xml: manifest %s is empty' % path,
              file=sys.stderr)
        return None
    return manifest


def build_parser():
    parser = argparse.ArgumentParser(
        description='Audit the MapleStory-Server Skill.wz/String.wz XML for '
                    'the 12 wide-attack skills.')
    parser.add_argument('--server', default=str(DEFAULT_SERVER),
                        help='server wz root (default: %(default)s)')
    parser.add_argument('--manifest', default=str(DEFAULT_MANIFEST),
                        help='wide-attack manifest (default: %(default)s)')
    return parser


def main(argv=None):
    args = build_parser().parse_args(argv)
    manifest = load_manifest(args.manifest)
    if manifest is None:
        return 2

    skill_dir = Path(args.server) / 'Skill.wz'
    string_path = Path(args.server) / 'String.wz' / 'Skill.img.xml'

    paths = {string_path}
    for entry in manifest:
        paths.add(skill_dir / (entry['img'] + '.xml'))
        paths.add(skill_dir / (entry['source_img'] + '.xml'))
    trees = {path: safe_parse(path) for path in paths}

    results = []
    for path in sorted(trees):
        root, error = trees[path]
        if root is None:
            results.append((False, error))

    string_root = trees[string_path][0]
    for entry in manifest:
        skill_root = trees[skill_dir / (entry['img'] + '.xml')][0]
        source_root = trees[skill_dir / (entry['source_img'] + '.xml')][0]
        if skill_root is not None:
            results.extend(check_skill_xml(skill_root, entry))
        if source_root is not None:
            results.extend(check_source_xml(source_root, entry))
        if string_root is not None:
            results.extend(check_string_xml(string_root, entry))

    for ok, what in results:
        print(('  ok  ' if ok else ' FAIL ') + what)
    failures = sum(1 for ok, _ in results if not ok)
    print('RESULT: %s (%d checks, %d failed)'
          % ('PASSED' if not failures else 'FAILED', len(results), failures))
    return 0 if not failures else 1


if __name__ == '__main__':
    sys.exit(main())
