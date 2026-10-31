#!/usr/bin/env python3
"""Give the three beginner Nimble Feet skills a 30s cooldown on all data sides.

Targets (see TARGETS): beginner sprint skills 0001002, 10001002, 20001002
(3 levels each, currently cooltime 3600 / 60 / 60).  Cooldowns are
server-driven, so every side mirroring the level data becomes 30:

* ``assets/Skill.nx`` -- each ``level/{1,2,3}/cooltime`` int64 union is
  rewritten in place (size-preserving 8-byte write, no rebuild), like
  patch_nx_int.py; the assets-server reloads on mtime change.
* ``assets/String.nx`` -- ``Skill.img/<sid>/desc`` hints switch to 30
  seconds via a rebuild through the scripts/nx-l10n/apply.py helpers
  (replacements keyed by string index, tmp + os.replace, one-shot
  ``.pre-nimble-feet-cd-backup``).
* ``link_repos/MapleStory-Server/wz/Skill.wz/{000,1000,2000}.img.xml``
  and ``.../String.wz/Skill.img.xml`` -- inside the three skill blocks
  only, every level ``cooltime`` int becomes 30 and the English hint
  says 30 sec; all other bytes untouched (one-shot backups).

A cooltime that is neither the known old value nor 30 aborts the run;
already-30 levels are no-ops (idempotent).  All four sides are re-read
and asserted after writing.

Usage:
    patch_nimble_feet_cooldown_nx.py [--repo-root <repo>] [--dry-run]
"""

import argparse
import mmap
import os
import re
import shutil
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
for _p in (REPO / 'tools/nx', REPO / 'scripts/nx-l10n'):
    sys.path.insert(0, str(_p))

from apply import node_at, read_all_strings, read_header, rebuild  # noqa: E402
from wide_attack_common import read_text, write_text  # noqa: E402

TARGETS = ((0, '0001002'), (1000, '10001002'), (2000, '20001002'))
OLD_COOLTIME = {'0001002': 3600, '10001002': 60, '20001002': 60}
COOLDOWN = 30
LEVELS = 3
BACKUP_SUFFIX = '.pre-nimble-feet-cd-backup'
TYPE_INTEGER = 1
TYPE_STRING = 3
NODE_SIZE = 20
COOLTIME = b'cooltime'
DESC_OLD = '再次使用时间间隔 ： 1分'
DESC_NEW = '再次使用时间间隔 ： 30秒'
DESC_CHECK = DESC_NEW[-3:]
XML_DESC = {
    '0001002': ('Terms between skills : 1 min.', 'Terms between skills : 30 sec.'),
    '10001002': ('Terms between skills : 1 min.', 'Terms between skills : 30 sec.'),
    '20001002': ('Takes about 1 min. until the next call.',
                 'Takes about 30 sec. until the next call.'),
}
XML_DESC_CHECK = '30 sec.'
IMGDIR = re.compile(r'<imgdir\b[^>]*>|</imgdir>')
COOLTIME_INT = re.compile(r'<int\s+name="cooltime"\s+value="(-?\d+)"\s*/>')
INT_TAG = '<int name="cooltime" value="%d"/>'


def fail(msg):
    sys.exit('patch_nimble_feet_cd: ' + msg)


def backup_once(path):
    """Keep one pre-run copy; never overwrite an existing backup."""
    bak = Path(str(path) + BACKUP_SUFFIX)
    if bak.exists():
        return bak, False
    shutil.copy2(path, bak)
    return bak, True


def find_child(data, node_offset, string_offset, children, num, want):
    for k in range(num):
        name_idx = node_at(data, node_offset, children + k)[0]
        off = struct.unpack_from('<Q', data, string_offset + 8 * name_idx)[0]
        length = struct.unpack_from('<H', data, off)[0]
        if bytes(data[off + 2:off + 2 + length]) == want:
            return children + k
    return None


def resolve_path(data, node_offset, string_offset, parts):
    index = 0
    for part in parts:
        _, first, count, _, _ = node_at(data, node_offset, index)
        child = find_child(data, node_offset, string_offset, first, count, part)
        if child is None:
            fail('node not found: %s' % '/'.join(p.decode() for p in parts))
        index = child
    return index


