#!/usr/bin/env python3
"""Shared helpers for the Final Attack variant server-XML patch.

Pure functions only, shared by ``patch_final_attack_variants_xml.py`` (the
CLI that writes) and ``fa_variants_xml_verify.py`` (the contracts):

* byte-preserving file I/O, one-shot backups and the stat triple used to
  prove a rerun did not touch a file;
* rendering of one complete ``<imgdir>`` skill subtree straight out of
  ``assets/Skill.nx`` -- with the ``level`` children in *numeric* order,
  because the server indexes them positionally
  (``Skill.getEffect(level)`` returns ``effects.get(level - 1)``);
* minified-XML surgery: locate the ``<imgdir name="skill">`` node and its
  direct children, splice a grafted subtree into numeric slot order, splice
  the ``finalAttack`` table under a skill, and reverse a batch of edits so
  the pre-write bytes can be re-derived.
"""
from __future__ import annotations

import shutil
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
for _p in (REPO / 'tools', REPO / 'tools/nx'):
    sys.path.insert(0, str(_p))

from emit_skill_xml import child_spans, matching_end, render, scalars  # noqa: E402
from map079_tree import child_indices, find_path, node_name  # noqa: E402
from patch_final_attack_counts_nx import xml_edits  # noqa: E402

SERVER_WZ = REPO / 'link_repos' / 'MapleStory-Server' / 'wz' / 'Skill.wz'
BACKUP_SUFFIX = '.pre-fa-variants-backup'
LEVELS = 30
COUNTER_LEVELS = {'attackCount': 2, 'mobCount': 8}
# (img, grafted skill, resident sibling whose level curve the graft inherited)
GRAFTS = ((110, '1100003', '1100002'), (120, '1200002', '1200003'),
          (130, '1300003', '1300002'))
# Archer / Cygnus FA skills that exist but omit the level counters.
COUNTERS = ((310, '3100001'), (320, '3200001'), (1110, '11101002'),
            (1310, '13101002'))
# 1st-job Dawn Warrior attacks that lost their FA trigger table.
TRIGGERS = ((1100, '11001002'), (1100, '11001003'))
TRIGGER_ENTRY = '11101002'
TRIGGER_PROPS = {'0': 30, '1': 40}
TRIGGER_BLOCK = ('<imgdir name="finalAttack"><imgdir name="%s">'
                 '<int name="0" value="30"/><int name="1" value="40"/>'
                 '</imgdir></imgdir>' % TRIGGER_ENTRY)


def fail(msg):
    sys.exit('fa_variants_xml: ' + str(msg))


# ------------------------------------------------------------- file plumbing

def read_text(path):
    """Read UTF-8 without universal-newline translation."""
    return Path(path).read_bytes().decode('utf-8')


def write_text(path, text):
    """Write UTF-8 bytes without newline translation."""
    Path(path).write_bytes(text.encode('utf-8'))


def backup_once(path):
    """Keep one pre-run copy; never overwrite an existing backup."""
    path = Path(path)
    bak = path.with_name(path.name + BACKUP_SUFFIX)
    if bak.exists():
        return bak, False
    shutil.copy2(path, bak)
    return bak, True


def stat_of(path):
    """Size/inode/mtime triple used to prove a rerun did not touch a file."""
    st = Path(path).stat()
    return {'bytes': st.st_size, 'inode': st.st_ino, 'mtime_ns': st.st_mtime_ns}


def fa_skills():
    """(img, skill id) of all ten Final Attack skills, both data sides."""
    out = [(img, sid) for img, sid, _ in GRAFTS]
    out += [(img, sib) for img, _, sib in GRAFTS]
    return out + list(COUNTERS)


def target_images():
    """Image numbers owning at least one edit, ascending."""
    return sorted({img for img, _ in fa_skills()} | {img for img, _ in TRIGGERS})


# ---------------------------------------------------------------- NX render

def nx_skill(nx, img, sid):
    """Node index of <img>/skill/<sid> in the client archive."""
    idx = find_path(nx, (('%d.img' % img).encode(), b'skill', sid.encode()))
    if idx is None:
        fail('assets/Skill.nx has no %d.img/skill/%s; run '
             'inject_final_attack_variants_nx.py first' % (img, sid))
    return idx


def nx_levels(nx, img, sid):
    """{level: {key: value}} straight from the client archive."""
    path = (('%d.img' % img).encode(), b'skill', sid.encode(), b'level')
    idx = find_path(nx, path)
    if idx is None:
        fail('%d.img/skill/%s has no level branch' % (img, sid))
    return dict((int(node_name(nx, c)), scalars(nx, c))
                for c in child_indices(nx, idx))


