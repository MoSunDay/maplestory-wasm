#!/usr/bin/env python3
"""Restore English QuestInfo + Say dialogue text in Quest.nx from the server WZ XML.

The shipped Quest.nx is a repack whose non-ASCII (Korean) strings were
masked to one '?' (0x3F) per source character (Latin runs and colour
markers like #b / #p...# survived, so many values are only partially
masked). The masking happened before the NX conversion, so the string
table deduplicated the fully-masked runs by length: the whole file keeps
only ~130 distinct '?'-runs shared by thousands of nodes (in
QuestInfo.img AND Say.img alike). An entry-level rewrite would blast a
single English text over hundreds of unrelated nodes.

This script instead re-points nodes: every original string entry keeps
its index and bytes, and each patched node's u32 value index is aimed at
a fresh (or already-present) entry holding the English text from the
linked server checkout:
  - QuestInfo.img/<qid>/{name,parent,0..31}  <- QuestInfo.img.xml
    (whose marker ids - e.g. #p9000021# - actually exist in the client's
    String.nx, unlike ids surviving in the masked repack)
  - Say.img dialogue pages, i.e. every <string> under the Say.img XML
    subtree: <qid>/<0|1>/<page>, <qid>/<0|1>/{yes,no}/<page> and
    <qid>/<0|1>/stop/{npc,item,mob}/<page> <- Say.img.xml
The file is rebuilt compactly (header + node table + string blob +
offset table, node_offset preserved), which is safe because Quest.nx
contains 0 bitmap and 0 audio nodes, so no other absolute offsets exist.
Re-running is a no-op: patched nodes then match the XML text verbatim
and nothing gets rewritten.

Usage:
    patch_quest_info_text.py [--nx FILE ...] [--xml FILE] [--say-xml FILE]
                             [--dry-run] [--only-fully-masked]
"""
import argparse
import collections
import os
import random
import struct
import sys
import xml.etree.ElementTree as ET

MAGIC = 0x34474B50  # PKG4
HEADER = struct.Struct('<IIQIQIQIQ')
HEADER_SIZE = HEADER.size  # 52
NODE_STRIDE = 20
NODE_HEAD = struct.Struct('<IIHH')  # name, first_child, num, type
NODE_TYPE_STRING = 3
VALUE_OFFSET = 12  # union -> u32 string index for type-3 nodes
ROOT_IMG = 'QuestInfo.img'
SAY_ROOT_IMG = 'Say.img'
TEXT_FIELDS = frozenset({'name', 'parent'} | {str(i) for i in range(32)})
BACKUP_SUFFIX = '.pre-quest-l10n-backup'
SAY_BACKUP_SUFFIX = '.pre-say-l10n-backup'
SAMPLE_SEED = 20261031
SAMPLE_COUNT = 10
MAX_STRING = 0xFFFF  # the string length prefix is u16

DEFAULT_NX = ('assets/Quest.nx', 'desktop/resources/msweb/assets/Quest.nx')
DEFAULT_XML = 'link_repos/MapleStory-Server/wz/Quest.wz/QuestInfo.img.xml'
DEFAULT_SAY_XML = 'link_repos/MapleStory-Server/wz/Quest.wz/Say.img.xml'

CANONICAL = [  # (path, expected text) asserted after every rebuild
    (('QuestInfo.img', '1000', 'name'), "Borrowing Sera's Mirror"),
    (('QuestInfo.img', '1000', 'parent'), "Sera's Mirror"),
    (('QuestInfo.img', '1000', '0'), "Let's go to Heena."),
]

SAY_CANONICAL_PATHS = (  # relative Say.img paths asserted after every rebuild
    ('1000', '0', '0'),
    ('1000', '1', 'stop', 'npc', '0'),
    ('21000', '1', '0'),
)


def read_header(data):
    """Return the unpacked PKG4 header, validating magic and node_offset."""
    fields = HEADER.unpack_from(data, 0)
    if fields[0] != MAGIC:
        raise ValueError('not a PKG4 NX file (magic %#x)' % fields[0])
    if fields[5] or fields[7]:
        raise ValueError('file embeds bitmap/audio data; compact rewrite is unsafe')
    if fields[2] < HEADER_SIZE:
        raise ValueError('node_offset %d overlaps the %d-byte header' % (fields[2], HEADER_SIZE))
    return fields