def cooltime_slots(data, node_offset, string_offset, img, sid):
    """[(level, union offset, current, 'ok'|'set')] with sanity checks."""
    parts = [('%03d.img' % img).encode(), b'skill', sid.encode(), b'level']
    root = resolve_path(data, node_offset, string_offset, parts)
    _, first, count, _, _ = node_at(data, node_offset, root)
    if count != LEVELS:
        fail('%s: expected %d level children, saw %d' % (sid, LEVELS, count))
    out = []
    for n in range(1, LEVELS + 1):
        level = find_child(data, node_offset, string_offset,
                           first, count, str(n).encode())
        if level is None:
            fail('%s: no level %d child' % (sid, n))
        _, lfirst, lcount, _, _ = node_at(data, node_offset, level)
        node = find_child(data, node_offset, string_offset,
                          lfirst, lcount, COOLTIME)
        if node is None:
            fail('%s level %d: no cooltime node' % (sid, n))
        base = node_offset + NODE_SIZE * node
        current = struct.unpack_from('<q', data, base + 12)[0]
        if node_at(data, node_offset, node)[3] != TYPE_INTEGER:
            fail('%s level %d: cooltime is not an int' % (sid, n))
        if current not in (COOLDOWN, OLD_COOLTIME[sid]):
            fail('%s level %d: cooltime %d is neither %d nor %d'
                 % (sid, n, current, OLD_COOLTIME[sid], COOLDOWN))
        out.append((n, base + 12, current,
                    'ok' if current == COOLDOWN else 'set'))
    return out


def patch_skill_nx(skill_nx, dry):
    """In-place int64 union writes; returns ((sid, levels, ok, set), ...)."""
    report, writes = [], []
    with open(skill_nx, 'r+b') as handle:
        data = mmap.mmap(handle.fileno(), 0, access=mmap.ACCESS_READ)
        header = read_header(data)
        for img, sid in TARGETS:
            state = cooltime_slots(data, header[1], header[3], img, sid)
            ok = sum(1 for s in state if s[3] == 'ok')
            writes += [s[1] for s in state if s[3] == 'set']
            report.append((sid, len(state), ok, len(state) - ok))
        data.close()
        if writes and not dry:
            for offset in writes:  # 8-byte union, size-preserving write
                handle.seek(offset)
                handle.write(struct.pack('<q', COOLDOWN))
    if writes:
        print('Skill.nx: %s %d int64 union(s) of 8 bytes'
              % ('dry-run, would rewrite' if dry else 'rewrote', len(writes)))
    return report


def verify_skill_nx(skill_nx):
    """Re-read the unions; fail unless every level cooltime is COOLDOWN."""
    with open(skill_nx, 'rb') as handle:
        data = mmap.mmap(handle.fileno(), 0, access=mmap.ACCESS_READ)
        header = read_header(data)
        state = [s for img, sid in TARGETS
                 for s in cooltime_slots(data, header[1], header[3], img, sid)]
        data.close()
    if any(s[2] != COOLDOWN for s in state):
        fail('Skill.nx: a level is not cooltime=%d' % COOLDOWN)
    print('  Skill.nx: %d/%d level cooltime == %d'
          % (len(state), len(TARGETS) * LEVELS, COOLDOWN))


def desc_string_index(data, node_offset, string_offset, sid):
    """String-table index of Skill.img/<sid>/desc (must be a string node)."""
    root = resolve_path(data, node_offset, string_offset,
                        [b'Skill.img', sid.encode()])
    _, first, count, _, _ = node_at(data, node_offset, root)
    node = find_child(data, node_offset, string_offset, first, count, b'desc')
    if node is None:
        fail('%s: no desc node under Skill.img' % sid)
    ntype, value = node_at(data, node_offset, node)[3:]
    if ntype != TYPE_STRING:
        fail('%s: desc is not a string node (type %d)' % (sid, ntype))
    return value


