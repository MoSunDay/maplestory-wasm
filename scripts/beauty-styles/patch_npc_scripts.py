#!/usr/bin/env python3
"""Idempotently replace the per-city cosmetic style pools in MapleStory-Server
NPC scripts with the FULL style sets from beauty_styles.json.

Scope (mapping of the `var <name> = Array(...)` declaration names):
  mhair*, hair  -> male_hair     fhair*, fhair -> female_hair
  mface*, face  -> male_face     fface*, fface -> female_face
Never touched: skin, and runtime accumulators (hairnew/facenew/haircolor/colors).
Bare names (hair/fhair/face/fface, only used by GM all-in stylist 9900000.js)
are only remapped when the existing array already holds cosmetic-looking ids.

Both `Array(a, b, c)` and `[a, b, c]` declaration styles are supported (also
when the array spans multiple lines); every replacement is written back as a
single line, preserving the Array()/[] spelling of the original declaration.

Modes:
  default   patch scripts/npc/*.js in place (idempotent: re-running reports 0)
  --check   verify every patched style array: non-empty, no duplicate ids,
            comma/paren balanced, and exactly equal to the full set
"""

import json
import os
import re
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
DEFAULT_NPC_DIR = os.path.join(
    REPO_ROOT, "link_repos", "MapleStory-Server", "scripts", "npc")
STYLES_JSON = os.path.join(SCRIPT_DIR, "beauty_styles.json")

# The 47 beauty salon scripts known to carry pushIfItemExists-style pools.
KNOWN_TARGETS = [
    "1012103.js", "1012104.js", "1012117.js", "1052004.js", "1052005.js",
    "1052100.js", "1052101.js", "2010001.js", "2010002.js", "2012007.js",
    "2012009.js", "2040019.js", "2041007.js", "2041009.js", "2041010.js",
    "2090100.js", "2090101.js", "2090103.js", "2090104.js", "2100005.js",
    "2100006.js", "2100008.js", "2100009.js", "9120100.js", "9120101.js",
    "9120102.js", "9120103.js", "9200100.js", "9200101.js", "9200102.js",
    "9201015.js", "9201016.js", "9201017.js", "9201018.js", "9201019.js",
    "9201039.js", "9201062.js", "9201063.js", "9201064.js", "9201069.js",
    "9201070.js", "9270023.js", "9270024.js", "9270026.js", "9270036.js",
    "9270037.js", "9900000.js",
]

PREFIX_MAP = (
    ("mhair", "male_hair"),
    ("fhair", "female_hair"),
    ("mface", "male_face"),
    ("fface", "female_face"),
)
BARE_MAP = {
    "hair": "male_hair",
    "fhair": "female_hair",
    "face": "male_face",
    "fface": "female_face",
}
NEVER_TOUCH = {"skin", "colors", "hairnew", "facenew", "haircolor", "skinnew"}

DECL_RE = re.compile(
    r"(?P<indent>[ \t]*)var\s+(?P<name>[A-Za-z_$][A-Za-z0-9_$]*)\s*=\s*"
    r"(?P<call>Array\s*\((?P<cargs>[^()]*)\)|\[(?P<bargs>[^\[\]]*)\])\s*;",
    re.M,
)
ID_RE = re.compile(r"^\s*-?\d+\s*$")


def parse_ids(raw):
    """Parse a comma-separated id list; None when not a plain int list."""
    if not raw.strip():
        return []
    parts = raw.split(",")
    if not all(ID_RE.match(p) for p in parts):
        return None
    return [int(p) for p in parts]


def is_cosmetic_id(item_id):
    return 20000 <= item_id < 22000 or 30000 <= item_id < 35000


def style_key_for(var_name, current_ids):
    """Map a declaration name to a full-set key, or None to leave it alone."""
    if var_name in NEVER_TOUCH:
        return None
    if var_name in BARE_MAP:
        # Bare names are only style pools when they already hold cosmetic ids.
        if current_ids and all(is_cosmetic_id(i) for i in current_ids):
            return BARE_MAP[var_name]
        return None
    for prefix, key in PREFIX_MAP:
        if var_name.startswith(prefix):
            return key
    return None


def render(var_name, ids, use_array_call, indent):
    body = ", ".join(str(i) for i in ids)
    inner = "Array(%s)" % body if use_array_call else "[%s]" % body
    return "%svar %s = %s;" % (indent, var_name, inner)


