#!/usr/bin/env python3
"""Idempotent patcher for the hub MapleStory server wz XMLs.

Brings the 079 "orient" (701/702 Shanghai/Shaolin) and "thailand"
(500xxx) content into the hub server data and removes two retired NPCs
from the Free Market life list:

1. String.wz/Map.img.xml   - append map-name entries (500xxx -> <elin>,
   701/702 -> <event>) using names from the client dump Map.json.
2. String.wz/Npc.img.xml   - copy NPC dialog entries from the 079 source
   String.wz for ids that have no entry in the hub yet.
3. String.wz/Mob.img.xml   - same for mob ids 9600012..9600024.
4. Map.wz/Map/Map9/910000000.img.xml - drop life entries 9000020 /
   9000021 and renumber the remaining life children 0..6.

The script never touches unrelated bytes: all edits are line-oriented
text surgery that preserves the original indentation and EOL style of
each file.  Run with --check for a dry run (exit 1 when changes are
pending) or without arguments to apply atomically (tmp + os.replace).
"""

from __future__ import annotations

import argparse
import json
import os
import re
import stat
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
SRC_WZ = (REPO_ROOT / "link_repos" / ".ref-079max4" /
          "079MAX4-2023年度版" / "怀旧岛079MAX4服务端" / "wz")
HUB_WZ = REPO_ROOT / "link_repos" / "MapleStory-Server" / "wz"
MAP_NAMES_JSON = REPO_ROOT / "link_repos" / ".ref-079max4" / "extracted" / "map079" / "Map.json"

THAI_MAP_IDS = ["500000000", "500010000", "500010100", "500020000",
                "500020100", "500020101", "500020200", "500020300",
                "500020400"]
ORIENT_MAP_IDS = ["701000000", "701000100", "701000200", "701010000",
                  "701010100", "701010200", "701010300", "701010310",
                  "701010320", "701010321", "701010322", "701010323",
                  "701010324", "701010400", "701010500", "701010600",
                  "702000000", "702010000", "702020000", "702030000",
                  "702040000", "702050000", "702070100", "702070200",
                  "702070300", "702070400", "702080000", "702100000"]
NPC_IDS = ["1082100", "9000087", "9300015", "9310031", "9310032",
           "9310033", "9310034", "9310035", "9310036", "9310037",
           "9310038", "9310039", "9310040", "9310041", "9310042",
           "9310043", "9310045", "9310046", "9310047", "9310048",
           "9310049", "9310050", "9310051", "9310052", "9310053",
           "9310055", "9310057", "9310060", "9310082", "9310084",
           "9310085", "9310100", "9330042", "9330078", "9330090"]
MOB_IDS = [str(i) for i in range(9600012, 9600025)]

FM_MAP_XML = HUB_WZ / "Map.wz" / "Map" / "Map9" / "910000000.img.xml"
FM_LIFE_REMOVE_IDS = ("9000020", "9000021")
FM_LIFE_EXPECTED_IDS = ["9030000", "9270052", "9110000", "9201116",
                        "9010000", "9010005", "1100000"]

ID_RE = re.compile(r'<imgdir name="(\d+)"')
ENTRY_RE = re.compile(r'^\s*<imgdir name="(\d+)">\s*$')


# ---------------------------------------------------------------- text utils

def read_text(path: Path) -> str:
    """Raw file content; newline translation is disabled so CRLF survives."""
    return path.read_bytes().decode("utf-8")


def split_lines(text: str) -> list:
    """Split keeping each line's own EOL (handles mixed CRLF/LF files)."""
    parts = text.split("\n")
    lines = [part + "\n" for part in parts[:-1]]
    if parts[-1] != "":
        lines.append(parts[-1])
    return lines


def join_lines(lines: list) -> str:
    return "".join(lines)


def bare(line: str) -> str:
    """Line content without its trailing newline."""
    return re.sub(r"\r?\n$", "", line)


def eol_of(line: str) -> str:
    return "\r\n" if line.endswith("\r\n") else "\n"


def xml_escape(value: str) -> str:
    return (value.replace("&", "&amp;").replace("<", "&lt;")
            .replace(">", "&gt;").replace('"', "&quot;")
            .replace("'", "&apos;"))


def assert_parses(text: str, label: str) -> None:
    try:
        ET.fromstring(text)
    except ET.ParseError as exc:
        raise SystemExit(f"FATAL: patched {label} is not valid XML: {exc}")


def write_atomic(path: Path, text: str) -> None:
    """Write via tmp + os.replace, preserving the destination's mode."""
    data = text.encode("utf-8")
    mode = stat.S_IMODE(path.stat().st_mode)
    fd, tmp = tempfile.mkstemp(dir=str(path.parent), prefix=path.name + ".", suffix=".tmp")
    try:
        with os.fdopen(fd, "wb") as fh:
            fh.write(data)
        os.chmod(tmp, mode)
        os.replace(tmp, path)
    except BaseException:
        if os.path.exists(tmp):
            os.unlink(tmp)
        raise


