#!/usr/bin/env python3
"""Per-level child-block surgery for the Final Attack data patches.

Two operations on ``Skill.nx`` level nodes, shared by the FA data tools:

* ``plan_counters``/``apply_counters`` -- force ``attackCount``/``mobCount``
  (plus per-target extras such as the ranged FA ``bulletCount``) on every
  level of a skill, splicing missing ints into the child block and
  overwriting stray values;
* ``plan_level_sync``/``apply_level_sync`` -- copy one skill's whole per-level
  child sets from a resident sibling (a graft keeps its icons/hit/req).

nlnx resolves children with a bytewise binary search over the contiguous
child block, so every inserted run stays name-sorted and the node table is
rebuilt with the parent/children pointers remapped.  Pure helpers around the
``merge_skill_nx`` dict, stdlib only.
"""
from __future__ import annotations

import bisect
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
for _p in (REPO / 'tools', REPO / 'tools/nx'):
    sys.path.insert(0, str(_p))

from inject_skill_nx import T_INT  # noqa: E402
from map079_tree import child_indices, find_path, node_name  # noqa: E402
from patch_final_attack_counts_nx import (  # noqa: E402
    LEVELS, PROPS, bump_level_counts, level_indices, plan_level, set_prop,
    splice_props,
)


def fail(msg):
    sys.exit('level_children: ' + str(msg))


def skill_path(img, sid):
    """Node path of one skill subtree in a Skill.nx archive."""
    return (('%d.img' % img).encode(), b'skill', sid.encode())


def require(nx, path, what):
    idx = find_path(nx, path)
    if idx is None:
        fail('%s not found: %s' % (what, b'/'.join(path).decode()))
    return idx


def int_values(nx, idx):
    """name -> value for every int child of a node."""
    return {node_name(nx, c): struct.unpack('<q', nx['nodes'][c][4])[0]
            for c in child_indices(nx, idx) if nx['nodes'][c][3] == T_INT}


# ------------------------------------------------------------- level counters
def plan_counters(nx, targets, extras=None):
    """Per-skill report plus the pending counter inserts/overwrites.

    Every level of every target carries the fixed ``PROPS`` pair
    (``attackCount=2``/``mobCount=8``); ``extras`` optionally maps a target
    ``(img, sid)`` to further ``(prop, value)`` pairs.  The ranged Final
    Attack variants use it for ``bulletCount=2``: the client derives the
    damage-line count of an ``Attack::RANGED`` follow-up from ``bulletCount``
    (``Skill.cpp`` ``apply_stats``), so attackCount alone is ignored there.
    """
    extras = extras or {}
    report, inserts, sets, per_level = [], [], [], {}
    for img, sid in targets:
        if find_path(nx, skill_path(img, sid)) is None:
            continue
        props = tuple(PROPS) + tuple(extras.get((img, sid), ()))
        ok = fixed = inserted = 0
        for lv in level_indices(nx, img, sid):
            here = 0
            for (act, at), (prop, value) in zip(plan_level(nx, lv, props),
                                                props):
                if act == 'insert':
                    inserts.append((lv, prop, value, at))
                    here += 1
                elif act == 'set':
                    sets.append((lv, prop, value))
                    fixed += 1
                else:
                    ok += 1
            if here:
                per_level[lv] = per_level.get(lv, 0) + here
                inserted += here
        report.append({'image': '%d.img' % img, 'skill': sid, 'ok': ok,
                       'set': fixed, 'insert': inserted,
                       'props': [prop.decode() for prop, _ in props]})
    return report, inserts, sets, per_level


def apply_counters(nx, inserts, sets, per_level):
    """Overwrite stray values in place, then splice the missing int nodes."""
    for lv, prop, value in sets:
        set_prop(nx, lv, prop, value)
    if not inserts:
        return 0
    points, counts = splice_props(nx, inserts)
    bump_level_counts(nx, points, per_level, counts)
    return sum(counts)

# ------------------------------------------------------- sibling level copy
def level_leaves(nx, idx):
    """(name id, type, union) of a node's leaf children, in stored order."""
    return [(nx['nodes'][c][0],) + tuple(nx['nodes'][c][3:])
            for c in child_indices(nx, idx)]


