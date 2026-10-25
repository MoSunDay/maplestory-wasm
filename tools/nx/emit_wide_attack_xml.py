#!/usr/bin/env python3
"""Emit server Skill.wz XML for the 12 wide-attack skills from assets/Skill.nx.

NX stores a skill's ``level`` children byte-sorted (1, 10, 11, ..., 2, ...).
Server XML wants numeric 1..10; that is the only reordering done here.  Every
other top-level child keeps its NX order.  Blocks are spliced into the
existing ``<imgdir name="skill">`` node without touching existing bytes, a
one-shot ``<file>.pre-wide-attack-backup`` is kept, and a run that finds a
target id already present is refused.

Usage:
    emit_wide_attack_xml.py [--assets-dir DIR] [--manifest PATH]
                            [--server-wz DIR]
"""
from __future__ import annotations
import argparse
import os
import struct
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
for _p in (REPO / 'tools', REPO / 'tools/nx'):
    sys.path.insert(0, str(_p))
from emit_skill_xml import child_spans, matching_end  # noqa: E402
from map079_tree import child_indices, find_path, node_name  # noqa: E402
from merge_skill_nx import parse_nx  # noqa: E402
from wide_attack_common import (  # noqa: E402
    DEFAULT_ASSETS, DEFAULT_MANIFEST, DEFAULT_SERVER_WZ, FORBIDDEN,
    INT_STATS, LEVELS, VECTOR_STATS, backup_once, fail, find_imgdir_start,
    load_manifest, read_text, write_text, xml_escape,
)

T_NONE, T_INT, T_REAL, T_STRING, T_VECTOR, T_BITMAP, T_AUDIO = 0, 1, 2, 3, 4, 5, 6


def nx_child(nx, idx, name):
    return next((c for c in child_indices(nx, idx) if node_name(nx, c) == name), None)


def nx_scalar(nx, idx):
    """int/str/vector/double value of one NX node, else None."""
    typ, union = nx['nodes'][idx][3], nx['nodes'][idx][4]
    if typ == T_INT:
        return struct.unpack('<q', union)[0]
    if typ == T_STRING:
        return nx['strings'][struct.unpack_from('<I', union, 0)[0]].decode('utf-8')
    if typ == T_VECTOR:
        return struct.unpack('<ii', union)
    if typ == T_REAL:
        return struct.unpack('<d', union)[0]
    return None


def render_node(nx, idx, tag=None):
    """Render one NX node and its subtree as minified XML."""
    _, base, num, typ, union = nx['nodes'][idx]
    name = tag if tag is not None else node_name(nx, idx).decode('utf-8')
    kids = ''.join(render_node(nx, base + k) for k in range(num))
    if typ == T_NONE:
        return '<imgdir name="%s">%s</imgdir>' % (xml_escape(name), kids)
    if typ == T_INT:
        return '<int name="%s" value="%d"/>' % (xml_escape(name),
                                                struct.unpack('<q', union)[0])
    if typ == T_STRING:
        value = nx['strings'][struct.unpack_from('<I', union, 0)[0]]
        return '<string name="%s" value="%s"/>' % (
            xml_escape(name), xml_escape(value.decode('utf-8')))
    if typ == T_VECTOR:
        x, y = struct.unpack('<ii', union)
        return '<vector name="%s" x="%d" y="%d"/>' % (xml_escape(name), x, y)
    if typ == T_BITMAP:
        _, width, height = struct.unpack('<IHH', union)
        return '<canvas name="%s" width="%d" height="%d">%s</canvas>' % (
            xml_escape(name), width, height, kids)
    if typ == T_REAL:
        return '<double name="%s" value="%s"/>' % (
            xml_escape(name), repr(struct.unpack('<d', union)[0]))
    if typ == T_AUDIO:
        return '<sound name="%s"/>' % xml_escape(name)
    fail('unsupported NX node type %d at %r' % (typ, name))


def render_levels(nx, idx):
    """Render the level branch with numeric 1..10 child order."""
    entries = []
    for child in child_indices(nx, idx):
        raw = node_name(nx, child).decode('utf-8')
        if not raw.isdigit():
            fail('non-numeric level child %r' % raw)
        entries.append((int(raw), child))
    if sorted(number for number, _ in entries) != list(LEVELS):
        fail('level branch has %s; expected 1..10'
             % sorted(number for number, _ in entries))
    body = ''.join(render_node(nx, child, str(number))
                   for number, child in sorted(entries))
    return '<imgdir name="level">%s</imgdir>' % body