# ------------------------------------------------------------ map name data

def load_map_names(path: Path) -> dict:
    """Return {map_id: {"streetName": s, "mapName": m}} for target ids."""
    wanted = set(THAI_MAP_IDS) | set(ORIENT_MAP_IDS)
    data = json.loads(path.read_text(encoding="utf-8"))
    names = {}
    for node in data.get("children", []):
        if node.get("name") == "thai":
            for child in node.get("children", []):
                if child.get("name") in wanted:
                    names[child["name"]] = string_fields(child)
        elif node.get("name") in wanted:
            names[node["name"]] = string_fields(node)
    missing = sorted(wanted - set(names))
    if missing:
        raise SystemExit(f"FATAL: Map.json lacks names for: {missing}")
    return names


def string_fields(node: dict) -> dict:
    return {c["name"]: c.get("value", "")
            for c in node.get("children", []) if c.get("kind") == "str"}


# ------------------------------------------------------- String.wz/Map.img

def find_category(lines: list, category: str, indent: str):
    """Locate a direct child <imgdir name=category> block; return (open, close)."""
    opener = indent + '<imgdir name="%s">' % category
    closer = indent + "</imgdir>"
    for i, line in enumerate(lines):
        if bare(line) == opener:
            for j in range(i + 1, len(lines)):
                if bare(lines[j]) == closer:
                    return i, j
            raise SystemExit(f"FATAL: unclosed category {category}")
    return None, None


def render_map_entry(map_id: str, fields: dict, eol: str, indent: str) -> list:
    out = ["%s<imgdir name=\"%s\">%s" % (indent, map_id, eol)]
    for key in ("streetName", "mapName"):
        if key in fields:
            out.append("%s  <string name=\"%s\" value=\"%s\"/>%s"
                       % (indent, key, xml_escape(fields[key]), eol))
    out.append("%s</imgdir>%s" % (indent, eol))
    return out


def patch_map_strings(text: str, category: str, ids: list, names: dict):
    """Append missing map entries to a category; return (text, inserted, skipped)."""
    lines = split_lines(text)
    open_i, close_i = find_category(lines, category, "  ")
    if open_i is None:
        raise SystemExit(f"FATAL: category <{category}> not found")
    present = {m.group(1) for m in (ENTRY_RE.match(bare(l)) for l in lines[open_i:close_i]) if m}
    eol = eol_of(lines[open_i])
    new_lines, inserted, skipped = [], 0, 0
    for map_id in ids:
        if map_id in present:
            skipped += 1
            continue
        new_lines.extend(render_map_entry(map_id, names[map_id], eol, "    "))
        inserted += 1
    if new_lines:
        lines[close_i:close_i] = new_lines
    return join_lines(lines), inserted, skipped


# ---------------------------------------------- String.wz Npc/Mob entry copy

def render_string_entry(elem, eol: str) -> list:
    """Render a source <imgdir name=id> entry in hub formatting (strings only)."""
    out = ['  <imgdir name="%s">%s' % (elem.get("name"), eol)]
    for child in elem:
        if child.tag == "string":
            out.append('    <string name="%s" value="%s"/>%s'
                       % (child.get("name"), xml_escape(child.get("value") or ""), eol))
    out.append("  </imgdir>%s" % eol)
    return out


def patch_string_entries(text: str, ids: list, src_root, label: str):
    """Copy missing entries from the 079 source tree; return (text, stats)."""
    lines = split_lines(text)
    root_close = max(i for i, l in enumerate(lines) if bare(l) == "</imgdir>")
    present = {m.group(1) for m in (ENTRY_RE.match(bare(l)) for l in lines) if m}
    eol = eol_of(lines[0])
    new_lines = []
    inserted = skipped = no_source = 0
    for entry_id in ids:
        if entry_id in present:
            skipped += 1
            continue
        elem = src_root.find('./imgdir[@name="%s"]' % entry_id)
        if elem is None:
            no_source += 1
            print("  NOTE: %s id %s has no entry in 079 source String.wz, skipped" % (label, entry_id))
            continue
        new_lines.extend(render_string_entry(elem, eol))
        inserted += 1
    if new_lines:
        lines[root_close:root_close] = new_lines
    return join_lines(lines), {"inserted": inserted, "skipped": skipped, "no_source": no_source}


# --------------------------------------------------------- FM life surgery

def parse_life_blocks(lines: list, start: int, end: int) -> list:
    """Return child blocks of <life> as dicts {name, id, start, end}."""
    blocks = []
    i = start + 1
    while i < end:
        m = re.match(r'^    <imgdir name="(\d+)">$', bare(lines[i]))
        if not m:
            i += 1
            continue
        j = i
        while j < end and bare(lines[j]) != "    </imgdir>":
            j += 1
        id_m = re.search(r'<string name="id" value="(\d+)"/>', join_lines(lines[i:j + 1]))
        blocks.append({"name": m.group(1), "id": id_m.group(1) if id_m else None,
                       "start": i, "end": j})
        i = j + 1
    return blocks