def string_at(data, offset):
    """Read one u16-length-prefixed UTF-8 blob at an absolute file offset."""
    length, = struct.unpack_from('<H', data, offset)
    return data[offset + 2:offset + 2 + length]


def read_string_table(data, count, table_offset):
    """Materialise the whole string table as raw bytes, indexed by entry id."""
    return [string_at(data, struct.unpack_from('<Q', data, table_offset + i * 8)[0])
            for i in range(count)]


def node_head(data, node_offset, index):
    """Return (name_idx, first_child, num, type) of a node."""
    return NODE_HEAD.unpack_from(data, node_offset + NODE_STRIDE * index)


def node_value_index(data, node_offset, index):
    """Return the u32 string-table index stored in a type-3 node's union."""
    value, = struct.unpack_from('<I', data, node_offset + NODE_STRIDE * index + VALUE_OFFSET)
    return value


def walk(data, node_offset, node_count, table):
    """Yield (index, name, path, img_root, type, value_index) per node.

    Iterative walk of the child-range forest rooted at node 0; names are
    resolved through `table`. `img_root` is the name of the top-level
    *.img subtree the node belongs to ('QuestInfo.img', 'Say.img', ...),
    or None above them, so callers can scope themselves to one section.
    Raises if the walk does not cover exactly node_count nodes.
    """
    stack = [(0, (), None)]
    seen = 0
    while stack:
        index, path, img_root = stack.pop()
        seen += 1
        name_idx, first_child, num, ntype = node_head(data, node_offset, index)
        name = table[name_idx]
        here = name.decode('utf-8', 'replace') if len(path) == 1 else img_root
        value = node_value_index(data, node_offset, index) if ntype == NODE_TYPE_STRING else None
        yield index, name, path, here, ntype, value
        for child in range(first_child, first_child + num):
            stack.append((child, path + (name,), here))
    if seen != node_count:
        raise ValueError('tree walk covered %d nodes, header claims %d' % (seen, node_count))


def is_masked(raw):
    """True when the blob is non-empty and consists solely of '?' bytes."""
    return len(raw) > 0 and all(byte == 0x3F for byte in raw)


def parse_quest_xml(xml_path):
    """Return {quest_id: {field: text}} for direct <string> children we handle.

    ElementTree already unescapes the five predefined XML entities
    (&apos; &quot; &amp; &lt; &gt;); colour markers such as #b / #p...#
    pass through untouched.
    """
    quests = {}
    for quest in ET.parse(xml_path).getroot():
        if quest.tag != 'imgdir':
            continue
        texts = {}
        for element in quest:
            name = element.get('name')
            if element.tag == 'string' and name in TEXT_FIELDS:
                value = element.get('value')
                if value is not None:
                    texts[name] = value
        quests[quest.get('name')] = texts
    return quests


def parse_say_xml(xml_path):
    """Return {relative_path_tuple: text} for every <string> under Say.img.

    The whole subtree is walked because dialogue pages do not follow one
    flat layout: they sit at <qid>/<0|1>/<page> and inside nested
    yes / no / stop / stop/{npc,item,mob} imgdirs (e.g. the key
    ('1000', '1', 'stop', 'npc', '0') maps Say.img/1000/1/stop/npc/0).
    ElementTree already unescapes the five predefined XML entities
    (&apos; &quot; &amp; &lt; &gt;); colour/npc/item markers such as
    #b, #p9000021# and #t2010007# pass through untouched, as do \r\n
    line breaks the client renders literally.
    """
    root = ET.parse(xml_path).getroot()
    if root.tag != 'imgdir' or root.get('name') != SAY_ROOT_IMG:
        raise ValueError('%s: expected an <imgdir name="%s"> root, got %r'
                         % (xml_path, SAY_ROOT_IMG, root.get('name')))
    texts = {}

    def collect(element, prefix):
        for child in element:
            name = child.get('name')
            if child.tag == 'string':
                if child.get('value') is not None:
                    texts.setdefault(prefix + (name,), child.get('value'))
            elif child.tag == 'imgdir':
                collect(child, prefix + (name,))

    collect(root, ())
    return texts


