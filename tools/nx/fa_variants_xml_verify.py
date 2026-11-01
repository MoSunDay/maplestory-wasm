#!/usr/bin/env python3
"""Contract checks for the Final Attack variant server XMLs.

Reads the eight patched ``Skill.wz/*.img.xml`` dumps and asserts every data
contract the patch promises, failing with a non-zero exit on the first
violation:

* all ten Final Attack skills exist, expose exactly 30 level children in
  numeric order (the server indexes them positionally) and carry
  ``attackCount=2``/``mobCount=8`` on every level; the three ranged variants
  (3100001/3200001/13101002) additionally carry ``bulletCount=2`` on every
  level, because the client's ``Attack::RANGED`` path takes the damage-line
  count from ``bulletCount``, while every melee FA skill (11101002 included)
  must stay free of it;
* the three grafted subtrees (1100003/1200002/1300003) match
  ``assets/Skill.nx`` level for level, equal their resident sibling's curve
  (lv1 2 %/105 % -> lv30 60 %/350 %) and keep the lv30 ``lt``/``rb``
  hitbox vectors;
* 11001002/11001003 carry exactly one ``finalAttack`` entry, ``11101002``
  with ``0=30``/``1=40``;
* every XML parses with ``ElementTree`` and, replayed from its one-shot
  ``.pre-fa-variants-backup``, reproduces the current file byte for byte
  (nothing outside the inserted regions moved).
"""
from __future__ import annotations

import sys
import xml.etree.ElementTree as ET
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
for _p in (REPO / 'tools', REPO / 'tools/nx'):
    sys.path.insert(0, str(_p))

from fa_variants_xml_common import (  # noqa: E402
    BACKUP_SUFFIX, GRAFTS, LEVELS, RANGED_COUNTERS, TRIGGER_ENTRY,
    TRIGGER_PROPS, TRIGGERS, apply_with_undo, fa_skills, fail, level_contract,
    nx_levels, plan, read_text, target_images,
)


# ------------------------------------------------------------- verification

def element_levels(node):
    """([level names in document order], {level: {key: value}})."""
    order, values = [], {}
    for branch in node.findall('imgdir'):
        if branch.get('name') != 'level':
            continue
        for lv in branch.findall('imgdir'):
            level = int(lv.get('name'))
            order.append(level)
            current = {}
            for child in lv:
                if child.tag == 'int':
                    current[child.get('name')] = int(child.get('value'))
                elif child.tag == 'string':
                    current[child.get('name')] = child.get('value')
                elif child.tag == 'vector':
                    current[child.get('name')] = (int(child.get('x')),
                                                  int(child.get('y')))
            values[level] = current
    return order, values


def load_docs(wz_dir):
    """{img: (text, <imgdir name="skill"> element)} for every target file."""
    docs = {}
    for img in target_images():
        path = Path(wz_dir) / ('%d.img.xml' % img)
        if not path.exists():
            fail('missing %s' % path)
        text = read_text(path)
        try:
            root = ET.fromstring(text)
        except ET.ParseError as exc:
            fail('%s is not well-formed XML: %s' % (path.name, exc))
        skill = root.find("imgdir[@name='skill']")
        if skill is None:
            fail('%s lost its <imgdir name="skill">' % path.name)
        docs[img] = (text, skill)
    return docs


def check_levels(sid, node):
    """Level order + counters of one FA skill; returns the row for the table.

    Every level carries ``attackCount=2``/``mobCount=8``; the ranged variants
    (3100001/3200001/13101002) additionally carry ``bulletCount=2`` and every
    melee FA skill must stay free of it.
    """
    order, levels = element_levels(node)
    if order != list(range(1, LEVELS + 1)):
        fail('%s: level imgdirs are not in numeric order 1..%d (%s)'
             % (sid, LEVELS, order[:6]))
    contract = level_contract(sid)
    for level, values in levels.items():
        for key, want in contract.items():
            if values.get(key) != want:
                fail('%s lv%d: %s=%r, want %d'
                     % (sid, level, key, values.get(key), want))
        if sid not in RANGED_COUNTERS and 'bulletCount' in values:
            fail('%s lv%d: melee FA must not carry bulletCount'
                 % (sid, level))
    return {'levels': len(levels),
            'lv1': 'prop=%s damage=%s' % (levels[1].get('prop'),
                                          levels[1].get('damage')),
            'lv30': 'prop=%s damage=%s' % (levels[LEVELS].get('prop'),
                                           levels[LEVELS].get('damage'))}


