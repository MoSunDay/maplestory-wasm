#!/usr/bin/env python3
"""Dump the full 079MAX4 ``data/*.img`` property tree as JSON.

Reuses :func:`extract_079_data.parse_img` (repo tools dir is put on sys.path)
and converts the in-memory tree to a JSON-able shape:

* ``{"name", "kind": "u16|int|f32|f64|str", "value"}``   scalars
* ``{"name", "kind": "null"}``
* ``{"name", "kind": "vector", "x", "y"}``
* ``{"name", "kind": "uol", "target", "uol_type"}``
* ``{"name", "kind": "canvas", "width", "height", "format", "pixels",
   "children"}``  -- ``pixels`` is the raw decompressed blob (pre colour
   conversion, ARGB4444/565 = 2 B/px, ARGB8888 = 4 B/px) written to
   ``<outdir>/pixels/<sanitized tree path>.bin``
* ``{"name", "kind": "convex", "children"}``  -- Shape2D#Convex2D polygon,
   children are auto-numbered vector points
* ``{"name", "kind": "list", "children": [...]}``

Parsing uses ``extract_079_data.parse_img`` (Npc-style imgs) and falls back
to ``img079_parser.parse_img`` (Map/String/Obj/Back body-layout quirks:
variable header length, optional root ext-block wrapper, canvas flag 0x00,
Convex2D, 2x zero-padded canvas streams).

Output file is ``<outdir>/<sanitized-stem>.json``; when two inputs share a
stem (e.g. Obj/chewchewIsland + Back/chewchewIsland) the parent directory
name is prepended.  One summary line per img (nodes/canvases/UOLs) goes to
stdout.  ``--summary`` additionally writes ``<stem>.summary.json`` with the
map-level facts needed for the 910000000 replica (info fields, per-layer
obj/tile counts, back/portal/ladderRope/foothold/life, miniMap size).

Pure functions only; stdlib only.  Usage::

    python3 tools/dump_079_tree.py <file.img>... -o <outdir> [--summary]
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools"))

from extract_079_data import parse_img as parse_img_strict  # noqa: E402
from img079_parser import parse_img as parse_img_any  # noqa: E402


def parse_tree(data: bytes) -> dict:
    """Strict Npc-style parse first, then the tolerant Map/String variant."""
    try:
        return parse_img_strict(data)
    except Exception:  # noqa: BLE001 - header/body quirks of Map-family imgs
        return parse_img_any(data)

UNSAFE = re.compile(r"[^0-9A-Za-z_.\-]+")
SCALARS = ("u16", "int", "f32", "f64", "str")
MAX_PART = 80
PORTAL_KEYS = ("pn", "pt", "x", "y", "tm", "tn", "script")


# --------------------------------------------------------------------------
# naming helpers
# --------------------------------------------------------------------------

def sanitize(text: str) -> str:
    """Make one path component filesystem-safe and bounded."""
    cleaned = UNSAFE.sub("_", text).strip("._")
    return (cleaned[:MAX_PART] or "n")


def unique(base: str, used: dict) -> str:
    """Return `base` (or base~N) not yet present in `used`; records it."""
    if base not in used:
        used[base] = 1
        return base
    used[base] += 1
    return f"{base}~{used[base]}"


def output_stem(img: Path, taken: set) -> str:
    """Stem for the JSON file; disambiguate collisions with parent dir name."""
    stem = sanitize(img.stem)
    if stem not in taken:
        taken.add(stem)
        return stem
    for parent in img.parents:
        alt = f"{sanitize(parent.name)}.{stem}"
        if alt not in taken:
            taken.add(alt)
            return alt
    n = 2
    while f"{stem}.{n}" in taken:
        n += 1
    taken.add(f"{stem}.{n}")
    return f"{stem}.{n}"


# --------------------------------------------------------------------------
# tree -> JSON conversion
# --------------------------------------------------------------------------

def build(name: str, node: dict, parts: tuple, pixels: list, used: dict) -> dict:
    """Convert a parser node to the JSON shape; collects canvas blobs."""
    kind = node["kind"]
    out = {"name": name, "kind": kind}
    if kind in SCALARS:
        out["value"] = node["value"]
    elif kind == "vector":
        out["x"] = node["x"]
        out["y"] = node["y"]
    elif kind == "uol":
        out["target"] = node["target"]
        out["uol_type"] = node["uol_type"]
    elif kind == "canvas":
        out["width"] = node["width"]
        out["height"] = node["height"]
        out["format"] = node["format"]
        base = unique(".".join(sanitize(p) for p in parts), used)
        pixels.append((f"{base}.bin", node["pixels"]))
        out["pixels"] = f"pixels/{base}.bin"
        out["children"] = [build(n, c, parts + (n,), pixels, used)
                           for n, c in node.get("children", [])]
    elif kind == "list":
        out["children"] = [build(n, c, parts + (n,), pixels, used)
                           for n, c in node.get("children", [])]
    elif kind == "convex":
        out["children"] = [build(n, c, parts + (n,), pixels, used)
                           for n, c in node.get("children", [])]
    return out


def count_stats(node: dict) -> tuple:
    """(total nodes, canvases, uols) for a JSON-shape tree."""
    total = 1
    canvas = 1 if node["kind"] == "canvas" else 0
    uol = 1 if node["kind"] == "uol" else 0
    for child in node.get("children", []):
        t, c, u = count_stats(child)
        total += t
        canvas += c
        uol += u
    return total, canvas, uol


# --------------------------------------------------------------------------
# map summary helpers (operate on JSON-shape trees)
# --------------------------------------------------------------------------

def entries(node):
    return node.get("children", []) if node else []


def child(node, name):
    return next((c for c in entries(node) if c.get("name") == name), None)


def value_of(node):
    """Compact JSON-able value for any non-list node; lists become dicts."""
    kind = node.get("kind")
    if kind in SCALARS:
        return node["value"]
    if kind == "vector":
        return {"x": node["x"], "y": node["y"]}
    if kind == "uol":
        return {"uol": node["target"]}
    if kind == "canvas":
        return {"canvas": [node["width"], node["height"]]}
    if kind == "list":
        return {c["name"]: value_of(c) for c in entries(node)}
    return None


def fields(node):
    return {c["name"]: value_of(c) for c in entries(node)}


def layer_summary(root):
    out = {}
    for node in entries(root):
        if node["kind"] == "list" and node["name"].isdigit():
            out[node["name"]] = {
                "obj": len(entries(child(node, "obj"))),
                "tile": len(entries(child(node, "tile"))),
            }
    return out


def portal_summary(root):
    portals = [fields(p) for p in entries(child(root, "portal"))]
    picked = [{k: p[k] for k in PORTAL_KEYS if k in p} for p in portals]
    return {"count": len(portals), "list": picked}


def ladder_summary(root):
    ladders = [fields(l) for l in entries(child(root, "ladderRope"))]
    return {"count": len(ladders), "list": ladders}


def foothold_summary(root):
    layers, total = {}, 0
    for layer in entries(child(root, "foothold")):
        segs = [fields(s) for g in entries(layer) for s in entries(g)]
        xs = [v for s in segs for v in (s.get("x1"), s.get("x2"))
              if isinstance(v, (int, float))]
        ys = [v for s in segs for v in (s.get("y1"), s.get("y2"))
              if isinstance(v, (int, float))]
        layers[layer["name"]] = {
            "count": len(segs),
            "x": [min(xs), max(xs)] if xs else None,
            "y": [min(ys), max(ys)] if ys else None,
        }
        total += len(segs)
    return {"total": total, "layers": layers}


def life_summary(root):
    lives = [fields(l) for l in entries(child(root, "life"))]
    return {"count": len(lives), "by_type": dict(Counter(
        l.get("type") for l in lives))}


def minimap_summary(root):
    mm = child(root, "miniMap")
    if mm is None:
        return None
    if mm["kind"] == "canvas":
        return {"width": mm["width"], "height": mm["height"],
                "format": mm["format"], "pixels": mm.get("pixels")}
    for sub in entries(mm):  # flattened layout: miniMap -> {canvas, width, ...}
        if sub["kind"] == "canvas":
            return {"width": sub["width"], "height": sub["height"],
                    "format": sub["format"], "pixels": sub.get("pixels"),
                    "fields": {c["name"]: value_of(c) for c in entries(mm)
                               if c["kind"] != "canvas"}}
    return {"note": f"unexpected kind {mm['kind']}"}


KNOWN_SECTIONS = frozenset((
    "miniMap", "back", "life", "portal", "ladderRope", "foothold",
    "reactor", "clock", "seat", "shipObj", "skill", "weather",
))


def info_summary(root):
    """info fields; supports both layouts: an ``info`` child list, or the
    flattened custom format where the root list itself holds the info
    fields and sections follow as anonymous root entries."""
    explicit = child(root, "info")
    if explicit is not None:
        return fields(explicit)
    return {c["name"]: value_of(c) for c in entries(root)
            if c["name"] not in KNOWN_SECTIONS and not c["name"].isdigit()
            and c["kind"] != "canvas"}


def summarize(root, map_id):
    return {
        "map": map_id,
        "sections": [c["name"] for c in entries(root)],
        "info": info_summary(root),
        "miniMap": minimap_summary(root),
        "layers": layer_summary(root),
        "back": {"count": len(entries(child(root, "back")))},
        "portal": portal_summary(root),
        "ladderRope": ladder_summary(root),
        "foothold": foothold_summary(root),
        "life": life_summary(root),
    }


# --------------------------------------------------------------------------
# driver
# --------------------------------------------------------------------------

def dump_img(img: Path, out_dir: Path, want_summary: bool, taken: set) -> str:
    """Parse `img`, write JSON (+pixels, +summary); returns the log line."""
    root = parse_tree(img.read_bytes())
    stem = output_stem(img, taken)
    pixels, used = [], {}
    tree = build(stem, root, (stem,), pixels, used)
    pix_dir = out_dir / "pixels"
    pix_dir.mkdir(parents=True, exist_ok=True)
    for fname, blob in pixels:
        (pix_dir / fname).write_bytes(blob)
    json_path = out_dir / f"{stem}.json"
    json_path.write_text(json.dumps(tree, ensure_ascii=False, indent=1),
                         encoding="utf-8")
    extra = ""
    if want_summary:
        spath = out_dir / f"{stem}.summary.json"
        spath.write_text(
            json.dumps(summarize(tree, img.stem), ensure_ascii=False, indent=1),
            encoding="utf-8")
        extra = f" summary->{spath.name}"
    total, canvas, uol = count_stats(tree)
    return f"{img.name} -> {stem}.json: nodes={total} canvases={canvas} uols={uol}{extra}"


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description="Dump 079MAX4 .img property trees to JSON")
    parser.add_argument("imgs", nargs="+", type=Path, help=".img files to dump")
    parser.add_argument("-o", "--out", type=Path, default=Path("dump079"),
                        help="output directory")
    parser.add_argument("--summary", action="store_true",
                        help="also write <stem>.summary.json (map structure)")
    args = parser.parse_args(argv)
    args.out.mkdir(parents=True, exist_ok=True)
    taken, status = set(), 0
    for img in args.imgs:
        try:
            print(dump_img(img, args.out, args.summary, taken), flush=True)
        except Exception as exc:  # noqa: BLE001 - report, keep other files
            print(f"{img}: FAILED: {exc}", file=sys.stderr)
            status = 1
    return status


if __name__ == "__main__":
    raise SystemExit(main())