def plan_patches(data, node_offset, node_count, table, xml, only_fully_masked=False):
    """Decide which QuestInfo string nodes get which English text.

    Returns (patches {node_index: text}, stats Counter, questinfo_indices).
    A node is patched when its value still carries masking damage - a '?'
    run, partial or full - and the XML carries text for that quest id +
    field. Values that already equal the XML text are skipped, which keeps
    the run idempotent and leaves readable Latin text untouched.
    """
    patches = {}
    stats = collections.Counter()
    questinfo = set()
    for index, name, path, img_root, ntype, value in walk(data, node_offset, node_count, table):
        if img_root == ROOT_IMG:
            questinfo.add(index)
        if img_root != ROOT_IMG or len(path) != 3 or ntype != NODE_TYPE_STRING:
            continue
        quest_id, field = path[2].decode('utf-8', 'replace'), name.decode('utf-8', 'replace')
        current = table[value]
        masked = is_masked(current)
        partial = (not masked) and (0x3F in current)
        if not masked and (only_fully_masked or not partial):
            stats['plain (already readable)'] += 1
            continue
        if field not in TEXT_FIELDS:
            stats['masked, field outside {name,parent,0..31}'] += 1
            continue
        text = xml.get(quest_id, {}).get(field)
        if text is None:
            stats['masked, no XML text'] += 1
            continue
        if text.encode('utf-8') == current:
            stats['text already matches XML'] += 1
            continue
        patches[index] = text
        stats['replaced field %s [%s]' % (field, 'fully masked' if masked else 'partially masked')] += 1
    return patches, stats, questinfo


def plan_say_patches(data, node_offset, node_count, table, xml, only_fully_masked=False):
    """Decide which Say.img string nodes get which English dialogue text.

    `xml` is the {relative_path: text} map from parse_say_xml. Every
    type-3 node under the Say.img root is looked up by its path
    (<qid>/<0|1>/[.../]page) and re-pointed at the XML text when its
    value still carries masking damage; the same idempotency rules as
    the QuestInfo pass apply. XML paths the repack never contained (a
    few quests lost whole subtrees, e.g. Say.img/3927/1) are skipped and
    counted instead of forced in. Returns (patches, stats, say_indices).
    """
    patches = {}
    stats = collections.Counter()
    say_nodes = set()
    node_types = {}
    for index, name, path, img_root, ntype, value in walk(data, node_offset, node_count, table):
        if img_root != SAY_ROOT_IMG:
            continue
        say_nodes.add(index)
        rel = tuple(part.decode('utf-8', 'replace') for part in path[2:] + (name,))
        node_types[rel] = ntype
        if ntype != NODE_TYPE_STRING:
            continue
        current = table[value]
        masked = is_masked(current)
        partial = (not masked) and (0x3F in current)
        if not masked and (only_fully_masked or not partial):
            stats['plain (already readable)'] += 1
            continue
        text = xml.get(rel)
        if text is None:
            stats['masked, no XML text'] += 1
            continue
        if text.encode('utf-8') == current:
            stats['text already matches XML'] += 1
            continue
        patches[index] = text
        stats['replaced [%s]' % ('fully masked' if masked else 'partially masked')] += 1
    for rel in xml:
        if rel not in node_types:
            stats['XML path missing from nx (skipped)'] += 1
        elif node_types[rel] != NODE_TYPE_STRING:
            stats['XML path maps to nx type-%d node (skipped)' % node_types[rel]] += 1
    return patches, stats, say_nodes


def rebuild(data, header, node_count, node_offset, table, patches):
    """Return the patched file bytes: original entries keep their indices,
    patched nodes are re-pointed to appended (deduplicated) entries."""
    entries = list(table)
    index_of = {}
    for position, raw in enumerate(entries):
        index_of.setdefault(raw, position)
    node_bytes = bytearray(data[node_offset:node_offset + node_count * NODE_STRIDE])
    for index, text in patches.items():
        raw = text.encode('utf-8')
        if len(raw) > MAX_STRING:
            raise ValueError('text too long for u16 length prefix: %r' % text[:50])
        target = index_of.get(raw)
        if target is None:
            target = len(entries)
            entries.append(raw)
            index_of[raw] = target
        struct.pack_into('<I', node_bytes, index * NODE_STRIDE + VALUE_OFFSET, target)
    blob = bytearray()
    offsets = []
    for raw in entries:
        offsets.append(node_offset + len(node_bytes) + len(blob))
        blob += struct.pack('<H', len(raw)) + raw
    table_offset = node_offset + len(node_bytes) + len(blob)
    out = bytearray(data[:HEADER_SIZE])
    out += data[HEADER_SIZE:node_offset]  # preserve any header padding verbatim
    out += node_bytes
    out += blob
    out += b''.join(struct.pack('<Q', offset) for offset in offsets)
    struct.pack_into('<I', out, 16, len(entries))    # string_count
    struct.pack_into('<Q', out, 20, table_offset)    # string_offset
    return bytes(out)


