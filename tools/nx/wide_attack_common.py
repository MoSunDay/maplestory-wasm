#!/usr/bin/env python3
"""Shared helpers for the wide-attack NX/XML tools.

Pure functions only: manifest loading, XML escaping, byte-preserving file I/O,
one-shot backups, and direct-child scanning of minified server XML.
"""
from __future__ import annotations
import json
import re
import shutil
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEFAULT_ASSETS = REPO / 'assets'
DEFAULT_MANIFEST = REPO / 'tools/nx/wide_attack_manifest.json'
DEFAULT_SERVER_WZ = REPO / 'link_repos/MapleStory-Server/wz/Skill.wz'
DEFAULT_STRING_XML = REPO / 'link_repos/MapleStory-Server/wz/String.wz/Skill.img.xml'
LEVELS = tuple(range(1, 11))
INT_STATS = ('attackCount', 'damage', 'mobCount', 'mpCon', 'cooltime', 'range')
VECTOR_STATS = ('lt', 'rb')
FORBIDDEN = ('invisible', 'skillType')
BACKUP_SUFFIX = '.pre-wide-attack-backup'
IMGDIR = re.compile(r'<imgdir\b[^>]*>|</imgdir>')
NAME = re.compile(r'name="([^"]*)"')


def fail(msg):
    sys.exit('wide_attack_xml: ' + msg)


def xml_escape(value):
    """Escape an XML 1.0 attribute value, including apostrophes."""
    return (value.replace('&', '&amp;').replace('<', '&lt;')
            .replace('>', '&gt;').replace('"', '&quot;')
            .replace("'", '&apos;').replace('\r', '&#13;')
            .replace('\n', '&#10;').replace('\t', '&#9;'))


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


def load_manifest(path):
    """Read and lightly validate the wide-attack manifest."""
    path = Path(path)
    if not path.exists():
        fail('missing manifest %s' % path)
    try:
        data = json.loads(read_text(path))
    except ValueError as err:
        fail('cannot read manifest %s: %s' % (path, err))
    if not isinstance(data, list) or not data:
        fail('%s: expected a non-empty JSON list' % path)
    required = ('job', 'img', 'id', 'source_id', 'source_img', 'name', 'desc',
                'kind', 'stats')
    seen = set()
    for pos, target in enumerate(data):
        missing = [key for key in required
                   if not isinstance(target, dict) or key not in target]
        if missing:
            fail('%s[%d]: missing %s' % (path, pos, ', '.join(missing)))
        job, sid, stats = int(target['job']), int(target['id']), target['stats']
        if target['img'] != '%d.img' % job:
            fail('%s[%d]: img must be %d.img' % (path, pos, job))
        if sid in seen:
            fail('%s[%d]: duplicate skill id %d' % (path, pos, sid))
        seen.add(sid)
        if target['kind'] not in ('melee', 'magic', 'ranged'):
            fail('%s[%d]: unknown kind %r' % (path, pos, target['kind']))
        if not isinstance(stats, dict):
            fail('%s[%d]: stats must be an object' % (path, pos))
        for key in INT_STATS:
            # damage is the physical-attack stat; magic skills scale via mad.
            if key == 'damage' and target['kind'] == 'magic':
                if key in stats:
                    fail('%s[%d]: magic skill must not carry stats.%s'
                         % (path, pos, key))
                continue
            if key not in stats:
                fail('%s[%d]: stats.%s missing' % (path, pos, key))
            int(stats[key])
        for key in VECTOR_STATS:
            if key not in stats or len(stats[key]) != 2:
                fail('%s[%d]: stats.%s must be [x, y]' % (path, pos, key))
            int(stats[key][0]), int(stats[key][1])
        if target['kind'] == 'magic' and int(stats.get('mad', 0)) <= 0:
            fail('%s[%d]: magic skill needs stats.mad > 0' % (path, pos))
        if target['kind'] == 'ranged':
            for key in ('bulletCount', 'bulletConsume'):
                if key not in stats:
                    fail('%s[%d]: ranged skill needs stats.%s' % (path, pos, key))
                int(stats[key])
    return data


def find_imgdir_start(text, name):
    """Offset of the first opening <imgdir name="..."> tag."""
    for match in IMGDIR.finditer(text):
        tag = match.group(0)
        found = NAME.search(tag)
        if not tag.startswith('</') and found and found.group(1) == name:
            return match.start()
    fail('no <imgdir name="%s"> in the target XML' % name)
