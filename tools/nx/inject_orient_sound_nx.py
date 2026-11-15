#!/usr/bin/env python3
"""Add the missing Chinese/Thai BGM trees to the client ``Sound.nx`` archive.

The v83 archive ships ``Bgm00.img``..``Bgm21.img`` plus ``BgmEvent``/``GL``/
``Jp``/``MY``/``SG``, but the 079 Shanghai/Shaolin (CN) and Thailand (TH)
trees are absent, so map music such as ``BgmCN.img/ShanghaiField`` cannot be
resolved.  This tool appends the two converted per-image NX trees as root
wrappers ``BgmCN.img`` and ``BgmTH.img``.

Layout strategy (no existing node index or audio index moves):

* node 0 (the root) keeps its index; the wrapper and its remapped source
  nodes are appended at the end of the node table;
* existing root child descriptor tuples are copied into a fresh contiguous
  block at the end of the node table, the two new wrapper descriptors are
  inserted at their bytewise sorted positions (nlnx binary-searches children)
  and node 0 is repointed at the new block;
* source string ids are interned into the destination string table, source
  audio/bitmap blobs are appended and their union indices remapped, so no
  existing audio payload is duplicated, reordered or rewritten.

The rewritten image is first verified in memory, the archive is backed up to
``<sound-nx><backup-suffix>``, written through a temp file + ``os.replace``
and then reparsed to verify the final file.  Reruns are refused as soon as
either wrapper already exists (the backup path is also refused if present).

Usage:
    inject_orient_sound_nx.py [--sound-nx assets/Sound.nx]
                              [--bgm-cn /tmp/orient-sound/BgmCN.nx]
                              [--bgm-th /tmp/orient-sound/BgmTH.nx]
                              [--backup-suffix .pre-orient-sound-backup]
"""
from __future__ import annotations

import argparse
import os
import shutil
import struct
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from merge_skill_nx import (  # noqa: E402
    T_AUDIO, build, find_child, intern, parse_nx, remap_union,
)

AUDIO_HEADER = 82          # client-side audio header included in union length
ORIGINAL_AUDIO_COUNT = 3966
ORIGINAL_ROOT_CHILDREN = tuple(
    [b'Bgm%02d.img' % i for i in range(22)]
    + [b'BgmEvent.img', b'BgmGL.img', b'BgmJp.img', b'BgmMY.img',
       b'BgmSG.img', b'BgmUI.img', b'CashEffect.img', b'ConsumeEffect.img',
       b'Field.img', b'Game.img', b'Item.img', b'Jukebox.img',
       b'MiniGame.img', b'Mob.img', b'Object.img', b'Pet.img',
       b'Radio.img', b'Reactor.img', b'Skill.img', b'Summon.img',
       b'UI.img', b'Weapon.img'])
WRAPPER_CHILDREN = {
    b'BgmCN.img': (
        b'EnSchool', b'GoShanghai', b'PvP', b'ShanghaiField', b'Yuyuan',
        b'shaolin_cangjingge', b'shaolin_daxiongbaodian', b'shaolin_mishi',
        b'shaolin_shan', b'shaolin_songshan', b'summerboating01',
        b'summerboating02', b'summerboating03'),
    b'BgmTH.img': (
        b'ThaiField', b'ThaiTown', b'goldTempleDungeonTH',
        b'goldTempleFieldTH', b'goldTempleTownTH'),
}


def fail(msg):
    sys.exit('inject_orient_sound_nx: ' + msg)


def root_child_names(nx):
    root = nx['nodes'][0]
    return [nx['strings'][nx['nodes'][root[1] + i][0]]
            for i in range(root[2])]


def wrapper_children(nx, wrapper):
    idx = find_child(nx, wrapper)
    if idx is None:
        return None
    node = nx['nodes'][idx]
    return [nx['nodes'][node[1] + k] for k in range(node[2])]


def with_source(state, src, wrapper_name):
    """Append one converted source tree as a wrapper node.

    The source root (node 0) becomes the wrapper; source nodes 1..N-1 keep
    their relative child indices, remapped onto the destination node table.
    Returns ``(new_state, wrapper_node_index)`` and never mutates ``state``.
    """
    strings = list(state['strings'])
    index = {s: i for i, s in enumerate(strings)}
    nodes = list(state['nodes'])
    bitmaps = list(state['bitmaps'])
    audios = list(state['audios'])
    src_nodes = src['nodes']
    base = len(nodes)
    str_map = [intern(strings, index, s) for s in src['strings']]
    bmp_map = []
    for blob in src['bitmaps']:
        bmp_map.append(len(bitmaps))
        bitmaps.append(blob)
    aud_map = []
    for blob in src['audios']:
        aud_map.append(len(audios))
        audios.append(blob)
    root = src_nodes[0]
    nodes.append((intern(strings, index, wrapper_name),
                  base + root[1] if root[2] else 0,
                  root[2], 0, b'\0' * 8))
    for name, children, num, typ, union in src_nodes[1:]:
        nodes.append((str_map[name], base + children if num else 0, num, typ,
                      remap_union(union, typ, str_map, bmp_map, aud_map)))
    return ({'strings': strings, 'nodes': nodes, 'bitmaps': bitmaps,
             'audios': audios}, base)


