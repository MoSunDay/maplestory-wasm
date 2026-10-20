#!/usr/bin/env python3
"""Emit server-side Skill.wz XML for the grafted 1111003/1111004/1111006.

The game server reads ``wz/Skill.wz/<img>.xml`` instead of the client NX, so
after ``inject_skill_nx.py`` the three finishers need matching XML.  Rather
than hand-copying the v83 dump (whose Coma values differ from this dataset),
the blocks are rendered straight out of the post-graft ``assets/Skill.nx``
subtrees: every int/string/vector and every canvas size is therefore the same
on both ends.

Output matches the target file's style: one minified line, no whitespace
between tags, ``<imgdir>``/``<int>``/``<string>``/``<vector>``/``<canvas>``
elements.  Blocks are spliced into ``<imgdir name="skill">`` in numeric order
and all pre-existing bytes are kept verbatim (the untouched text is checked
by removing the inserts again before the file is replaced).

Usage:
    emit_skill_xml.py [--assets-dir assets] [--xml PATH]
"""
from __future__ import annotations

import argparse
import os
import re
import struct
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
for _p in (REPO / 'tools', REPO / 'tools/nx'):
    sys.path.insert(0, str(_p))

from map079_tree import find_path, node_name  # noqa: E402
from merge_skill_nx import parse_nx  # noqa: E402

T_NONE, T_INT, T_REAL, T_STRING, T_VECTOR, T_BITMAP, T_AUDIO = 0, 1, 2, 3, 4, 5, 6
IMG = b'111.img'
SKILL = b'skill'
SKILL_IDS = (1111003, 1111004, 1111006)
SERVER_XML = (REPO / 'link_repos/MapleStory-Server/wz/Skill.wz/111.img.xml')
IMGDIR = re.compile(r'<imgdir\b[^>]*>|</imgdir>')
NAME = re.compile(r'name="([^"]*)"')


def fail(msg):
    sys.exit('emit_skill_xml: ' + msg)


def escape(value):
    return (value.replace('&', '&amp;').replace('<', '&lt;')
            .replace('>', '&gt;').replace('"', '&quot;'))


# --------------------------------------------------------------- NX render

def render(nx, idx, name=None):
    """Render one NX node (and its subtree) as a minified XML element."""
    sid, base, num, typ, union = nx['nodes'][idx]
    tag = (name or nx['strings'][sid].decode('utf-8'))
    if typ == T_NONE:
        kids = ''.join(render(nx, base + k) for k in range(num))
        return '<imgdir name="%s">%s</imgdir>' % (escape(tag), kids)
    if typ == T_INT:
        return '<int name="%s" value="%d"/>' % (escape(tag),
                                                struct.unpack('<q', union)[0])
    if typ == T_STRING:
        value = nx['strings'][struct.unpack_from('<I', union, 0)[0]]
        return '<string name="%s" value="%s"/>' % (escape(tag),
                                                  escape(value.decode('utf-8')))
    if typ == T_VECTOR:
        x, y = struct.unpack('<ii', union)
        return '<vector name="%s" x="%d" y="%d"/>' % (escape(tag), x, y)
    if typ == T_BITMAP:
        _, w, h = struct.unpack('<IHH', union)
        kids = ''.join(render(nx, base + k) for k in range(num))
        return ('<canvas name="%s" width="%d" height="%d">%s</canvas>'
                % (escape(tag), w, h, kids))
    if typ == T_REAL:
        value = struct.unpack('<d', union)[0]
        return '<double name="%s" value="%s"/>' % (escape(tag), repr(value))
    fail('unsupported node type %d at %r' % (typ, tag))


def nx_levels(nx, skill_id):
    """{level: {key: value}} for one skill's level branch."""
    level = find_path(nx, (IMG, SKILL, str(skill_id).encode(), b'level'))
    if level is None:
        fail('Skill.nx has no %d level branch' % skill_id)
    out = {}
    base, num = nx['nodes'][level][1], nx['nodes'][level][2]
    for c in range(base, base + num):
        out[int(node_name(nx, c))] = scalars(nx, c)
    return out


def scalars(nx, idx):
    base, num = nx['nodes'][idx][1], nx['nodes'][idx][2]
    out = {}
    for c in range(base, base + num):
        typ, union = nx['nodes'][c][3], nx['nodes'][c][4]
        name = node_name(nx, c).decode('utf-8')
        if typ == T_INT:
            out[name] = struct.unpack('<q', union)[0]
        elif typ == T_STRING:
            out[name] = nx['strings'][struct.unpack_from('<I', union, 0)[0]].decode('utf-8')
        elif typ == T_VECTOR:
            out[name] = struct.unpack('<ii', union)
    return out


# -------------------------------------------------------- minified XML scan

def matching_end(text, start):
    """Offset just past the </imgdir> that closes the tag at ``start``."""
    depth = 0
    for m in IMGDIR.finditer(text, start):
        tag = m.group(0)
        if tag.startswith('</'):
            depth -= 1
        elif tag.endswith('/>'):
            if depth == 0:
                return m.end()
            continue
        else:
            depth += 1
        if depth == 0:
            return m.end()
    fail('unbalanced <imgdir> at offset %d' % start)


