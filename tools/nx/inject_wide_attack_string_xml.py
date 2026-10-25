#!/usr/bin/env python3
"""Add the 12 wide-attack skills to server String.wz/Skill.img.xml.

Each target becomes an ``<imgdir name="<id>">`` under ``Skill.img`` holding
``name``, ``desc`` and ``h1``..``h10``.  The h-values are generated from the
manifest stats; all ten levels share the same text.  Target entries are
spliced in numeric skill-id slot order and values are XML-escaped (including
apostrophes).

A one-shot ``<file>.pre-wide-attack-backup`` is kept.  A run that finds a
target id already present is refused.  The written file is re-parsed and the
exact inserted blocks are removed again to prove that every pre-existing byte
is unchanged.

Usage:
    inject_wide_attack_string_xml.py [--manifest PATH] [--string-xml PATH]
"""
from __future__ import annotations
import argparse
import os
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
for _p in (REPO / 'tools', REPO / 'tools/nx'):
    sys.path.insert(0, str(_p))
from emit_skill_xml import child_spans, matching_end  # noqa: E402
from wide_attack_common import (  # noqa: E402
    DEFAULT_MANIFEST, DEFAULT_STRING_XML, LEVELS, backup_once, fail,
    find_imgdir_start, load_manifest, read_text, write_text, xml_escape,
)


def level_text(target):
    """One h-value generated from the manifest stats."""
    stats = target['stats']
    if target['kind'] == 'magic':
        # Magic skills scale via mad% (no damage stat in the manifest).
        text = ('MP -%d; Magic Damage %d%% x%d; Monsters %d; '
                'Cooldown %ds; Range %d'
                % (int(stats['mpCon']), int(stats['mad']),
                   int(stats['attackCount']), int(stats['mobCount']),
                   int(stats['cooltime']), int(stats['range'])))
        return text
    text = ('MP -%d; Damage %d%% x%d; Monsters %d; Cooldown %ds; Range %d'
            % (int(stats['mpCon']), int(stats['damage']),
               int(stats['attackCount']), int(stats['mobCount']),
               int(stats['cooltime']), int(stats['range'])))
    if target['kind'] == 'ranged':
        text += '; Bullets %d' % int(stats['bulletCount'])
    return text


def field_values(target):
    h_value = level_text(target)
    return ([('name', target['name']), ('desc', target['desc'])]
            + [('h%d' % number, h_value) for number in LEVELS])


def render_block(target, indent='', newline='\n', pretty=True):
    """Render one <imgdir name="<id>"> entry."""
    target_id = int(target['id'])
    fields = field_values(target)
    if not pretty:
        body = ''.join('<string name="%s" value="%s"/>'
                       % (name, xml_escape(str(value)))
                       for name, value in fields)
        return '<imgdir name="%d">%s</imgdir>' % (target_id, body)
    lines = [indent + '<imgdir name="%d">' % target_id]
    lines.extend(indent + '  <string name="%s" value="%s"/>'
                 % (name, xml_escape(str(value))) for name, value in fields)
    lines.append(indent + '</imgdir>')
    return newline.join(lines) + newline


def sibling_indent(text, start):
    """Indentation of the first existing Skill.img child, or two spaces."""
    spans = child_spans(text, start)
    if spans:
        line_start = text.rfind('\n', 0, spans[0][1]) + 1
        indent = text[line_start:spans[0][1]]
        if not indent.strip():
            return indent
    return '  '


def insertion_point(text, start, skill_id):
    """(offset, indent, newline, pretty) for one numeric slot."""
    spans = child_spans(text, start)
    at = next((span_start for name, span_start, _ in spans
               if name.isdigit() and int(name) > skill_id), None)
    if at is None:
        at = matching_end(text, start) - len('</imgdir>')
    line_start = text.rfind('\n', 0, at) + 1
    if '\n' in text and not text[line_start:at].strip():
        newline = '\r\n' if '\r\n' in text else '\n'
        return line_start, sibling_indent(text, start), newline, True
    return at, '', '\n', False


def insert_target(text, target):
    """Splice one target block and return (new_text, block)."""
    start = find_imgdir_start(text, 'Skill.img')
    at, indent, newline, pretty = insertion_point(text, start, int(target['id']))
    block = render_block(target, indent, newline, pretty)
    return text[:at] + block + text[at:], block


def validate_string_text(text, original, blocks):
    """Targets are present once, parse cleanly, and untouched bytes survive."""
    try:
        root = ET.fromstring(text)
    except ET.ParseError as err:
        fail('rebuilt String XML does not parse: %s' % err)
    if root.tag != 'imgdir' or root.get('name') != 'Skill.img':
        fail('rebuilt String XML lost the Skill.img root')
    for target, block in blocks:
        target_id = int(target['id'])
        matches = [child for child in root.findall('imgdir')
                   if child.get('name') == str(target_id)]
        if len(matches) != 1:
            fail('Skill.img has %d copies of %d' % (len(matches), target_id))
        elem = matches[0]
        names = [child.get('name') for child in elem]
        expected_names = [name for name, _ in field_values(target)]
        if names != expected_names:
            fail('%d: fields are %r; expected %r'
                 % (target_id, names, expected_names))
        values = {}
        for child in elem:
            if child.tag != 'string':
                fail('%d: %s is not a <string>' % (target_id, child.get('name')))
            values[child.get('name')] = child.get('value')
        for name, value in field_values(target):
            if values.get(name) != str(value):
                fail('%d: %s value %r != %r'
                     % (target_id, name, values.get(name), str(value)))
    stripped = text
    positions = []
    for target, block in blocks:
        count = text.count(block)
        if count != 1:
            fail('new block for %d occurs %d times' % (int(target['id']), count))
        positions.append((text.find(block), int(target['id'])))
        stripped = stripped.replace(block, '', 1)
    if stripped != original:
        fail('existing String XML bytes changed while splicing the new blocks')
    if [target_id for _, target_id in sorted(positions)] != sorted(
            int(target['id']) for target, _ in blocks):
        fail('inserted String blocks are not in numeric id order')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--manifest', type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument('--string-xml', type=Path, default=DEFAULT_STRING_XML)
    args = parser.parse_args(argv)

    targets = load_manifest(args.manifest)
    path = Path(args.string_xml)
    if not path.exists():
        fail('missing %s' % path)
    original = read_text(path)
    start = find_imgdir_start(original, 'Skill.img')
    existing = {int(name) for name, _, _ in child_spans(original, start)
                if name.isdigit()}
    for target in targets:
        target_id = int(target['id'])
        if target_id in existing or ('<imgdir name="%d">' % target_id) in original:
            fail('%s already contains skill %d; refusing double insertion'
                 % (path, target_id))

    text, blocks = original, []
    for target in sorted(targets, key=lambda item: int(item['id'])):
        text, block = insert_target(text, target)
        blocks.append((target, block))
    validate_string_text(text, original, blocks)

    bak, created = backup_once(path)
    tmp = path.with_name(path.name + '.tmp')
    write_text(tmp, text)
    ET.fromstring(read_text(tmp))
    os.replace(tmp, path)
    validate_string_text(read_text(path), original, blocks)

    print('%s: %d -> %d bytes (+%d)'
          % (path, len(original.encode('utf-8')), len(text.encode('utf-8')),
             len(text.encode('utf-8')) - len(original.encode('utf-8'))))
    for target, _ in blocks:
        print('  %d: name=%r fields=name,desc,h1..h10 h1=%r'
              % (int(target['id']), target['name'], level_text(target)))
    print('backup: %s%s' % (bak, '' if created else ' (kept)'))


if __name__ == '__main__':
    main()