def with_sorted_root_slots(state, wrapper_indices):
    """Copy root child slots into a new sorted contiguous block at the end."""
    nodes, strings = state['nodes'], state['strings']
    root = nodes[0]
    slots = [nodes[root[1] + i] for i in range(root[2])]
    slots += [nodes[i] for i in wrapper_indices]
    slots.sort(key=lambda node: strings[node[0]])
    block = len(nodes)
    new_nodes = nodes + slots
    new_nodes[0] = (root[0], block, len(slots), root[3], root[4])
    return {'strings': strings, 'nodes': new_nodes,
            'bitmaps': state['bitmaps'], 'audios': state['audios']}


def check_archive(nx, baseline_names, sources, baseline_audios):
    """Return a list of invariant violations for an injected archive."""
    errors = []
    names = root_child_names(nx)
    expected_count = len(baseline_names) + len(sources)
    if len(names) != expected_count:
        errors.append('root has %d children, expected %d'
                      % (len(names), expected_count))
    for name in baseline_names:
        count = names.count(name)
        if count != 1:
            errors.append('original root child %r present %d times'
                          % (name, count))
    if len(set(names)) != len(names):
        errors.append('duplicate root child names')
    if names != sorted(names):
        errors.append('root children not sorted bytewise')
    expected_audios = baseline_audios + sum(len(src['audios'])
                                            for _, src in sources)
    if len(nx['audios']) != expected_audios:
        errors.append('audio count %d, expected %d'
                      % (len(nx['audios']), expected_audios))
    for wrapper, src in sources:
        idx = find_child(nx, wrapper)
        if idx is None:
            errors.append('missing root wrapper %r' % wrapper)
            continue
        node = nx['nodes'][idx]
        src_root = src['nodes'][0]
        src_names = [src['strings'][src['nodes'][src_root[1] + k][0]]
                     for k in range(src_root[2])]
        children = wrapper_children(nx, wrapper)
        got_names = [nx['strings'][child[0]] for child in children]
        if node[3] != 0:
            errors.append('%r is not a folder node' % wrapper)
        if node[2] != src_root[2]:
            errors.append('%r has %d children, source has %d'
                          % (wrapper, node[2], src_root[2]))
        if got_names != src_names:
            errors.append('%r children %r != source %r'
                          % (wrapper, got_names, src_names))
        if got_names != sorted(got_names):
            errors.append('%r children not sorted bytewise' % wrapper)
        for name, child in zip(got_names, children):
            if child[3] != T_AUDIO:
                errors.append('%r/%r is not an audio node'
                              % (wrapper, name))
                continue
            audio_idx, length = struct.unpack('<II', child[4])
            label = '%r/%r' % (wrapper, name)
            if length <= AUDIO_HEADER:
                errors.append('%s audio length %d <= %d'
                              % (label, length, AUDIO_HEADER))
            if audio_idx < baseline_audios:
                errors.append('%s reuses existing audio index %d'
                              % (label, audio_idx))
            if audio_idx >= len(nx['audios']):
                errors.append('%s audio index %d out of range'
                              % (label, audio_idx))
            elif len(nx['audios'][audio_idx]) != length:
                errors.append('%s blob is %d bytes, union says %d'
                              % (label, len(nx['audios'][audio_idx]), length))
    return errors


def resolve_path(nx, wrapper, child_name):
    """Resolve ``Sound.nx -> <wrapper> -> <child_name>`` like the client does."""
    idx = find_child(nx, wrapper)
    if idx is None:
        return None
    node = nx['nodes'][idx]
    for k in range(node[2]):
        child = nx['nodes'][node[1] + k]
        if nx['strings'][child[0]] == child_name:
            return child
    return None