def string_nx_plan(data):
    """({string index: replacement bytes}, [(sid, ok, set), ...])."""
    header = read_header(data)
    strings = read_all_strings(data, header[2], header[3])
    replacements, report = {}, []
    for _, sid in TARGETS:
        index = desc_string_index(data, header[1], header[3], sid)
        text = strings[index]
        if DESC_OLD in text:
            replacements[index] = text.replace(
                DESC_OLD, DESC_NEW).encode('utf-8')
            report.append((sid, 0, 1))
        elif DESC_CHECK in text:
            report.append((sid, 1, 0))
        else:
            fail('%s: desc lacks the expected cooldown sentence' % sid)
    return replacements, report


def patch_string_nx(string_nx, dry):
    """Rebuild via apply.rebuild; returns ((sid, ok, set), ...)."""
    data = Path(string_nx).read_bytes()
    replacements, report = string_nx_plan(data)
    if not replacements:
        return report
    if dry:
        print('String.nx: dry-run, %d desc(s) would be rewritten'
              % len(replacements))
        return report
    bak, _ = backup_once(string_nx)
    out = rebuild(data, *read_header(data), replacements)
    tmp = Path(str(string_nx) + '.tmp')
    tmp.write_bytes(out)
    os.replace(tmp, string_nx)
    print('String.nx: rewrote %d string(s), %d bytes, backup %s'
          % (len(replacements), len(out), bak.name))
    return report


def verify_string_nx(string_nx):
    """Re-plan the descs; fail unless all already mention DESC_CHECK."""
    replacements, report = string_nx_plan(Path(string_nx).read_bytes())
    if replacements:
        fail('String.nx: %d desc(s) do not contain %r'
             % (len(replacements), DESC_CHECK))
    print('  String.nx: %d/%d desc contains %r'
          % (len(report), len(TARGETS), DESC_CHECK))


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


def skill_span(text, sid):
    """(inner, close) of the <imgdir name="<sid>"> block."""
    open_m = re.search(r'<imgdir name="%s">' % sid, text)
    if open_m is None:
        fail('%s: no <imgdir name="%s"> in the XML' % (sid, sid))
    return imgdir_span(text, open_m.start())


def level_span(text, sid):
    """(inner, close) of the skill's level imgdir."""
    s_inner, s_close = skill_span(text, sid)
    lvl_open = text.find('<imgdir name="level">', s_inner, s_close)
    if lvl_open < 0:
        fail('%s: no level imgdir inside the skill' % sid)
    return imgdir_span(text, lvl_open)


def write_xml(path, text, edits, dry):
    """Backup once and apply (start, end, repl) edits back-to-front."""
    if not edits:
        return
    if dry:
        print('  %s: dry-run, nothing written' % path.name)
        return
    out, last = text, len(text) + 1
    for start, end, repl in sorted(edits, reverse=True):
        if start < 0 or end < start or end > last:
            fail('bad or overlapping XML edit at %d' % start)
        out = out[:start] + repl + out[end:]
        last = start
    bak, _ = backup_once(path)
    write_text(path, out)
    print('  %s: wrote %d edit(s), backup %s'
          % (path.name, len(edits), bak.name))


def skill_xml_edits(text, sid):
    """(edits, (already, changed)) for the skill's level cooltime ints."""
    inner, close = level_span(text, sid)
    found = list(COOLTIME_INT.finditer(text, inner, close))
    if len(found) != LEVELS:
        fail('%s: expected %d cooltime int(s) in the level block, saw %d'
             % (sid, LEVELS, len(found)))
    values = [int(m.group(1)) for m in found]
    if sorted(set(values)) not in ([COOLDOWN], [OLD_COOLTIME[sid]],
                                   [OLD_COOLTIME[sid], COOLDOWN]):
        fail('%s: unexpected cooltime value(s) %s' % (sid, values))
    edits = [(m.start(), m.end(), INT_TAG % COOLDOWN) for m in found
             if int(m.group(1)) != COOLDOWN]
    return edits, (len(found) - len(edits), len(edits))


def patch_skill_xml(server_wz, dry):
    """Returns ((img, sid, levels, already, changed), ...)."""
    report = []
    for img, sid in TARGETS:
        path = Path(server_wz) / ('%03d.img.xml' % img)
        if not path.exists():
            fail('missing %s' % path)
        text = read_text(path)
        edits, (ok, changed) = skill_xml_edits(text, sid)
        report.append((img, sid, LEVELS, ok, changed))
        write_xml(path, text, edits, dry)
    return report


