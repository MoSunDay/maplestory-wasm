#!/usr/bin/env python3
"""Verify the BgmCN.img / BgmTH.img injection in the client Sound.nx.

Read-only counterpart of ``tools/nx/inject_orient_sound_nx.py``; safe to run
in CI against the shipped asset.  It asserts:

1. the root has exactly the 44 original wrappers plus BgmCN.img/BgmTH.img,
   each original name exactly once and the slot block bytewise sorted;
2. the archive holds exactly 3966 + 18 audio blobs;
3. both wrappers are folder nodes whose direct children match the expected
   CN/TH track names in order (no ``.img`` suffix), sorted bytewise;
4. every wrapper child is an audio node (type 6) with union length > 82, a
   distinct appended audio index and a blob of exactly that length;
5. ``Sound.nx -> BgmCN.img -> GoShanghai``, ``BgmCN.img/ShanghaiField`` and
   ``BgmTH.img/ThaiTown`` resolve (checked in the report).

Usage:
    verify_orient_sound_nx.py [--sound-nx assets/Sound.nx] [--report]
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'nx'))
from inject_orient_sound_nx import (  # noqa: E402
    AUDIO_HEADER, ORIGINAL_AUDIO_COUNT, ORIGINAL_ROOT_CHILDREN,
    WRAPPER_CHILDREN, report, root_child_names, wrapper_children,
)
from merge_skill_nx import T_AUDIO, parse_nx  # noqa: E402

EXPECTED_ROOT_COUNT = len(ORIGINAL_ROOT_CHILDREN) + len(WRAPPER_CHILDREN)
EXPECTED_NEW_AUDIOS = sum(len(names) for names in WRAPPER_CHILDREN.values())
EXPECTED_AUDIO_COUNT = ORIGINAL_AUDIO_COUNT + EXPECTED_NEW_AUDIOS


def check_archive(nx):
    errors = []
    names = root_child_names(nx)
    if len(names) != EXPECTED_ROOT_COUNT:
        errors.append('root has %d children, expected %d'
                      % (len(names), EXPECTED_ROOT_COUNT))
    for name in ORIGINAL_ROOT_CHILDREN:
        count = names.count(name)
        if count != 1:
            errors.append('original root child %r present %d times'
                          % (name, count))
    for wrapper in WRAPPER_CHILDREN:
        count = names.count(wrapper)
        if count != 1:
            errors.append('wrapper %r present %d times'
                          % (wrapper, count))
    if len(set(names)) != len(names):
        errors.append('duplicate root child names')
    if names != sorted(names):
        errors.append('root children not sorted bytewise')
    if len(nx['audios']) != EXPECTED_AUDIO_COUNT:
        errors.append('audio blobs %d, expected %d'
                      % (len(nx['audios']), EXPECTED_AUDIO_COUNT))
    used_audio_indices = []
    for wrapper, expected_names in WRAPPER_CHILDREN.items():
        children = wrapper_children(nx, wrapper)
        if children is None:
            errors.append('missing root wrapper %r' % wrapper)
            continue
        got_names = [nx['strings'][child[0]] for child in children]
        if got_names != list(expected_names):
            errors.append('%r children %r != expected %r'
                          % (wrapper, got_names, list(expected_names)))
        if got_names != sorted(got_names):
            errors.append('%r children not sorted bytewise' % wrapper)
        for child in children:
            name = nx['strings'][child[0]]
            label = '%r/%r' % (wrapper, name)
            if child[3] != T_AUDIO:
                errors.append('%s is not an audio node' % label)
                continue
            audio_idx, length = struct.unpack('<II', child[4])
            if length <= AUDIO_HEADER:
                errors.append('%s audio length %d <= %d'
                              % (label, length, AUDIO_HEADER))
            if audio_idx < ORIGINAL_AUDIO_COUNT:
                errors.append('%s reuses existing audio index %d'
                              % (label, audio_idx))
            elif audio_idx >= len(nx['audios']):
                errors.append('%s audio index %d out of range'
                              % (label, audio_idx))
            elif len(nx['audios'][audio_idx]) != length:
                errors.append('%s blob is %d bytes, union says %d'
                              % (label, len(nx['audios'][audio_idx]), length))
            else:
                used_audio_indices.append(audio_idx)
    expected_indices = list(range(ORIGINAL_AUDIO_COUNT, EXPECTED_AUDIO_COUNT))
    if sorted(used_audio_indices) != expected_indices:
        errors.append('wrapper audio indices %r != appended range %r'
                      % (sorted(used_audio_indices), expected_indices))
    return errors


def main(argv=None):
    parser = argparse.ArgumentParser(
        description='Verify BgmCN.img/BgmTH.img in Sound.nx (read-only).')
    parser.add_argument('--sound-nx', default='assets/Sound.nx')
    parser.add_argument('--report', action='store_true',
                        help='print root neighbours, wrapper children and paths')
    args = parser.parse_args(argv)
    path = Path(args.sound_nx)
    if not path.is_file():
        sys.exit('verify_orient_sound_nx: missing archive: %s' % path)
    nx = parse_nx(str(path))
    errors = check_archive(nx)
    if errors:
        print('FAILED: %d error(s)' % len(errors))
        for err in errors[:20]:
            print('  - %s' % err)
        sys.exit(1)
    print('%s: OK (%d root children, %d audios, %d nodes)'
          % (path, len(root_child_names(nx)), len(nx['audios']),
             len(nx['nodes'])))
    if args.report:
        report(nx, list(WRAPPER_CHILDREN))
    print('ALL CHECKS PASSED')


if __name__ == '__main__':
    main(sys.argv[1:])
