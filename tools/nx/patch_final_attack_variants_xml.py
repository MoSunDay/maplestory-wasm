#!/usr/bin/env python3
"""Mirror the Final Attack variant grafts into the server's Skill.wz XML.

``inject_final_attack_variants_nx.py`` fixed the client archive
(``assets/Skill.nx``); the Java game server reads the same skills from
``link_repos/MapleStory-Server/wz/Skill.wz/*.img.xml`` and those dumps were
still stale.  This tool applies the matching three edits on the XML side:

* **graft** -- ``110``/``120``/``130.img.xml`` receive the complete
  ``<imgdir name="1100003|1200002|1300003">`` subtree rendered straight out
  of ``assets/Skill.nx`` (hit, three icons, req, skillType and 30 levels
  with the inherited curve: lv1 2 %/105 % -> lv30 60 %/350 %, two hits on
  eight mobs, plus the lv30 ``lt``/``rb`` hitbox vectors).  Level children
  stay in *numeric* order because the server indexes them positionally
  (``Skill.getEffect(level)`` returns ``effects.get(level - 1)``);
* **counters** -- ``attackCount=2``/``mobCount=8`` spliced into every level
  of 3100001, 3200001, 11101002 and 13101002 (all 30 levels; stray values
  are rewritten to 2/8);
* **trigger** -- ``finalAttack`` (one entry ``11101002`` with ``0=30`` and
  ``1=40``) inserted under 11001002 and 11001003 in ``1100.img.xml``, every
  other child preserved.

Output stays minified: each edit is a splice of a rendered block into the
one-line XML, and the patched text is un-edited again to prove the pre-write
bytes are untouched.  One ``.pre-fa-variants-backup`` copy is kept per file
(never overwritten) and serves as the byte-level reference afterwards.  The
files are re-parsed (``ElementTree``) and the data contracts are asserted by
``fa_variants_xml_verify.py``: ten FA skills whose 30 levels each read 2/8,
the three new subtrees matching the archive level for level, both trigger
tables naming 11101002.  A JSON summary is printed and any failed assertion
exits non-zero.

Idempotent: a rerun finds everything in place, writes nothing (same inode
and mtime) and reports ``status: noop``.

Usage:
    patch_final_attack_variants_xml.py [--repo-root <repo>] [--wz-dir DIR]
        [--assets-dir DIR] [--dry-run] [--check]
"""
from __future__ import annotations

import argparse
import json
import os
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
for _p in (REPO / 'tools', REPO / 'tools/nx'):
    sys.path.insert(0, str(_p))

from fa_variants_xml_common import (  # noqa: E402
    BACKUP_SUFFIX, COUNTER_LEVELS, GRAFTS, LEVELS, apply_with_undo,
    backup_once, fail, plan, read_text, stat_of, target_images, write_text,
)
from fa_variants_xml_verify import (  # noqa: E402
    check_all, check_untouched,
)
from merge_skill_nx import parse_nx  # noqa: E402
from patch_final_attack_counts_nx import apply_edits  # noqa: E402


def patch_file(path, nx, img, dry):
    """Plan (and unless dry) apply one file; return its summary record."""
    text = read_text(path)
    before = stat_of(path)
    edits, report = plan(text, nx, img)
    record = dict(report, bytes_before=before['bytes'])
    if not edits:
        print('  %s: already up to date (%d targets verified); no write'
              % (path.name, len(report['grafts']) + len(report['counters'])
                 + len(report['triggers'])))
        return dict(record, status='noop', bytes_after=before['bytes'],
                    inode=before['inode'], mtime_ns=before['mtime_ns'])
    new_text, undo = apply_with_undo(text, edits)
    if apply_edits(new_text, undo) != text:
        fail('%s: splices are not reversible' % path.name)
    try:
        ET.fromstring(new_text)
    except ET.ParseError as exc:
        fail('%s: patched text is not well-formed XML: %s' % (path.name, exc))
    if plan(new_text, nx, img)[0]:
        fail('%s: patched text still schedules edits' % path.name)
    inserted = len(new_text.encode('utf-8')) - len(text.encode('utf-8'))
    if dry:
        print('  %s: dry-run, %d edit(s), %+d bytes, nothing written'
              % (path.name, len(edits), inserted))
        return dict(record, status='dry-run', bytes_after=before['bytes'],
                    inode=before['inode'], mtime_ns=before['mtime_ns'])
    bak, created = backup_once(path)
    tmp = path.with_name(path.name + '.tmp')
    write_text(tmp, new_text)
    os.replace(tmp, path)
    after = stat_of(path)
    print('  %s: %d -> %d bytes (%+d), %d edit(s), backup %s %s'
          % (path.name, before['bytes'], after['bytes'], inserted, len(edits),
             bak.name, 'created' if created else 'kept'))
    return dict(record, status='patched', bytes_after=after['bytes'],
                inode=after['inode'], mtime_ns=after['mtime_ns'],
                backup=bak.name, backup_created=created)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--repo-root', type=Path, default=REPO)
    ap.add_argument('--wz-dir', type=Path, default=None)
    ap.add_argument('--assets-dir', type=Path, default=None)
    ap.add_argument('--dry-run', action='store_true',
                    help='report the pending edits, write nothing')
    ap.add_argument('--check', action='store_true',
                    help='verify only; exit non-zero when edits are pending')
    args = ap.parse_args(argv)

    root = Path(args.repo_root)
    wz_dir = Path(args.wz_dir) if args.wz_dir else (
        root / 'link_repos' / 'MapleStory-Server' / 'wz' / 'Skill.wz')
    skill_nx = (Path(args.assets_dir) if args.assets_dir
                else root / 'assets') / 'Skill.nx'
    if not skill_nx.exists():
        fail('missing %s' % skill_nx)
    nx = parse_nx(skill_nx)
    print('Final Attack variant XML patch: %d files, %d levels, grafts %s'
          % (len(target_images()), LEVELS,
             '/'.join(sid for _, sid, _ in GRAFTS)))

    files = [patch_file(wz_dir / ('%d.img.xml' % img), nx, img,
                        args.dry_run or args.check)
             for img in target_images()]
    pending = sum(len(plan(read_text(wz_dir / ('%d.img.xml' % img)),
                           nx, img)[0]) for img in target_images())
    checks = None
    if pending:
        print('  %d edit(s) pending: contracts are asserted once the files '
              'are patched' % pending)
        if args.check:
            fail('%d edit(s) still pending in %s' % (pending, wz_dir))
    else:
        checks = check_all(wz_dir, nx)
        check_untouched(wz_dir, nx)
        print('  verified: %d FA skills x %d levels at attackCount=%d/'
              'mobCount=%d, %d grafted subtrees match assets/Skill.nx, '
              '%d trigger tables, XMLs parse'
              % (checks['fa_skills'], checks['levels'],
                 COUNTER_LEVELS['attackCount'], COUNTER_LEVELS['mobCount'],
                 len(checks['grafts']), len(checks['triggers'])))
        for sid, row in sorted(checks['rows'].items()):
            print('    %-9s %-9s lv1 %s | lv30 %s'
                  % (row['img'], sid, row['lv1'], row['lv30']))

    status = 'check' if args.check else 'dry-run' if args.dry_run else (
        'patched' if any(r['status'] == 'patched' for r in files) else 'noop')
    summary = {'status': status, 'wz_dir': str(wz_dir),
               'pending_edits': pending, 'backup_suffix': BACKUP_SUFFIX,
               'files': files, 'verified': checks}
    print('summary ' + json.dumps(summary, sort_keys=True))


if __name__ == '__main__':
    main()
