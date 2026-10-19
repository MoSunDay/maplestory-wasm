#!/usr/bin/env python3
"""Extractor for the custom 079MAX4 ``data/*.img`` property/canvas format.

The format is a re-serialized WZ property tree (see
``link_repos/.ref-079max4/extracted/FORMAT.md`` for the byte-level spec):

* 24-byte file header, body is a sequence of property-list records.
* Strings use the classic WZ XOR stream cipher with the KMS key
  (``c[i] = p[i] ^ key[i] ^ (0xAA + i)``, unicode mask ``0xAAAA + i``).
* Canvases are zlib streams of ARGB4444 pixels (2 bytes/pixel).

This tool parses a ``.img`` file, walks the property tree and writes every
canvas as an RGBA PNG next to its animation container name, e.g.
``stand.0.png``.  Pure functions only; no classes, stdlib only.

Usage::

    python3 tools/extract_079_data.py <file.img>... -o <output-dir>
"""

from __future__ import annotations

import argparse
import struct
import sys
import zlib
from pathlib import Path

# ---------------------------------------------------------------------------
# constants
# ---------------------------------------------------------------------------

HEADER_SIZE = 0x18
HEADER_MAGIC = bytes.fromhex("73f851bc8ad8ac1093c2")
HEADER_CONST = bytes.fromhex("fc68a083c709")
LIST_PREAMBLE = bytes.fromhex("1b010000000000")  # 1b 01 00 00 00 00 00

# First 64 bytes of the WZ "key_kms" key used by the 079MAX4 data files.
# Property names/types never exceed a few dozen bytes, but keep some slack.
KMS_KEY = bytes.fromhex(
    "ab65490567cd570a987b870aec65078b9574366cb359295e"
    "a259037ebf7dcb7490532694186f6187d0d6791eac52e952827a1ee80690ef4b03ffd3f3704a39d3"
)

T_NULL, T_U16, T_INT, T_F32, T_F64, T_STR, T_EXT = 0x00, 0x02, 0x03, 0x04, 0x05, 0x08, 0x09

# canvas pixel formats (field f1)
FMT_ARGB4444, FMT_ARGB8888, FMT_RGB565 = 1, 2, 513
BPP = {FMT_ARGB4444: 2, FMT_ARGB8888: 4, FMT_RGB565: 2}


# ---------------------------------------------------------------------------
# primitive readers (each returns (value, next_offset))
# ---------------------------------------------------------------------------

def read_cint(data: bytes, pos: int) -> tuple[int, int]:
    """WZ compact int: i8, or 0x80 flag followed by i32."""
    b = data[pos]
    pos += 1
    if b == 0x80:
        return struct.unpack_from("<i", data, pos)[0], pos + 4
    return (b - 256 if b >= 0x80 else b), pos


def _xor_ascii(raw: bytes) -> str:
    return "".join(chr(c ^ KMS_KEY[i] ^ (0xAA + i)) for i, c in enumerate(raw))