def sorted_leaves(nx, idx):
    """Leaf children sorted by name bytes (the order nlnx binary-searches)."""
    return sorted(level_leaves(nx, idx), key=lambda leaf: nx['strings'][leaf[0]])


def plan_level_sync(nx, sibling):
    """Per-graft report plus {grafted level node: sibling leaves} edits."""
    report, edits = [], {}
    for (img, sid), (simg, ssid) in sibling:
        if find_path(nx, skill_path(img, sid)) is None:
            continue
        changed = 0
        for lv in level_indices(nx, img, sid):
            sib = require(nx, skill_path(simg, ssid) + (b'level', node_name(nx, lv)),
                          'sibling level')
            want = sorted_leaves(nx, sib)
            if level_leaves(nx, lv) != want:
                edits[lv] = want
                changed += 1
        report.append({'image': '%d.img' % img, 'skill': sid, 'sibling': ssid,
                       'levels': changed, 'of': LEVELS})
    return report, edits


def check_level_blocks(nx, blocks):
    """Every edited child block must be owned by its level node alone.

    The node table is a DAG (frames share blocks), so replacing a block that
    another parent also points at would silently corrupt that subtree.
    """
    starts = {}
    for i, (_, ch, num, _, _) in enumerate(nx['nodes']):
        if num:
            starts.setdefault(ch, []).append(i)
    parents = {lv for lv, _ in blocks.values()}
    foreign = [(ch, ids) for ch, ids in starts.items()
               if not any(i in parents for i in ids)]
    for base, (lv, num) in blocks.items():
        if not num:
            fail('level node %d has no children to replace' % lv)
        if any(i != lv for i in starts.get(base, ())):
            fail('level node %d shares its child block at %d' % (lv, base))
        for ch, ids in foreign:
            span = nx['nodes'][ids[0]][2]
            if ch < base + num and base < ch + span:
                fail('node %d block [%d,%d) overlaps level node %d'
                     % (ids[0], ch, ch + span, lv))


def splice_leaves(nodes, drop, ins_at):
    """Rebuild the node table around replaced child blocks.

    ``drop`` holds the old indices to remove, ``ins_at`` maps an old insertion
    point to the leaf tuples emitted there (name-sorted).  A run emitted at a
    block's old start takes that slot, so the parent keeps pointing at the
    first leaf; every other children pointer is remapped by the same shift.
    Returns the new table plus a old-index -> new-index mapping.
    """
    points = sorted(ins_at)
    kept = [0] * (len(nodes) + 1)
    added = [0] * (len(points) + 1)
    for k, point in enumerate(points):
        added[k + 1] = added[k] + len(ins_at[point])
    for i in range(len(nodes)):
        kept[i + 1] = kept[i] + (i not in drop)

    def remap(idx):
        return kept[idx] + added[bisect.bisect_left(points, idx)]

    out = []
    for i, (name, base, num, typ, union) in enumerate(nodes):
        if i in ins_at:
            out.extend((leaf[0], 0, 0, leaf[1], leaf[2]) for leaf in ins_at[i])
        if i not in drop:
            out.append((name, remap(base), num, typ, union))
    return out, remap


def apply_level_sync(nx, edits):
    """Replace drifting level child blocks; returns the level count."""
    if not edits:
        return 0
    nodes = nx['nodes']
    blocks, ins_at = {}, {}
    for lv, leaves in edits.items():
        _, base, num, _, _ = nodes[lv]
        blocks[base] = (lv, num)
        ins_at[base] = tuple(leaves)
    check_level_blocks(nx, blocks)
    drop = {i for base, (_, num) in blocks.items()
            for i in range(base, base + num)}
    out, remap = splice_leaves(nodes, drop, ins_at)
    for lv, leaves in edits.items():
        i = remap(lv)
        name, base, _, typ, union = out[i]
        out[i] = (name, base, len(leaves), typ, union)
    nx['nodes'] = out
    return len(edits)