def patch_text(text, styles):
    """Return (new_text, changes, matched) for one script file."""
    changes = []
    matched = [0]

    def repl(m):
        var_name = m.group("name")
        raw = m.group("cargs") if m.group("cargs") is not None else m.group("bargs")
        ids = parse_ids(raw)
        key = style_key_for(var_name, ids) if ids is not None else None
        if key is None:
            return m.group(0)
        matched[0] += 1
        full = styles[key]
        if ids == full:
            return m.group(0)  # already patched -> idempotent no-op
        changes.append((var_name, key, ids, full))
        use_array_call = m.group("call").startswith("Array")
        return render(var_name, full, use_array_call, m.group("indent"))

    return DECL_RE.sub(repl, text), changes, matched[0]


def patch_files(npc_dir, styles):
    known_set = set(KNOWN_TARGETS)
    report = {}
    for fname in sorted(os.listdir(npc_dir)):
        if not fname.endswith(".js"):
            continue
        path = os.path.join(npc_dir, fname)
        # newline='' keeps \r\n / \n endings byte-exact on read and write.
        with open(path, encoding="utf-8", errors="surrogateescape",
                  newline="") as fh:
            text = fh.read()
        new_text, changes, matched = patch_text(text, styles)
        if changes:
            with open(path, "w", encoding="utf-8",
                      errors="surrogateescape", newline="") as fh:
                fh.write(new_text)
        if changes or matched or fname in known_set:
            report[fname] = (changes, matched)
    return report


def check_files(npc_dir, styles):
    """Structural + content verification of the patched style arrays.

    Only asserts on declarations whose names map to a style pool (the arrays
    this tool owns); unrelated string arrays in the same scripts are skipped,
    as are runtime accumulators (hairnew/colors/...) and skin arrays.
    """
    failures = []
    checked = 0
    for fname in sorted(KNOWN_TARGETS):
        path = os.path.join(npc_dir, fname)
        if not os.path.isfile(path):
            failures.append("%s: missing file" % fname)
            continue
        with open(path, encoding="utf-8", errors="surrogateescape",
                  newline="") as fh:
            text = fh.read()
        for m in DECL_RE.finditer(text):
            var_name = m.group("name")
            raw = m.group("cargs") if m.group("cargs") is not None else m.group("bargs")
            ids = parse_ids(raw)
            key = style_key_for(var_name, ids) if ids is not None else None
            if key is None:
                continue
            checked += 1
            if ids is None:
                failures.append("%s: %s -> malformed id list" % (fname, var_name))
                continue
            if "\n" in m.group(0):
                failures.append("%s: %s -> not single-line" % (fname, var_name))
            if not ids:
                failures.append("%s: %s -> EMPTY style array" % (fname, var_name))
                continue
            if len(ids) != len(set(ids)):
                failures.append("%s: %s -> duplicate ids" % (fname, var_name))
            if ids != styles[key]:
                missing = sorted(set(styles[key]) - set(ids))
                extra = sorted(set(ids) - set(styles[key]))
                failures.append("%s: %s != %s (missing %s extra %s)"
                                % (fname, var_name, key, missing, extra))
    return checked, failures


def main():
    npc_dir = sys.argv[2] if len(sys.argv) > 2 and not sys.argv[2].startswith("-") \
        else DEFAULT_NPC_DIR
    with open(STYLES_JSON, encoding="utf-8") as fh:
        styles = {k: list(v) for k, v in json.load(fh).items()}

    if "--check" in sys.argv:
        checked, failures = check_files(npc_dir, styles)
        print("checked %d style arrays across target scripts" % checked)
        if failures:
            print("FAILURES (%d):" % len(failures))
            for line in failures:
                print("  " + line)
            sys.exit(1)
        print("all style arrays OK: non-empty, no dupes, equal to full sets")
        return

    report = patch_files(npc_dir, styles)
    total_arrays = 0
    no_match = []
    print("=== per-file replacements (name: before -> after ids) ===")
    for fname, (changes, matched) in sorted(report.items()):
        if not changes:
            if matched == 0:
                no_match.append(fname)
            continue
        total_arrays += len(changes)
        detail = ", ".join("%s %d->%d" % (n, len(b), len(a)) for n, _, b, a in changes)
        print("%s: %d array(s) [%s]" % (fname, len(changes), detail))
    print("=== summary ===")
    print("files patched: %d, style arrays replaced: %d"
          % (sum(1 for c, _ in report.values() if c), total_arrays))
    if no_match:
        print("WARNING: %d known target file(s) matched no style array:" % len(no_match))
        for fname in no_match:
            print("  " + fname)
    discovered = sorted(f for f, (c, _) in report.items()
                        if f not in KNOWN_TARGETS)
    if discovered:
        print("note: patched files outside the known target list: %s" % discovered)


if __name__ == "__main__":
    main()