def report(nx, wrappers):
    names = root_child_names(nx)
    for wrapper in wrappers:
        pos = names.index(wrapper)
        lo, hi = max(0, pos - 2), min(len(names), pos + 3)
        print('root children %d..%d: %s'
              % (lo, hi - 1, ', '.join(
                  '%d:%s' % (i, names[i].decode()) for i in range(lo, hi))))
    for wrapper in wrappers:
        children = wrapper_children(nx, wrapper)
        print('%s: %d children' % (wrapper.decode(), len(children)))
        for child in children:
            name = nx['strings'][child[0]]
            audio_idx, length = struct.unpack('<II', child[4])
            print('  %-24s audio[%d] %d bytes (> %d)'
                  % (name.decode(), audio_idx, length, AUDIO_HEADER))
    for wrapper, child_name in ((b'BgmCN.img', b'GoShanghai'),
                                (b'BgmCN.img', b'ShanghaiField'),
                                (b'BgmTH.img', b'ThaiTown')):
        child = resolve_path(nx, wrapper, child_name)
        status = 'resolved' if child is not None else 'MISSING'
        print('path %s/%s: %s'
              % (wrapper.decode(), child_name.decode(), status))


def write_archive(path, blob, backup_suffix):
    backup = Path(str(path) + backup_suffix)
    if backup.exists():
        fail('backup already exists, refusing to overwrite it: %s' % backup)
    mode = path.stat().st_mode & 0o777
    shutil.copy2(path, backup)
    fd, tmp_name = tempfile.mkstemp(prefix=path.name + '.', suffix='.tmp',
                                    dir=str(path.parent))
    try:
        with os.fdopen(fd, 'wb') as fh:
            fh.write(blob)
            fh.flush()
            os.fsync(fh.fileno())
        os.chmod(tmp_name, mode)
        os.replace(tmp_name, path)
    finally:
        if os.path.exists(tmp_name):
            os.unlink(tmp_name)
    return backup


def main(argv=None):
    parser = argparse.ArgumentParser(
        description='Add BgmCN.img/BgmTH.img wrappers to Sound.nx.')
    parser.add_argument('--sound-nx', default='assets/Sound.nx')
    parser.add_argument('--bgm-cn', default='/tmp/orient-sound/BgmCN.nx')
    parser.add_argument('--bgm-th', default='/tmp/orient-sound/BgmTH.nx')
    parser.add_argument('--backup-suffix', default='.pre-orient-sound-backup')
    args = parser.parse_args(argv)

    path = Path(args.sound_nx)
    if not path.is_file():
        fail('missing archive: %s' % path)
    for wrapper, source in ((b'BgmCN.img', args.bgm_cn),
                            (b'BgmTH.img', args.bgm_th)):
        if not Path(source).is_file():
            fail('missing source %s (for %s)'
                 % (source, wrapper.decode()))

    nx = parse_nx(str(path))
    baseline_names = root_child_names(nx)
    baseline_audios = len(nx['audios'])
    if len(set(baseline_names)) != len(baseline_names):
        fail('%s: root children are not unique' % path)
    if baseline_names != sorted(baseline_names):
        fail('%s: root children are not sorted bytewise' % path)
    for wrapper, _source in ((b'BgmCN.img', args.bgm_cn),
                             (b'BgmTH.img', args.bgm_th)):
        if find_child(nx, wrapper) is not None:
            fail('%s already contains %s, nothing to do'
                 % (path, wrapper.decode()))

    sources = []
    for wrapper, source in ((b'BgmCN.img', args.bgm_cn),
                            (b'BgmTH.img', args.bgm_th)):
        src = parse_nx(source)
        src_root = src['nodes'][0]
        if src_root[3] != 0 or src_root[2] == 0:
            fail('%s: root is not a non-empty folder' % source)
        sources.append((wrapper, src))

    state = nx
    wrapper_indices = []
    for wrapper, src in sources:
        state, wrapper_index = with_source(state, src, wrapper)
        wrapper_indices.append(wrapper_index)
    merged = with_sorted_root_slots(state, wrapper_indices)

    errors = check_archive(merged, baseline_names, sources, baseline_audios)
    if errors:
        fail('in-memory verification failed:\n  - ' + '\n  - '.join(errors))

    blob = build(merged)
    before = path.stat().st_size
    backup = write_archive(path, blob, args.backup_suffix)
    print('%s: %d -> %d bytes (+%d), backup %s'
          % (path, before, len(blob), len(blob) - before, backup))

    baseline_names = tuple(baseline_names)
    del merged, state, blob, nx
    reparsed = parse_nx(str(path))
    errors = check_archive(reparsed, baseline_names, sources, baseline_audios)
    if errors:
        fail('post-write verification failed:\n  - ' + '\n  - '.join(errors))
    print('verified: %d root children, %d audios, %d nodes'
          % (len(root_child_names(reparsed)), len(reparsed['audios']),
             len(reparsed['nodes'])))
    report(reparsed, [wrapper for wrapper, _src in sources])
    print('path shape: wrapper children have no .img suffix, e.g. '
          'Sound.nx -> BgmCN.img -> GoShanghai')


if __name__ == '__main__':
    main(sys.argv[1:])
