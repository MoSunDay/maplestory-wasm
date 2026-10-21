#!/usr/bin/env python3
"""Verify the 079MAX4 free-market injection (run around inject_map_nx.py).

Before injecting, record a baseline:

    verify_map_nx.py --save-baseline

After injecting, verify (default mode) against that baseline:

    verify_map_nx.py

Checks, all of which must pass:

1. Map.nx: ``Obj/chewchewIsland.img``, ``Back/chewchewIsland.img`` and
   ``Map/Map9/910000000.img`` are node-for-node equivalent to the map079 tree
   JSONs (name/type/value); every bitmap matches its ``pixels/*.bin`` dims and
   decodes (LZ4) to identical bytes, inside the client read window.
2. Child blocks of the new subtrees stay bytewise sorted (nlnx binary search).
3. Untouched content: per-child structural digests recorded pre-injection for
   the root, ``Obj``, ``Back``, ``Map`` and ``Map/Map9`` match post-injection
   everywhere except the three spliced subtrees; node/string/bitmap counts
   grow by exactly the injected amounts.
4. String.nx: ``Map.img/etc/910000000..910000022`` streetName/mapName equal
   the 079 originals, other ``etc`` ids and root wrappers are unchanged, and
   the previously injected ``Npc.img/9270052`` (妮娜) is still present.

Exemption: after injection the 22 free-market rooms get their
``info/mapMark`` re-pointed (FreeMarket -> the 079 towns, patch_nx_string),
so their baseline digests would misreport.  Those rooms are skipped in the
untouched-digest comparison (and 910000001.img in the spot checks) but are
positively checked against ``MAPMARK_TOWNS`` instead.
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from map079_tree import (  # noqa: E402
    MAP079_DIR, T_BITMAP, T_INT, T_NONE, T_REAL, T_STRING, T_VECTOR,
    child_indices, child_stats, find_path, lz4_decompress, load_tree,
    node_name, require_path, subtree_digest, window_ok,
)
from merge_skill_nx import parse_nx  # noqa: E402

FM_IDS = tuple(range(910000000, 910000023))
SPOT_CHECKS = ('signboard.img', '910000001.img', 'grassySoil.img')
# Post-injection mapMark patches (patch_nx_string.py, node /info/mapMark):
# exempt from the baseline digest, positively checked by check_mapmark_patches.
MAPMARK_TOWNS = dict(
    [(  '%d.img' % i, 'Henesys') for i in (910000001, 910000002, 910000003,
                                           910000005, 910000006)]
    + [('%d.img' % i, 'ElNath') for i in (910000004, 910000018, 910000019,
                                          910000020, 910000021, 910000022)]
    + [('%d.img' % i, 'Ludibrium') for i in range(910000007, 910000013)]
    + [('%d.img' % i, 'Perion') for i in range(910000013, 910000018)]
)
DEFAULT_BASELINE = Path(__file__).resolve().parent / '.map079_baseline.json'
CONTAINERS = ('list', 'convex')
ERRORS = []


def check(ok, msg):
    if not ok:
        ERRORS.append(msg)
    return ok


def fail(msg):
    sys.exit('verify_map_nx: ' + msg)


# ------------------------------------------------ subtree vs tree JSON check

def value_ok(nx, node, child):
    typ, union = node[3], node[4]
    kind = child['kind']
    if kind in ('int', 'u16'):
        return typ == T_INT and struct.unpack_from('<q', union, 0)[0] == child['value']
    if kind in ('f32', 'f64'):
        return typ == T_REAL and struct.unpack_from('<d', union, 0)[0] == float(child['value'])
    if kind == 'str':
        sid = struct.unpack_from('<I', union, 0)[0]
        return typ == T_STRING and nx['strings'][sid] == child['value'].encode('utf-8')
    if kind == 'vector':
        return typ == T_VECTOR and struct.unpack('<ii', union[:8]) == (child['x'], child['y'])
    if kind == 'null':
        return typ == T_NONE and union == b'\0' * 8
    return typ == T_NONE and union == b'\0' * 8  # list / convex


def bitmap_ok(nx, node, child, where, pixel_root, stats):
    typ, union = node[3], node[4]
    w, h = child['width'], child['height']
    if typ != T_BITMAP:
        return False
    idx, bw, bh = struct.unpack_from('<IHH', union, 0)
    if (bw, bh) != (w, h):
        ERRORS.append('%s: dims %dx%d != %dx%d' % (where, bw, bh, w, h))
        return False
    blob = nx['bitmaps'][idx]
    clen = struct.unpack('<I', blob[:4])[0]
    if not window_ok(clen, w, h):
        ERRORS.append('%s: block %d B outside the %d B read window'
                      % (where, clen, 4 * w * h))
        return False
    ref = Path(pixel_root, child['pixels']).read_bytes()
    ok = lz4_decompress(blob[4:4 + clen], 4 * w * h) == ref
    stats[0] += 1
    if not ok:
        ERRORS.append('%s: pixels differ from %s' % (where, child['pixels']))
    return ok


def compare_subtree(nx, idx, child, where, pixel_root, stats):
    """Compare one NX node against one tree-JSON node; returns ok."""
    node = nx['nodes'][idx]
    stats[1] += 1
    name = node_name(nx, idx).decode('utf-8', 'replace')
    ok = check(name == child['name'], '%s: name %r != %r' % (where, name, child['name']))
    kids = list(child_indices(nx, idx))
    order = [node_name(nx, c) for c in kids]
    ok &= check(order == sorted(order), '%s/%s: children not bytewise sorted'
                % (where, child['name']))
    if child['kind'] == 'canvas':
        ok &= bitmap_ok(nx, node, child, where + '/' + name, pixel_root, stats)
    elif child['kind'] not in CONTAINERS:
        ok &= check(value_ok(nx, node, child),
                    '%s/%s: value mismatch for %s' % (where, name, child['kind']))
        return ok
    jkids = sorted(child.get('children', []), key=lambda c: c['name'].encode('utf-8'))
    if not check(len(kids) == len(jkids),
                 '%s/%s: %d children != %d' % (where, name, len(kids), len(jkids))):
        return False
    for c, jc in zip(kids, jkids):
        ok &= compare_subtree(nx, c, jc, '%s/%s' % (where, name), pixel_root, stats)
    return ok


def check_injected_subtrees(nx, map079):
    specs = [
        ([b'Obj', b'chewchewIsland.img'], 'chewchewIsland.json'),
        ([b'Back', b'chewchewIsland.img'], 'Back.chewchewIsland.json'),
        ([b'Map', b'Map9', b'910000000.img'], '910000000.json'),
    ]
    for path, tree_file in specs:
        where = '/'.join(p.decode() for p in path)
        idx = find_path(nx, path)
        if not check(idx is not None, '%s: missing' % where):
            continue
        tree = load_tree(Path(map079) / tree_file)
        stats = [0, 0]  # bitmaps, nodes
        # The NX wrapper node ("*.img") stands in for the JSON tree root:
        # compare its children against the root's children.
        kids = list(child_indices(nx, idx))
        jkids = sorted(tree.get('children', []),
                       key=lambda c: c['name'].encode('utf-8'))
        ok = check(len(kids) == len(jkids),
                   '%s: %d children != %d' % (where, len(kids), len(jkids)))
        stats[1] += 1
        order = [node_name(nx, c) for c in kids]
        ok &= check(order == sorted(order), '%s: children not bytewise sorted' % where)
        for c, jc in zip(kids, jkids):
            ok &= compare_subtree(nx, c, jc, where, map079, stats)
        print('  %-28s %-6s %5d nodes %4d bitmaps' % (where, 'ok' if ok else 'FAIL',
                                                      stats[1], stats[0]))


# ------------------------------------------------------------ baseline logic

def map_baseline(nx, map079):
    sys.setrecursionlimit(100000)
    data = {'counts': [len(nx['nodes']), len(nx['strings']), len(nx['bitmaps'])],
            'root': child_stats(nx, 0),
            'Obj': child_stats(nx, require_path(nx, [b'Obj'])),
            'Back': child_stats(nx, require_path(nx, [b'Back'])),
            'Map': child_stats(nx, require_path(nx, [b'Map'])),
            'Map9': child_stats(nx, require_path(nx, [b'Map', b'Map9']))}
    for name in SPOT_CHECKS:
        idx = (find_path(nx, [b'Obj', name.encode()])
               or find_path(nx, [b'Back', name.encode()])
               or find_path(nx, [b'Map', b'Map9', name.encode()]))
        if idx is not None:
            data['spot/%s' % name] = list(subtree_digest(nx, idx))
    return data


def compare_children(nx, idx, base, label, skip=()):
    """Every non-skipped child of `idx` must match its baseline digest."""
    base = {k: v for k, v in base.items() if k not in skip}
    names = [nm for nm in (node_name(nx, c).decode('utf-8', 'replace')
                           for c in child_indices(nx, idx)) if nm not in skip]
    extra = sorted(set(names) - set(base), key=str.encode)
    gone = sorted(set(base) - set(names), key=str.encode)
    check(not extra, '%s: unexpected children %s' % (label, extra))
    check(not gone, '%s: missing children %s' % (label, gone))
    seen = set()
    for c in child_indices(nx, idx):
        name = node_name(nx, c).decode('utf-8', 'replace')
        if name in base and name not in seen:  # first occurrence wins
            seen.add(name)
            check(list(subtree_digest(nx, c)) == base[name],
                  '%s/%s: subtree changed (%s -> %s)' % (label, name, base[name],
                                                         list(subtree_digest(nx, c))))
    return len(base)


def json_totals(tree):
    """(node count incl. root, canvas count) of a tree-JSON subtree."""
    nodes = canvases = 0
    stack = [tree]
    while stack:
        cur = stack.pop()
        nodes += 1
        canvases += cur['kind'] == 'canvas'
        stack.extend(cur.get('children', []))
    return nodes, canvases


def expected_growth(map079):
    """(nodes, bitmaps) the injection must add to Map.nx."""
    nodes = bitmaps = 0
    for tree_file in ('chewchewIsland.json', 'Back.chewchewIsland.json'):
        n, c = json_totals(load_tree(Path(map079) / tree_file))
        nodes += n  # tree size incl. root == new wrapper + descendants
        bitmaps += c
    n, c = json_totals(load_tree(Path(map079) / '910000000.json'))
    return nodes + n - 1, bitmaps + c  # wrapper reused, children appended


def check_untouched(nx, base, map079):
    counts = base['counts']
    add_nodes, add_bitmaps = expected_growth(map079)
    check(len(nx['nodes']) == counts[0] + add_nodes,
          'node count %d != baseline %d + %d' % (len(nx['nodes']), counts[0], add_nodes))
    check(len(nx['bitmaps']) == counts[2] + add_bitmaps,
          'bitmap count %d != baseline %d + %d'
          % (len(nx['bitmaps']), counts[2], add_bitmaps))
    compare_children(nx, 0, base['root'], 'root', skip=('Obj', 'Back', 'Map'))
    compare_children(nx, require_path(nx, [b'Obj']), base['Obj'], 'Obj',
                     skip=('chewchewIsland.img',))
    compare_children(nx, require_path(nx, [b'Back']), base['Back'], 'Back',
                     skip=('chewchewIsland.img',))
    compare_children(nx, require_path(nx, [b'Map']), base['Map'], 'Map', skip=('Map9',))
    compare_children(nx, require_path(nx, [b'Map', b'Map9']), base['Map9'], 'Map/Map9',
                     skip=('910000000.img',) + tuple(MAPMARK_TOWNS))
    for name in SPOT_CHECKS:
        idx = (find_path(nx, [b'Obj', name.encode()])
               or find_path(nx, [b'Back', name.encode()])
               or find_path(nx, [b'Map', b'Map9', name.encode()]))
        want = base.get('spot/%s' % name)
        if want is None:
            print('  spot %-16s not present in baseline, skipped' % name)
            continue
        if name in MAPMARK_TOWNS:
            print('  spot %-16s exempt (mapMark patch), see mapMark check'
                  % name)
            continue
        check(idx is not None and list(subtree_digest(nx, idx)) == want,
              'spot check %s: subtree changed' % name)
        print('  spot %-16s unchanged (%d nodes)' % (name, want[0]))


def check_mapmark_patches(nx):
    """Each patched FM room must carry its expected mapMark town."""
    good = 0
    for room, town in sorted(MAPMARK_TOWNS.items()):
        idx = find_path(nx, [b'Map', b'Map9', room.encode(), b'info', b'mapMark'])
        if idx is None:
            check(False, 'Map/Map9/%s/info/mapMark: missing' % room)
            continue
        node = nx['nodes'][idx]
        if node[3] != T_STRING:
            check(False, 'Map/Map9/%s/info/mapMark: not a string (type %d)'
                  % (room, node[3]))
            continue
        sid = struct.unpack_from('<I', node[4], 0)[0]
        got = nx['strings'][sid].decode('utf-8')
        good += check(got == town,
                      'Map/Map9/%s/info/mapMark: %r != %r' % (room, got, town))
    print('  Map/Map9 FM rooms mapMark: %d/%d match the patched towns'
          % (good, len(MAPMARK_TOWNS)))


# --------------------------------------------------------------- String.nx

def map079_names(map079):
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from inject_map_nx import map079_names as read_names
    return read_names(map079)


def check_string_nx(sx, map079, base):
    names = map079_names(map079)
    etc = require_path(sx, [b'Map.img', b'etc'])
    bad = 0
    for mapid in FM_IDS:
        idx = find_path(sx, [b'Map.img', b'etc', str(mapid).encode()])
        if idx is None:
            check(False, 'String.nx: Map.img/etc/%d missing' % mapid)
            bad += 1
            continue
        fields = {node_name(sx, c).decode(): c for c in child_indices(sx, idx)}
        for field, want in zip(('streetName', 'mapName'), names[mapid]):
            c = fields.get(field)
            sid = struct.unpack_from('<I', sx['nodes'][c][4], 0)[0] if c is not None else None
            got = sx['strings'][sid].decode('utf-8') if sid is not None else None
            if not check(got == want, 'etc/%d %s: %r != %r' % (mapid, field, got, want)):
                bad += 1
    print('  Map.img/etc/910000000..022 names: %d/23 entries match' % (23 - bad))
    compare_children(sx, 0, base['string_root'], 'String.nx root', skip=('Map.img',))
    compare_children(sx, require_path(sx, [b'Map.img']), base['string_Mapimg'],
                     'String.nx Map.img', skip=('etc',))
    compare_children(sx, etc, base['string_etc'], 'String.nx Map.img/etc',
                     skip=tuple(str(i) for i in FM_IDS))
    nina = find_path(sx, [b'Npc.img', b'9270052'])
    ok = check(nina is not None, 'String.nx: Npc.img/9270052 (妮娜) missing')
    if ok:
        fields = {node_name(sx, c).decode(): c for c in child_indices(sx, nina)}
        sid = struct.unpack_from('<I', sx['nodes'][fields['name']][4], 0)[0]
        check(sx['strings'][sid].decode('utf-8') == '妮娜',
              'Npc.img/9270052 name changed')
        print('  Npc.img/9270052 (妮娜): still present')


def string_baseline(sx):
    return {'string_root': child_stats(sx, 0),
            'string_Mapimg': child_stats(sx, require_path(sx, [b'Map.img'])),
            'string_etc': child_stats(sx, require_path(sx, [b'Map.img', b'etc']))}


# --------------------------------------------------------------------- main

def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--save-baseline', action='store_true',
                    help='record pre-injection state (run before injecting)')
    ap.add_argument('--baseline', type=Path, default=DEFAULT_BASELINE)
    ap.add_argument('--assets-dir', type=Path, default=Path('assets'))
    ap.add_argument('--map079-dir', type=Path, default=MAP079_DIR)
    args = ap.parse_args(argv)
    sys.setrecursionlimit(100000)

    map_nx = args.assets_dir / 'Map.nx'
    string_nx = args.assets_dir / 'String.nx'
    nx = parse_nx(map_nx)
    sx = parse_nx(string_nx)

    if args.save_baseline:
        if find_path(nx, [b'Obj', b'chewchewIsland.img']) is not None:
            fail('Map.nx already injected; baseline must be recorded before')
        data = map_baseline(nx, args.map079_dir)
        data.update(string_baseline(sx))
        args.baseline.write_text(json.dumps(data), encoding='utf-8')
        print('baseline saved: %s (%d root children, %d etc ids)'
              % (args.baseline, len(data['root']), len(data['string_etc'])))
        return

    if not args.baseline.exists():
        fail('no baseline at %s (run --save-baseline before injecting)' % args.baseline)
    base = json.loads(args.baseline.read_text(encoding='utf-8'))
    print('Map.nx: %d nodes, %d strings, %d bitmaps' % (len(nx['nodes']),
          len(nx['strings']), len(nx['bitmaps'])))
    check_injected_subtrees(nx, args.map079_dir)
    check_untouched(nx, base, args.map079_dir)
    check_mapmark_patches(nx)
    print('String.nx: %d nodes' % len(sx['nodes']))
    check_string_nx(sx, args.map079_dir, base)
    if ERRORS:
        print('FAILED: %d error(s)' % len(ERRORS))
        for err in ERRORS[:20]:
            print('  - %s' % err)
        sys.exit(1)
    print('ALL CHECKS PASSED')


if __name__ == '__main__':
    main()