def render_skill(nx, idx, skill_id):
    """Minified <imgdir name="<id>"> block with numeric levels."""
    parts, found_level = [], False
    for child in child_indices(nx, idx):
        if node_name(nx, child) == b'level':
            found_level = True
            parts.append(render_levels(nx, child))
        else:
            parts.append(render_node(nx, child))
    if not found_level:
        fail('%d: NX subtree has no level branch' % skill_id)
    return '<imgdir name="%d">%s</imgdir>' % (skill_id, ''.join(parts))


def nx_level_values(nx, idx):
    level = nx_child(nx, idx, b'level')
    if level is None:
        fail('%d: NX subtree has no level branch' % idx)
    out = {}
    for child in child_indices(nx, level):
        raw = node_name(nx, child).decode('utf-8')
        if not raw.isdigit():
            fail('non-numeric NX level child %r' % raw)
        values = {}
        for grandchild in child_indices(nx, child):
            value = nx_scalar(nx, grandchild)
            if value is not None:
                values[node_name(nx, grandchild).decode('utf-8')] = value
        out[int(raw)] = values
    return out


def check_stats(values, target, where):
    """Every manifest stat must be present with the manifest value."""
    stats, kind = target['stats'], target['kind']
    for key in INT_STATS:
        if key == 'damage' and kind == 'magic':
            # Magic skills scale via mad only; damage must stay absent.
            if 'damage' in values:
                fail('%s: magic level carries damage %r'
                     % (where, values['damage']))
            continue
        if values.get(key) != int(stats[key]):
            fail('%s: %s is %r, manifest wants %s'
                 % (where, key, values.get(key), stats[key]))
    for key in VECTOR_STATS:
        want = tuple(int(v) for v in stats[key])
        if values.get(key) != want:
            fail('%s: %s is %r, manifest wants %r'
                 % (where, key, values.get(key), want))
    if kind == 'magic':
        want = int(stats['mad'])
        if want <= 0 or values.get('mad') != want:
            fail('%s: mad must be %d (> 0), got %r'
                 % (where, want, values.get('mad')))
    elif kind == 'ranged':
        for key in ('bulletCount', 'bulletConsume'):
            if values.get(key) != int(stats[key]):
                fail('%s: %s is %r, manifest wants %s'
                     % (where, key, values.get(key), stats[key]))
    if kind != 'magic' and 'mad' in values:
        fail('%s: non-magic level carries mad %r' % (where, values['mad']))


def validate_nx_target(nx, target, idx):
    """Validate the injected NX subtree and return its XML expectations."""
    target_id = int(target['id'])
    names = [node_name(nx, child).decode('utf-8')
             for child in child_indices(nx, idx)]
    if len(set(names)) != len(names):
        fail('%d: duplicate NX top-level child names' % target_id)
    stack = list(child_indices(nx, idx))
    while stack:
        child = stack.pop()
        if node_name(nx, child).decode('utf-8') in FORBIDDEN:
            fail('%d: forbidden NX child %s'
                 % (target_id, node_name(nx, child).decode('utf-8')))
        stack.extend(child_indices(nx, child))
    if 'hit' not in names:
        fail('%d: NX subtree has no hit branch' % target_id)
    source = (target['source_img'].encode('utf-8'), b'skill',
              str(int(target['source_id'])).encode('ascii'))
    if find_path(nx, source) is None:
        fail('%d: source skill %s/skill/%d is missing from Skill.nx'
             % (target_id, target['source_img'], int(target['source_id'])))
    levels = nx_level_values(nx, idx)
    if sorted(levels) != list(LEVELS):
        fail('%d: NX levels are %s; expected 1..10' % (target_id, sorted(levels)))
    for number in LEVELS:
        check_stats(levels[number], target,
                    '%d level %d (NX)' % (target_id, number))
    weapon_child, weapon = nx_child(nx, idx, b'weapon'), None
    if weapon_child is not None:
        if nx['nodes'][weapon_child][3] != T_INT:
            fail('%d: NX weapon node is not an int' % target_id)
        weapon = nx_scalar(nx, weapon_child)
    return {'names': names, 'levels': levels, 'weapon': weapon}