def resolve(data, node_offset, table, parts):
    """Follow a /-less path of child names and return the node index."""
    index = 0
    for part in parts:
        _name, first_child, num, _type = node_head(data, node_offset, index)
        found = None
        for child in range(first_child, first_child + num):
            if table[node_head(data, node_offset, child)[0]] == part.encode():
                found = child
                break
        if found is None:
            raise KeyError('node not found: ' + '/'.join(parts))
        index = found
    return index


def verify(original, new, patches, xml, say_xml, protected, node_count, node_offset):
    """Return a list of assertion failures for the rebuilt file (empty = ok).

    `protected` is the set of node indices inside the patched subtrees
    (QuestInfo.img + Say.img); the random untouched-node sample is drawn
    from everything outside it.
    """
    failures = []
    head = read_header(new)
    old_head = HEADER.unpack_from(original, 0)
    if head[1] != node_count or head[2] != node_offset:
        failures.append('header node geometry changed: %r vs %r' % (head[1:3], (node_count, node_offset)))
    old_table = read_string_table(original, old_head[3], old_head[4])
    new_table = read_string_table(new, head[3], head[4])
    if new_table[:len(old_table)] != old_table:
        failures.append('original string entries were modified')
    canonical = CANONICAL + [(('QuestInfo.img', '20000', 'name'),
                              xml.get('20000', {}).get('name', ''))]
    canonical += [(('Say.img',) + rel, say_xml[rel])
                  for rel in SAY_CANONICAL_PATHS if rel in say_xml]
    for parts, expected in canonical:
        index = resolve(new, node_offset, new_table, parts)
        value = node_value_index(new, node_offset, index)
        actual = new_table[value].decode('utf-8', 'replace')
        if actual != expected:
            failures.append('%s == %r (wanted %r)' % ('/'.join(parts), actual, expected))
    for index, text in patches.items():
        value = node_value_index(new, node_offset, index)
        if new_table[value] != text.encode('utf-8'):
            failures.append('node %d value mismatch after rebuild' % index)
    rng = random.Random(SAMPLE_SEED)
    outside = [i for i in range(node_count) if i not in protected]
    for index in rng.sample(outside, SAMPLE_COUNT):
        old_head_i = node_head(original, node_offset, index)
        new_head_i = node_head(new, node_offset, index)
        same = old_head_i == new_head_i and old_table[old_head_i[0]] == new_table[new_head_i[0]]
        if same and old_head_i[3] == NODE_TYPE_STRING:
            same = old_table[node_value_index(original, node_offset, index)] == \
                new_table[node_value_index(new, node_offset, index)]
        if not same:
            failures.append('untouched node %d changed' % index)
    for index, name, path, img_root, ntype, value in walk(new, node_offset, node_count, new_table):
        if ntype == NODE_TYPE_STRING and value >= len(new_table):
            failures.append('node %d (/%s) value index %d out of range' % (index, name, value))
    return failures


def write_atomic(path, payload):
    """Write payload to a temp sibling, fsync, then atomically replace path."""
    tmp = path + '.tmp-quest-l10n'
    with open(tmp, 'wb') as handle:
        handle.write(payload)
        handle.flush()
        os.fsync(handle.fileno())
    os.replace(tmp, path)