def verify_skill_xml(server_wz):
    """Re-plan each XML; fail unless every level is already COOLDOWN."""
    seen, bad = 0, 0
    for img, sid in TARGETS:
        text = read_text(Path(server_wz) / ('%03d.img.xml' % img))
        edits, (ok, changed) = skill_xml_edits(text, sid)
        seen, bad = seen + ok + changed, bad + changed
    if bad:
        fail('Skill.wz XML: %d level(s) not cooltime=%d' % (bad, COOLDOWN))
    print('  Skill.wz XML: %d/%d level cooltime == %d'
          % (seen, len(TARGETS) * LEVELS, COOLDOWN))


def string_xml_edits(text, sid):
    """(edits, (already, changed)) for one desc hint inside the sid block."""
    old, new = XML_DESC[sid]
    inner, close = skill_span(text, sid)
    count = text.count(old, inner, close)
    if count == 0 and text.count(new, inner, close) >= 1:
        return [], (1, 0)
    if count != 1:
        fail('%s: %d occurrence(s) of %r inside the skill block'
             % (sid, count, old))
    at = text.index(old, inner, close)
    return [(at, at + len(old), new)], (0, 1)


def patch_string_xml(path, dry):
    """Returns ((sid, 1, already, changed), ...)."""
    if not path.exists():
        fail('missing %s' % path)
    text = read_text(path)
    report, edits = [], []
    for _, sid in TARGETS:
        side_edits, (ok, changed) = string_xml_edits(text, sid)
        edits.extend(side_edits)
        report.append((sid, 1, ok, changed))
    write_xml(path, text, edits, dry)
    return report


def verify_string_xml(path):
    """Re-plan the hints; fail unless all already mention XML_DESC_CHECK."""
    for _, sid in TARGETS:
        if string_xml_edits(read_text(path), sid)[0]:
            fail('%s: desc does not mention %r' % (sid, XML_DESC_CHECK))
    print('  String.wz XML: %d/%d desc contains %r'
          % (len(TARGETS), len(TARGETS), XML_DESC_CHECK))


def main(argv=None):
    ap = argparse.ArgumentParser(
        description='Set the beginner Nimble Feet cooldowns to %ds' % COOLDOWN)
    ap.add_argument('--repo-root', type=Path, default=REPO,
                    help='repo root holding assets/ and link_repos/')
    ap.add_argument('--dry-run', action='store_true')
    args = ap.parse_args(argv)
    root, wz = args.repo_root, args.repo_root / 'link_repos/MapleStory-Server/wz'
    skill_nx, string_nx = root / 'assets/Skill.nx', root / 'assets/String.nx'
    server_wz, string_xml = wz / 'Skill.wz', wz / 'String.wz/Skill.img.xml'
    for path in (skill_nx, string_nx, string_xml, server_wz):
        if not path.exists():
            fail('missing %s' % path)
    print('Nimble Feet cooldowns (cooltime=%d): %s'
          % (COOLDOWN, ', '.join(sid for _, sid in TARGETS)))
    nx_int = patch_skill_nx(skill_nx, args.dry_run)
    nx_str = patch_string_nx(string_nx, args.dry_run)
    xml_skill = patch_skill_xml(server_wz, args.dry_run)
    xml_string = patch_string_xml(string_xml, args.dry_run)
    if args.dry_run:
        print('dry-run: no files written, skipping post-write verification')
        return
    print('verify:')
    verify_skill_nx(skill_nx)
    verify_skill_xml(server_wz)
    verify_string_nx(string_nx)
    verify_string_xml(string_xml)
    for side, rep in (('Skill.nx', nx_int), ('String.nx', nx_str),
                      ('Skill.wz XML', xml_skill),
                      ('String.wz XML', xml_string)):
        changed = sum(row[-1] for row in rep)
        already = sum(row[-2] for row in rep)
        print('%s: %d node(s) ok (changed %d, already-%d %d)'
              % (side, changed + already, changed, COOLDOWN, already))


if __name__ == '__main__':
    main()