def insert_block(text, skill_id, block):
    """Splice one block into the skill node at its numeric slot."""
    start = find_imgdir_start(text, 'skill')
    spans = child_spans(text, start)
    if any(name.isdigit() and int(name) == skill_id for name, _, _ in spans):
        fail('skill %d already exists' % skill_id)
    at = next((span_start for name, span_start, _ in spans
               if name.isdigit() and int(name) > skill_id), None)
    if at is None:
        at = matching_end(text, start) - len('</imgdir>')
    return text[:at] + block + text[at:]


def xml_level_values(elem):
    values = {}
    for child in elem:
        name = child.get('name')
        if child.tag == 'int':
            value = int(child.get('value'))
        elif child.tag == 'string':
            value = child.get('value')
        elif child.tag == 'vector':
            value = (int(child.get('x')), int(child.get('y')))
        elif child.tag == 'double':
            value = float(child.get('value'))
        else:
            continue
        if name in values:
            fail('%s: duplicate level field %r' % (elem.get('name'), name))
        values[name] = value
    return values


def check_action(elem, target_id):
    for child in elem:
        if child.get('name') != 'action':
            continue
        values = [child.get('value')] if child.tag == 'string' else []
        values.extend(grand.get('value') for grand in child.iter()
                      if grand is not child and grand.get('value') is not None)
        if any(value.strip().lower() == 'alert2' for value in values):
            fail('%d: action must not be alert2' % target_id)


def validate_xml_target(elem, target, expected):
    target_id = int(target['id'])
    names = [child.get('name') for child in elem]
    if len(set(names)) != len(names):
        fail('%d: duplicate XML top-level child names' % target_id)
    if names != expected['names']:
        fail('%d: XML top-level children %r diverge from NX %r'
             % (target_id, names, expected['names']))
    if 'hit' not in names:
        fail('%d: XML target has no hit branch' % target_id)
    for child in elem.iter():
        if child is not elem and child.get('name') in FORBIDDEN:
            fail('%d: forbidden XML child %s' % (target_id, child.get('name')))
    check_action(elem, target_id)
    levels = [child for child in elem if child.get('name') == 'level']
    if len(levels) != 1 or levels[0].tag != 'imgdir':
        fail('%d: XML target needs exactly one level branch' % target_id)
    level_elems = levels[0].findall('imgdir')
    level_names = [child.get('name') for child in level_elems]
    if level_names != [str(number) for number in LEVELS]:
        fail('%d: XML level order is %r; expected 1..10'
             % (target_id, level_names))
    for number, child in zip(LEVELS, level_elems):
        values, want = xml_level_values(child), expected['levels'][number]
        diff = [key for key in want if values.get(key) != want[key]]
        if diff:
            fail('%d level %d: XML %s diverge from NX'
                 % (target_id, number, diff[:3]))
        check_stats(values, target, '%d level %d (XML)' % (target_id, number))
    weapons = [child for child in elem if child.get('name') == 'weapon']
    if expected['weapon'] is None:
        if weapons:
            fail('%d: XML invents a weapon branch absent from NX' % target_id)
    elif len(weapons) != 1 or weapons[0].tag != 'int':
        fail('%d: XML lost the NX weapon branch' % target_id)
    elif int(weapons[0].get('value')) != expected['weapon']:
        fail('%d: XML weapon %s != NX weapon %s'
             % (target_id, weapons[0].get('value'), expected['weapon']))