def child_spans(text, parent_start):
    """[(name, start, end)] for the direct <imgdir> children of a node."""
    parent_end = matching_end(text, parent_start)
    spans, pos = [], parent_start
    while True:
        m = IMGDIR.search(text, pos)
        if not m or m.start() >= parent_end:
            return spans
        tag = m.group(0)
        if tag.startswith('</'):
            pos = m.end()
            continue
        if m.start() == parent_start:
            pos = m.end()
            if tag.endswith('/>'):
                return spans
            continue
        if tag.endswith('/>'):
            end = m.end()
        else:
            end = matching_end(text, m.start())
        spans.append((NAME.search(tag).group(1), m.start(), end))
        pos = end


def skill_start(text):
    start = text.find('<imgdir name="skill">')
    if start < 0:
        fail('no <imgdir name="skill"> in the target XML')
    return start


def insert_blocks(text, blocks):
    """Splice [(skill_id, block)] into the skill node in numeric order."""
    start = skill_start(text)
    for skill_id, block in blocks:
        spans = child_spans(text, start)
        if any(name == str(skill_id) for name, _, _ in spans):
            fail('XML already contains %d' % skill_id)
        at = next((s for name, s, _ in spans
                   if name.isdigit() and int(name) > skill_id), None)
        if at is None:
            at = matching_end(text, start) - len('</imgdir>')
        text = text[:at] + block + text[at:]
    return text


# ----------------------------------------------------------------- checking

def xml_levels(elem):
    out = {}
    for branch in elem.findall('imgdir'):
        if branch.get('name') != 'level':
            continue
        for lv in branch.findall('imgdir'):
            values = {}
            for child in lv:
                if child.tag == 'int':
                    values[child.get('name')] = int(child.get('value'))
                elif child.tag == 'string':
                    values[child.get('name')] = child.get('value')
                elif child.tag == 'vector':
                    values[child.get('name')] = (int(child.get('x')),
                                                 int(child.get('y')))
            out[int(lv.get('name'))] = values
    return out


def check(text, original, nx, blocks):
    """XML parses, carries exactly the NX level values and nothing else moved."""
    root = ET.fromstring(text)
    skill = root.find("imgdir[@name='skill']")
    if skill is None:
        fail('rebuilt XML lost the skill node')
    found = {int(e.get('name')): e for e in skill.findall('imgdir')
             if e.get('name', '').isdigit()}
    for skill_id, _ in blocks:
        if skill_id not in found:
            fail('rebuilt XML lost %d' % skill_id)
        want = nx_levels(nx, skill_id)
        got = xml_levels(found[skill_id])
        if got != want:
            diff = [k for k in want if got.get(k) != want[k]]
            fail('%d XML levels diverge from NX at %s' % (skill_id, diff[:3]))
    stripped = text
    for _, block in blocks:
        stripped = stripped.replace(block, '', 1)
    if stripped != original:
        fail('existing XML bytes changed while splicing the new blocks')


# --------------------------------------------------------------------- main

def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--assets-dir', type=Path, default=REPO / 'assets')
    ap.add_argument('--xml', type=Path, default=SERVER_XML)
    args = ap.parse_args(argv)

    nx_path = Path(args.assets_dir) / 'Skill.nx'
    xml_path = Path(args.xml)
    if not nx_path.exists() or not xml_path.exists():
        fail('missing %s or %s' % (nx_path, xml_path))
    nx = parse_nx(nx_path)
    original = xml_path.read_text(encoding='utf-8')

    blocks = []
    for skill_id in SKILL_IDS:
        idx = find_path(nx, (IMG, SKILL, str(skill_id).encode()))
        if idx is None:
            fail('Skill.nx has no %d; run inject_skill_nx.py first' % skill_id)
        blocks.append((skill_id, render(nx, idx)))

    text = insert_blocks(original, blocks)
    check(text, original, nx, blocks)
    tmp = xml_path.with_suffix('.xml.tmp')
    tmp.write_text(text, encoding='utf-8')
    os.replace(tmp, xml_path)

    print('wrote %s: %d -> %d bytes (+%d)'
          % (xml_path, len(original.encode('utf-8')),
             len(text.encode('utf-8')),
             len(text.encode('utf-8')) - len(original.encode('utf-8'))))
    for skill_id, block in blocks:
        levels = nx_levels(nx, skill_id)
        top = levels[max(levels)]
        keys = ('attackCount', 'damage', 'mobCount', 'prop', 'time', 'mpCon', 'hpCon')
        print('  %d: %d bytes, %d levels, lv%d %s'
              % (skill_id, len(block), len(levels), max(levels),
                 ', '.join('%s=%s' % (k, top[k]) for k in keys if k in top)))


if __name__ == '__main__':
    main()