def _xor_unicode(raw: bytes) -> str:
    words = struct.unpack_from("<%dH" % (len(raw) // 2), raw)
    key = struct.unpack_from("<%dH" % (len(raw) // 2), KMS_KEY)
    return "".join(chr(c ^ k ^ (0xAAAA + i)) for i, (c, k) in enumerate(zip(words, key)))


def read_enc_string(data: bytes, pos: int) -> tuple[str, int]:
    """Encrypted string: i8 length byte (negative = ascii, positive = unicode)."""
    length = data[pos] - (256 if data[pos] >= 0x80 else 0)
    pos += 1
    if length > 0:
        if length == 127:
            length = struct.unpack_from("<I", data, pos)[0]
            pos += 4
        raw = data[pos : pos + length * 2]
        return _xor_unicode(raw), pos + length * 2
    if length < 0:
        if length == -128:
            length = -struct.unpack_from("<i", data, pos)[0]
            pos += 4
        raw = data[pos : pos - length]
        return _xor_ascii(raw), pos - length
    return "", pos


def read_prop_string(data: bytes, pos: int) -> tuple[str, int]:
    """Property string: 0x00/0x73 inline, 0x01/0x1b absolute file-offset ref."""
    tag = data[pos]
    pos += 1
    if tag in (0x00, 0x73):
        return read_enc_string(data, pos)
    if tag in (0x01, 0x1B):
        target = struct.unpack_from("<I", data, pos)[0]
        text, _ = read_enc_string(data, target)
        return text, pos + 4
    raise ValueError(f"bad string tag 0x{tag:02x} at 0x{pos - 1:x}")


# ---------------------------------------------------------------------------
# tree readers
# ---------------------------------------------------------------------------

def read_entries(data: bytes, pos: int, count: int) -> tuple[list, int]:
    """Read `count` entries; each is (name, node) where node describes the value."""
    children = []
    for _ in range(count):
        name, pos = read_prop_string(data, pos)
        tag = data[pos]
        pos += 1
        if tag in (T_U16, 0x0B):
            node = {"kind": "u16", "value": struct.unpack_from("<H", data, pos)[0]}
            pos += 2
        elif tag in (T_INT, 0x13):
            value, pos = read_cint(data, pos)
            node = {"kind": "int", "value": value}
        elif tag == T_F32:
            flag = data[pos]
            pos += 1
            value = struct.unpack_from("<f", data, pos)[0] if flag == 0x80 else flag
            node = {"kind": "f32", "value": value}
            pos += 4 if flag == 0x80 else 0
        elif tag == T_F64:
            node = {"kind": "f64", "value": struct.unpack_from("<d", data, pos)[0]}
            pos += 8
        elif tag == T_STR:
            value, pos = read_prop_string(data, pos)
            node = {"kind": "str", "value": value}
        elif tag == T_NULL:
            node = {"kind": "null"}
        elif tag == T_EXT:
            node, pos = read_ext_block(data, pos)
        else:
            raise ValueError(f"bad entry type 0x{tag:02x} at 0x{pos - 1:x} (name {name!r})")
        children.append((name, node))
    return children, pos


def read_ext_block(data: bytes, pos: int) -> tuple[dict, int]:
    """Extended block starting at `pos`; i32 rel-size read by caller precedes."""
    rel = struct.unpack_from("<i", data, pos)[0]
    pos += 4
    start, end = pos, pos + rel
    if data[start : start + 7] == LIST_PREAMBLE:
        count, p = read_cint(data, start + 7)
        children, p = read_entries(data, p, count)
        node = {"kind": "list", "children": children}
    else:
        ext_type, p = read_prop_string(data, start)
        if ext_type == "Shape2D#Vector2D":
            x, p = read_cint(data, p)
            y, p = read_cint(data, p)
            node = {"kind": "vector", "x": x, "y": y}
        elif ext_type == "UOL":
            node = {"kind": "uol", "uol_type": data[p], "target": read_prop_string(data, p + 1)[0]}
            p = start + rel
        elif ext_type == "Canvas":
            node, p = read_canvas(data, p)
        else:
            raise ValueError(f"unsupported extended type {ext_type!r} at 0x{start:x}")
    if p != end:
        raise ValueError(f"block at 0x{start:x}: consumed 0x{p:x}, expected 0x{end:x}")
    return node, end


def read_canvas(data: bytes, pos: int) -> tuple[dict, int]:
    """Canvas block: pad, u8 flag(1), u16 pad, entries, then the bitmap header."""
    pos += 1
    if data[pos] != 1:
        raise ValueError(f"canvas flag 0x{data[pos]:02x} at 0x{pos:x}")
    pos += 3
    count, pos = read_cint(data, pos)
    children, pos = read_entries(data, pos, count)
    width, pos = read_cint(data, pos)
    height, pos = read_cint(data, pos)
    fmt, pos = read_cint(data, pos)
    pos += 1  # format2 flags
    pos += 4  # reserved u32 (0)
    comp_len = struct.unpack_from("<I", data, pos)[0]
    pos += 4
    pos += 1  # leading zero byte; u32 counts it: payload = comp_len - 1
    payload = data[pos : pos + comp_len - 1]
    pos += comp_len - 1
    if fmt not in BPP:
        raise ValueError(f"unsupported canvas format {fmt} at 0x{pos:x}")
    need = width * height * BPP[fmt]
    raw = zlib.decompressobj().decompress(payload)
    if len(raw) != need:
        raise ValueError(f"canvas {width}x{height}: got {len(raw)} px bytes, want {need}")
    return {"kind": "canvas", "children": children, "width": width, "height": height,
            "format": fmt, "pixels": raw}, pos


def parse_img(data: bytes) -> dict:
    """Parse a full .img: header, root property list, trailing root entries."""
    if data[:10] != HEADER_MAGIC or data[0x0E:0x14] != HEADER_CONST:
        raise ValueError("not a 079MAX4 data img (bad header)")
    if data[HEADER_SIZE : HEADER_SIZE + 7] != LIST_PREAMBLE:
        raise ValueError("missing root list preamble at 0x18")
    count, pos = read_cint(data, HEADER_SIZE + 7)
    children, pos = read_entries(data, pos, count)
    while pos < len(data):  # unnamed root-level entries (stand/say/...)
        more, pos = read_entries(data, pos, 1)
        children.extend(more)
    if pos != len(data):
        raise ValueError(f"trailing bytes: stopped at 0x{pos:x} of 0x{len(data):x}")
    return {"kind": "list", "children": children}


# ---------------------------------------------------------------------------
# pixel decoding + PNG output
# ---------------------------------------------------------------------------

def argb4444_to_rgba(raw: bytes, w: int, h: int) -> bytes:
    out = bytearray(w * h * 4)
    for i, v in enumerate(struct.unpack_from("<%dH" % (w * h), raw)):
        out[i * 4 + 0] = ((v >> 8) & 0xF) * 17   # r
        out[i * 4 + 1] = ((v >> 4) & 0xF) * 17   # g
        out[i * 4 + 2] = (v & 0xF) * 17          # b
        out[i * 4 + 3] = ((v >> 12) & 0xF) * 17  # a
    return bytes(out)


def rgb565_to_rgba(raw: bytes, w: int, h: int) -> bytes:
    out = bytearray(w * h * 4)
    for i, v in enumerate(struct.unpack_from("<%dH" % (w * h), raw)):
        out[i * 4 + 0] = ((v >> 11) & 0x1F) * 255 // 31
        out[i * 4 + 1] = ((v >> 5) & 0x3F) * 255 // 63
        out[i * 4 + 2] = (v & 0x1F) * 255 // 31
        out[i * 4 + 3] = 0xFF
    return bytes(out)


def decode_pixels(node: dict) -> bytes:
    w, h, raw = node["width"], node["height"], node["pixels"]
    if node["format"] == FMT_ARGB4444:
        return argb4444_to_rgba(raw, w, h)
    if node["format"] == FMT_RGB565:
        return rgb565_to_rgba(raw, w, h)
    if node["format"] == FMT_ARGB8888:
        return b"".join(raw[i + 2] + raw[i + 1] + raw[i] + raw[i + 3]
                        for i in range(0, len(raw), 4))  # BGRA -> RGBA
    raise ValueError(f"format {node['format']} not supported")


def png_chunk(tag: bytes, payload: bytes) -> bytes:
    return (struct.pack(">I", len(payload)) + tag + payload
            + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))


def encode_png(width: int, height: int, rgba: bytes) -> bytes:
    stride = width * 4
    scanlines = b"".join(b"\x00" + rgba[y * stride : (y + 1) * stride] for y in range(height))
    return (b"\x89PNG\r\n\x1a\n"
            + png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
            + png_chunk(b"IDAT", zlib.compress(scanlines, 9))
            + png_chunk(b"IEND", b""))


# ---------------------------------------------------------------------------
# extraction driver
# ---------------------------------------------------------------------------

def iter_canvases(node: dict, path: str = ""):
    """Yield (dotted_path, canvas_node) for every canvas in the tree."""
    for name, child in node.get("children", []):
        here = f"{path}.{name}" if path else name
        if child["kind"] == "canvas":
            yield here, child
        elif child["kind"] in ("list",):
            yield from iter_canvases(child, here)


def extract_file(img_path: Path, out_dir: Path) -> list[str]:
    data = img_path.read_bytes()
    root = parse_img(data)
    written = []
    for name, canvas in iter_canvases(root):
        png = encode_png(canvas["width"], canvas["height"], decode_pixels(canvas))
        target = out_dir / f"{name}.png"
        target.write_bytes(png)
        written.append(f"{name}.png")
    return written


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Extract 079MAX4 data/*.img canvases to PNG")
    parser.add_argument("imgs", nargs="+", type=Path, help=".img files to extract")
    parser.add_argument("-o", "--out", type=Path, default=Path("extracted"), help="output directory")
    args = parser.parse_args(argv)
    status = 0
    for img in args.imgs:
        out_dir = args.out / img.stem
        out_dir.mkdir(parents=True, exist_ok=True)
        try:
            written = extract_file(img, out_dir)
        except Exception as exc:  # noqa: BLE001 - report and continue with other files
            print(f"{img}: FAILED: {exc}", file=sys.stderr)
            status = 1
            continue
        print(f"{img}: {len(written)} canvas(es) -> {out_dir}")
        for name in written:
            print(f"  {name}")
    return status


if __name__ == "__main__":
    raise SystemExit(main())