def validate_file_text(text, original, blocks, specs, nx):
    """Parse the rebuilt XML, check targets, and prove untouched bytes."""
    try:
        root = ET.fromstring(text)
    except ET.ParseError as err:
        fail('rebuilt XML does not parse: %s' % err)
    skill = root if root.get('name') == 'skill' else root.find("imgdir[@name='skill']")
    if skill is None:
        fail('rebuilt XML lost the skill node')
    for target, expected in specs:
        target_id = int(target['id'])
        matches = [child for child in skill.findall('imgdir')
                   if child.get('name') == str(target_id)]
        if len(matches) != 1:
            fail('rebuilt XML has %d copies of %d' % (len(matches), target_id))
        validate_xml_target(matches[0], target, expected)
        source = (target['source_img'].encode('utf-8'), b'skill',
                  str(int(target['source_id'])).encode('ascii'))
        if find_path(nx, source) is None:
            fail('%d: source skill disappeared from Skill.nx' % target_id)
    stripped = text
    for _, block in blocks:
        if text.count(block) != 1:
            fail('new block occurs %d times in the rebuilt XML' % text.count(block))
        stripped = stripped.replace(block, '', 1)
    if stripped != original:
        fail('existing XML bytes changed while splicing the new blocks')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--assets-dir', type=Path, default=DEFAULT_ASSETS)
    parser.add_argument('--manifest', type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument('--server-wz', type=Path, default=DEFAULT_SERVER_WZ)
    args = parser.parse_args(argv)
    manifest = load_manifest(args.manifest)
    nx_path = Path(args.assets_dir) / 'Skill.nx'
    if not nx_path.exists():
        fail('missing %s' % nx_path)
    nx = parse_nx(nx_path)
    plans, specs = {}, {}
    for target in manifest:
        img, target_id = target['img'], int(target['id'])
        idx = find_path(nx, (img.encode('utf-8'), b'skill',
                             str(target_id).encode('ascii')))
        if idx is None:
            fail('%d missing from %s/skill in Skill.nx; run '
                 'inject_wide_attack_nx.py first' % (target_id, img))
        expected = validate_nx_target(nx, target, idx)
        block = render_skill(nx, idx, target_id)
        xml_path = Path(args.server_wz) / (img + '.xml')
        if not xml_path.exists():
            fail('missing server XML %s' % xml_path)
        plans.setdefault(xml_path, []).append((target_id, block))
        specs[(xml_path, target_id)] = (target, expected)
    prepared = []
    for xml_path in sorted(plans):
        original = read_text(xml_path)
        spans = child_spans(original, find_imgdir_start(original, 'skill'))
        existing = {int(name) for name, _, _ in spans if name.isdigit()}
        for target_id, _ in plans[xml_path]:
            if target_id in existing or ('<imgdir name="%d">' % target_id) in original:
                fail('%s already contains skill %d; refusing double insertion'
                     % (xml_path, target_id))
        text, blocks = original, sorted(plans[xml_path])
        for target_id, block in blocks:
            text = insert_block(text, target_id, block)
        local_specs = [specs[(xml_path, target_id)] for target_id, _ in blocks]
        validate_file_text(text, original, blocks, local_specs, nx)
        prepared.append((xml_path, original, text, blocks, local_specs))
    made = [backup_once(xml_path) for xml_path, _, _, _, _ in prepared]
    for xml_path, original, text, blocks, local_specs in prepared:
        tmp = xml_path.with_name(xml_path.name + '.tmp')
        write_text(tmp, text)
        ET.fromstring(read_text(tmp))
        os.replace(tmp, xml_path)
        validate_file_text(read_text(xml_path), original, blocks,
                           local_specs, nx)
        before, after = len(original.encode('utf-8')), len(text.encode('utf-8'))
        print('%s: %d -> %d bytes (+%d)' % (xml_path, before, after,
                                            after - before))
        for target_id, _ in blocks:
            target = next(t for t, _ in local_specs if int(t['id']) == target_id)
            stats, kind = target['stats'], target['kind']
            damage_key = 'mad' if kind == 'magic' else 'damage'
            extra = ''
            if kind == 'ranged':
                extra = ' bullets=%d/%d' % (int(stats['bulletCount']),
                                            int(stats['bulletConsume']))
            print('  %d: levels=1..10 attackCount=%s %s=%s mobCount=%s '
                  'mpCon=%s cooltime=%s kind=%s%s'
                  % (target_id, stats['attackCount'], damage_key,
                     stats[damage_key],
                     stats['mobCount'], stats['mpCon'], stats['cooltime'],
                     kind, extra))
    print('backups: %s' % ', '.join('%s%s' % (path, '' if created else ' (kept)')
                                    for path, created in made))


if __name__ == '__main__':
    main()