def patch_fm_life(text: str):
    """Remove retired NPCs from FM life and renumber; return (text, stats)."""
    lines = split_lines(text)
    open_i, close_i = find_category(lines, "life", "  ")
    if open_i is None:
        raise SystemExit("FATAL: <life> section not found in FM map")
    blocks = parse_life_blocks(lines, open_i, close_i)
    before = [b["id"] for b in blocks]
    kept = [b for b in blocks if b["id"] not in FM_LIFE_REMOVE_IDS]
    removed = [b["id"] for b in blocks if b["id"] in FM_LIFE_REMOVE_IDS]
    wanted_names = [str(i) for i in range(len(kept))]
    if not removed and [b["name"] for b in kept] == wanted_names:
        return text, {"changed": False, "before": before, "after": before, "removed": []}
    rebuilt = []
    for idx, block in enumerate(kept):
        chunk = lines[block["start"]:block["end"] + 1]
        chunk[0] = re.sub(r'<imgdir name="\d+">',
                          '<imgdir name="%d">' % idx, chunk[0], count=1)
        rebuilt.extend(chunk)
    out = lines[:open_i + 1] + rebuilt + lines[close_i:]
    return join_lines(out), {"changed": True, "before": before,
                             "after": [b["id"] for b in kept], "removed": removed}


def validate_fm_life(text: str) -> None:
    root = ET.fromstring(text)
    life = root.find('./imgdir[@name="life"]')
    if life is None:
        raise SystemExit("FATAL: no <life> in FM map after patch")
    names = [c.get("name") for c in life]
    ids = [c.find('string[@name="id"]').get("value") for c in life]
    if names != [str(i) for i in range(len(ids))]:
        raise SystemExit(f"FATAL: life children not contiguous: {names}")
    if ids != FM_LIFE_EXPECTED_IDS:
        raise SystemExit(f"FATAL: life ids {ids} != expected {FM_LIFE_EXPECTED_IDS}")


# ---------------------------------------------------------------- driver

def run(check: bool) -> int:
    names = load_map_names(MAP_NAMES_JSON)
    npc_src = ET.parse(SRC_WZ / "String.wz" / "Npc.img.xml").getroot()
    mob_src = ET.parse(SRC_WZ / "String.wz" / "Mob.img.xml").getroot()

    jobs = []  # (label, path, new_text, summary lines)
    map_path = HUB_WZ / "String.wz" / "Map.img.xml"
    text = read_text(map_path)
    parts = []
    for cat, ids in (("elin", THAI_MAP_IDS), ("event", ORIENT_MAP_IDS)):
        text, ins, skip = patch_map_strings(text, cat, ids, names)
        parts.append("%s +%d (skip %d)" % (cat, ins, skip))
    assert_parses(text, str(map_path))
    jobs.append((map_path, text, ["inserted: " + ", ".join(parts)]))

    for label, path, ids, src in (("npc", HUB_WZ / "String.wz" / "Npc.img.xml", NPC_IDS, npc_src),
                                  ("mob", HUB_WZ / "String.wz" / "Mob.img.xml", MOB_IDS, mob_src)):
        text = read_text(path)
        text, stats = patch_string_entries(text, ids, src, label)
        assert_parses(text, str(path))
        jobs.append((path, text, ["inserted: %d (already present %d, no source entry %d)"
                                  % (stats["inserted"], stats["skipped"], stats["no_source"])]))

    fm_text = read_text(FM_MAP_XML)
    fm_text, stats = patch_fm_life(fm_text)
    if stats["changed"]:
        validate_fm_life(fm_text)
    jobs.append((FM_MAP_XML, fm_text,
                 ["removed: %d %s; life %d -> %d entries" % (len(stats["removed"]), stats["removed"],
                                                             len(stats["before"]), len(stats["after"]))]))

    pending = 0
    for path, new_text, summary in jobs:
        changed = new_text != read_text(path)
        pending += changed
        state = "PENDING" if changed else "ok"
        print("[%s] %s" % (state, path.relative_to(REPO_ROOT)))
        for line in summary:
            print("    " + line)
        if changed and not check:
            write_atomic(path, new_text)
            if path == FM_MAP_XML:
                validate_fm_life(read_text(path))
    if check and pending:
        print("%d file(s) pending; run without --check to apply" % pending)
        return 1
    print("apply complete: %d file(s) changed" % pending if not check else "check complete: nothing pending")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true",
                        help="dry-run: report pending changes, exit 1 if any")
    args = parser.parse_args()
    return run(check=args.check)


if __name__ == "__main__":
    sys.exit(main())