def render_skill(nx, img, sid):
    """Minified <imgdir> subtree with the level children in numeric order."""
    parts = []
    for child in child_indices(nx, nx_skill(nx, img, sid)):
        if node_name(nx, child) != b'level':
            parts.append(render(nx, child))
            continue
        levels = sorted(child_indices(nx, child),
                        key=lambda c: int(node_name(nx, c)))
        parts.append('<imgdir name="level">%s</imgdir>'
                     % ''.join(render(nx, lv) for lv in levels))
    return '<imgdir name="%s">%s</imgdir>' % (sid, ''.join(parts))


# ----------------------------------------------------------------- XML cuts

def skill_parent(text):
    """Offset of the <imgdir name="skill"> node of one image dump."""
    start = text.find('<imgdir name="skill">')
    if start < 0:
        fail('no <imgdir name="skill"> in the target XML')
    return start


def skill_span(text, sid):
    """(start, end) of the <imgdir name="sid"> skill node."""
    parent = skill_parent(text)
    for name, start, end in child_spans(text, parent):
        if name == sid:
            return start, end
    fail('%s: no such skill imgdir' % sid)


def insert_offset(text, parent, target_id):
    """Offset keeping the skill ids in numeric slot order."""
    for name, start, _ in child_spans(text, parent):
        if name.isdigit() and int(name) > target_id:
            return start
    return matching_end(text, parent) - len('</imgdir>')


def graft_edits(text, sid, block):
    """(edits, state) inserting one complete skill subtree."""
    parent = skill_parent(text)
    for name, start, end in child_spans(text, parent):
        if name != sid:
            continue
        if text[start:end] != block:
            fail('%s: existing subtree differs from the NX rendering; '
                 'refusing to overwrite' % sid)
        return [], 'present'
    at = insert_offset(text, parent, int(sid))
    return [(at, at, block)], 'inserted'


def trigger_edits(text, sid):
    """(edits, state) for the finalAttack table of one skill."""
    start, end = skill_span(text, sid)
    kids = child_spans(text, start)
    for name, k_start, k_end in kids:
        if name == 'finalAttack':
            if text[k_start:k_end] != TRIGGER_BLOCK:
                fail('%s: existing finalAttack is not the %s table'
                     % (sid, TRIGGER_ENTRY))
            return [], 'present'
    at = next((k_start for name, k_start, _ in kids if name == 'level'),
              end - len('</imgdir>'))
    return [(at, at, TRIGGER_BLOCK)], 'inserted'


def plan(text, nx, img):
    """(edits, report) for one <img>.img.xml, all offsets on the input text."""
    edits = []
    report = {'file': '%d.img.xml' % img, 'grafts': [], 'counters': [],
              'triggers': []}
    for graft_img, sid, _ in GRAFTS:
        if graft_img == img:
            new, state = graft_edits(text, sid, render_skill(nx, img, sid))
            edits += new
            report['grafts'].append({'id': sid, 'state': state})
    for counter_img, sid in COUNTERS:
        if counter_img == img:
            new, (inserted, fixed, ok) = xml_edits(text, int(sid))
            edits += new
            report['counters'].append({'id': sid, 'inserted': inserted,
                                       'fixed': fixed, 'ok': ok})
    for trigger_img, sid in TRIGGERS:
        if trigger_img == img:
            new, state = trigger_edits(text, sid)
            edits += new
            report['triggers'].append({'id': sid, 'state': state})
    report['edits'] = len(edits)
    return edits, report


def apply_with_undo(text, edits):
    """Apply (start, end, repl) edits, also returning the inverse edits.

    The inverse list is expressed in *patched* coordinates and replays the
    original slices back (apply it back-to-front, e.g. via ``apply_edits``):
    ``apply_edits(new_text, undo) == text`` proves nothing but the intended
    spans moved.  Edits may insert (``start == end``) or replace.
    """
    chunks, undo, pos, shift = [], [], 0, 0
    for start, end, repl in sorted(edits):
        if start < pos or end < start or end > len(text):
            fail('bad or overlapping XML edit at %d' % start)
        chunks.append(text[pos:start])
        chunks.append(repl)
        at = start + shift
        undo.append((at, at + len(repl), text[start:end]))
        shift += len(repl) - (end - start)
        pos = end
    chunks.append(text[pos:])
    return ''.join(chunks), undo
