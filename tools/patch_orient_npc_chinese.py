#!/usr/bin/env python3
"""Idempotent Chinese localisation of the orient/Thai NPC fallback strings.

Rewrites the ``<string name="..."/>`` fields of the NPC entries listed
in ``NPC_ZH`` (tools/npc_chinese_dialogue.py) inside
link_repos/MapleStory-Server/wz/String.wz/Npc.img.xml to Chinese.  Run
it after the 0021 fallback patch (tools/patch_orient_server_wz.py) so
the 079 Shanghai/Shaolin and Thailand NPCs no longer show the English
fallback lines.

The edit is line-oriented text surgery: unrelated bytes, entry order,
indentation and each line's own EOL style are preserved, and a field
whose value already matches is left untouched, so re-running is a
no-op.  ``--check`` is a dry run that exits 1 while changes are
pending; validation failures (npc id without an entry, or a listed
field missing from its entry) exit 2.
"""

from __future__ import annotations

import argparse
import os
import re
import stat
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from npc_chinese_dialogue import NPC_ZH  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[1]
NPC_XML = (REPO_ROOT / "link_repos" / "MapleStory-Server" / "wz" /
           "String.wz" / "Npc.img.xml")

ENTRY_RE = re.compile(r'^(\s*)<imgdir name="(\d+)">\s*$')
STRING_RE = re.compile(r'^(\s*)<string name="(\w+)" value="([^"]*)"/>(\r?\n?)$')


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
            .replace(">", "&gt;").replace('"', "&quot;"))


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
        os.replace(tmp, str(path))
    finally:
        if os.path.exists(tmp):
            os.unlink(tmp)


def fatal(msg: str) -> None:
    print("FATAL: " + msg, file=sys.stderr)
    raise SystemExit(2)


# ------------------------------------------------------------------- patcher

def find_blocks(lines: list) -> dict:
    """Map npc id -> (open_i, close_i, indent) for every listed entry.

    The closing ``</imgdir>`` is matched at the opening line's own
    indent, so entries nested elsewhere in the file cannot fool us.
    """
    wanted = set(NPC_ZH)
    blocks = {}
    for i, line in enumerate(lines):
        m = ENTRY_RE.match(bare(line))
        if not m or m.group(2) not in wanted or m.group(2) in blocks:
            continue
        indent, npc_id = m.group(1), m.group(2)
        closer = indent + "</imgdir>"
        close_i = next((j for j in range(i + 1, len(lines))
                        if bare(lines[j]) == closer), None)
        if close_i is None:
            fatal(f"npc {npc_id} entry at line {i + 1} has no matching </imgdir>")
        blocks[npc_id] = (i, close_i, indent)
    return blocks


def patch_text(text: str) -> tuple:
    """Swap in the Chinese field values; return (new_text, {npc_id: count})."""
    lines = split_lines(text)
    blocks = find_blocks(lines)
    missing = [npc_id for npc_id in NPC_ZH if npc_id not in blocks]
    if missing:
        fatal("no <imgdir> entry in Npc.img.xml for npc id(s): " + ", ".join(missing))
    stats = {}
    for npc_id, fields in NPC_ZH.items():
        open_i, close_i, _indent = blocks[npc_id]
        seen, updates = set(), {}
        for i in range(open_i + 1, close_i):
            m = STRING_RE.match(bare(lines[i]))
            if not m or m.group(2) not in fields:
                continue
            field, indent = m.group(2), m.group(1)
            seen.add(field)
            new_line = ('%s<string name="%s" value="%s"/>%s'
                        % (indent, field, xml_escape(fields[field]), eol_of(lines[i])))
            if new_line != lines[i]:
                updates[i] = new_line
        absent = [field for field in fields if field not in seen]
        if absent:
            fatal("npc %s has no <%s> string field(s) in its Npc.img.xml entry"
                  % (npc_id, "/".join(absent)))
        for i, new_line in updates.items():
            lines[i] = new_line
        stats[npc_id] = len(updates)
    return join_lines(lines), stats


def run(check: bool) -> int:
    text = read_text(NPC_XML)
    new_text, stats = patch_text(text)
    changed = {npc_id: n for npc_id, n in stats.items() if n}
    dirty = new_text != text
    print("[%s] %s" % ("PENDING" if dirty else "ok", NPC_XML.relative_to(REPO_ROOT)))
    for npc_id in sorted(changed):
        print("    %s %s: %d fields updated"
              % (npc_id, NPC_ZH[npc_id].get("name", ""), changed[npc_id]))
    print("%d npc entries, %d fields updated" % (len(changed), sum(changed.values())))
    if check:
        if dirty:
            print("run without --check to apply")
            return 1
        print("check complete: nothing pending")
        return 0
    if not dirty:
        print("apply complete: nothing to do")
        return 0
    assert_parses(new_text, str(NPC_XML))
    write_atomic(NPC_XML, new_text)
    fresh = read_text(NPC_XML)
    again, restats = patch_text(fresh)
    if again != fresh or sum(restats.values()):
        raise SystemExit("FATAL: Npc.img.xml still dirty after write")
    print("apply complete: %d npc entries, %d fields updated"
          % (len(changed), sum(changed.values())))
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true",
                        help="dry-run: report pending changes, exit 1 if any")
    args = parser.parse_args()
    return run(check=args.check)


if __name__ == "__main__":
    sys.exit(main())