def ensure_backup(path, data, preserved, suffix=BACKUP_SUFFIX):
    """Keep a one-time backup at path+suffix; never overwrite an existing one.

    The documented convention (assets/Quest.nx.pre-quest-l10n-backup /
    assets/Quest.nx.pre-say-l10n-backup) is honoured: identical copies
    (e.g. the desktop bundle twin) do not each get their own 5 MB backup,
    but a divergent file always does. If a backup of that suffix already
    exists it is left untouched (re-runs are no-ops by then anyway).
    """
    backup = path + suffix
    if os.path.exists(backup):
        return None
    for kept_path, kept_data in preserved:
        if kept_data == data:
            return 'backup skipped (identical copy preserved at %s)' % kept_path
    with open(backup, 'wb') as handle:
        handle.write(data)
    preserved.append((backup, data))
    return 'backup: %s' % backup


def process(path, quest_xml, say_xml, dry_run, only_fully_masked=False, preserved=()):
    """Patch one NX file end to end; returns the number of replacements."""
    with open(path, 'rb') as handle:
        data = handle.read()
    header = read_header(data)
    node_count, node_offset = header[1], header[2]
    table = read_string_table(data, header[3], header[4])
    quest_patches, quest_stats, questinfo = plan_patches(
        data, node_offset, node_count, table, quest_xml, only_fully_masked)
    say_patches, say_stats, say_nodes = plan_say_patches(
        data, node_offset, node_count, table, say_xml, only_fully_masked)
    overlap = quest_patches.keys() & say_patches.keys()
    if overlap:
        raise SystemExit('node(s) claimed by both sections: %r' % sorted(overlap))
    patches = dict(quest_patches)
    patches.update(say_patches)
    protected = questinfo | say_nodes
    replaced = len(patches)
    print('%s: %d replacement(s) (%d questinfo + %d say), %d new string entries%s'
          % (path, replaced, len(quest_patches), len(say_patches),
             len({text.encode('utf-8') for text in patches.values()}),
             ' [dry-run]' if dry_run else ''))
    for label, stats in (('questinfo', quest_stats), ('say', say_stats)):
        print('  [%s]' % label)
        for key, count in sorted(stats.items()):
            print('    %-44s %d' % (key, count))
    if not patches:
        failures = verify(data, data, {}, quest_xml, say_xml, protected, node_count, node_offset)
        print('  already patched, nothing to do' if not failures else '  WARN: %r' % failures)
        return 0
    new = rebuild(data, header, node_count, node_offset, table, patches)
    failures = verify(data, new, patches, quest_xml, say_xml, protected, node_count, node_offset)
    if failures:
        raise SystemExit('verification failed, refusing to write:\n  ' + '\n  '.join(failures))
    if dry_run:
        print('  verified in memory (%d bytes), not written' % len(new))
        return replaced
    for suffix, wanted in ((BACKUP_SUFFIX, quest_patches), (SAY_BACKUP_SUFFIX, say_patches)):
        if not wanted:
            continue
        backup = ensure_backup(path, data, preserved, suffix)
        if backup:
            print('  %s' % backup)
    write_atomic(path, new)
    with open(path, 'rb') as handle:  # re-verify what actually landed on disk
        failures = verify(data, handle.read(), patches, quest_xml, say_xml,
                          protected, node_count, node_offset)
    if failures:
        raise SystemExit('post-write verification failed for %s:\n  %s' % (path, '\n  '.join(failures)))
    print('  wrote %d bytes, verified' % len(new))
    return replaced


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--nx', nargs='+', default=list(DEFAULT_NX),
                        help='NX file(s) to patch (default: %(default)s)')
    parser.add_argument('--xml', default=DEFAULT_XML,
                        help='QuestInfo.img.xml with the English quest names')
    parser.add_argument('--say-xml', default=DEFAULT_SAY_XML,
                        help='Say.img.xml with the English NPC dialogue')
    parser.add_argument('--dry-run', action='store_true',
                        help='plan + verify in memory, write nothing')
    parser.add_argument('--only-fully-masked', action='store_true',
                        help='patch only values made entirely of \'?\' bytes')
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    quest_xml = parse_quest_xml(args.xml)
    say_xml = parse_say_xml(args.say_xml)
    preserved = []
    total = 0
    for path in args.nx:
        total += process(path, quest_xml, say_xml,
                         args.dry_run, args.only_fully_masked, preserved)
    print('total replacements: %d' % total)
    return 0


if __name__ == '__main__':
    sys.exit(main())