def check_all(wz_dir, nx):
    """Assert every data contract on the on-disk XMLs, return the summary."""
    docs = load_docs(wz_dir)
    rows, grafts, triggers = {}, [], []
    for img, sid in fa_skills():
        node = docs[img][1].find("imgdir[@name='%s']" % sid)
        if node is None:
            fail('%d.img.xml has no skill %s' % (img, sid))
        rows[sid] = dict(check_levels(sid, node), img='%d.img' % img)
    for img, sid, sibling in GRAFTS:
        node = docs[img][1].find("imgdir[@name='%s']" % sid)
        _, levels = element_levels(node)
        want = nx_levels(nx, img, sid)
        if levels != want:
            diff = sorted(set(levels) ^ set(want)) or [
                k for k in want if levels.get(k) != want[k]]
            fail('%s: XML levels diverge from assets/Skill.nx at %s'
                 % (sid, diff[:3]))
        sib = docs[img][1].find("imgdir[@name='%s']" % sibling)
        if element_levels(sib)[1] != levels:
            fail('%s: grafted levels differ from the resident sibling %s'
                 % (sid, sibling))
        for level in (1, LEVELS):
            if levels[level].get('prop') != (2 if level == 1 else 60) or \
                    levels[level].get('damage') != (105 if level == 1 else 350):
                fail('%s lv%d: curve is %r' % (sid, level, levels[level]))
        if 'lt' not in levels[LEVELS] or 'rb' not in levels[LEVELS]:
            fail('%s lv%d: lt/rb hitbox vectors missing' % (sid, LEVELS))
        grafts.append({'id': sid, 'img': '%d.img' % img,
                       'sibling': sibling,
                       'level_keys': len(levels[LEVELS])})
    for img, sid in TRIGGERS:
        node = docs[img][1].find("imgdir[@name='%s']" % sid)
        table = node.find("imgdir[@name='finalAttack']")
        if table is None:
            fail('%s: finalAttack table missing' % sid)
        entries = dict((e.get('name'),
                        dict((c.get('name'), int(c.get('value')))
                             for c in e.findall('int')))
                       for e in table.findall('imgdir'))
        if entries != {TRIGGER_ENTRY: TRIGGER_PROPS}:
            fail('%s: finalAttack table is %r' % (sid, entries))
        triggers.append({'id': sid, 'img': '%d.img' % img,
                         'table': {TRIGGER_ENTRY: TRIGGER_PROPS}})
    return {'fa_skills': len(rows), 'levels': LEVELS, 'rows': rows,
            'grafts': grafts, 'triggers': triggers}


def check_untouched(wz_dir, nx):
    """Re-derive every patched file from its one-shot backup, byte for byte.

    The backup is the exact pre-patch text, so replaying the plan on it must
    reproduce the file currently on disk: anything else means bytes outside
    the inserted regions moved.
    """
    for img in target_images():
        path = Path(wz_dir) / ('%d.img.xml' % img)
        bak = Path(wz_dir) / ('%d.img.xml%s' % (img, BACKUP_SUFFIX))
        if not bak.exists():
            continue
        original = read_text(bak)
        edits, _ = plan(original, nx, img)
        if apply_with_undo(original, edits)[0] != read_text(path):
            fail('%d.img.xml: bytes changed outside the inserted regions'
                 % img)

