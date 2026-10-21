#!/usr/bin/env python3
"""Tolerant parser for 079MAX4 ``data/*.img`` property/canvas trees.

``tools/extract_079_data.py`` was reverse-engineered from ``Npc/*.img``
samples only (see ``link_repos/.ref-079max4/extracted/FORMAT.md``).  The
Map/String/Obj/Back imgs add three container quirks this module handles by
re-using the same primitive readers (copied verbatim, the source file is
frozen):

1. the 6 bytes at 0x0E are *not* constant across files (only Npc imgs share
   ``fc68a083c709``), so only the 10-byte magic is validated;
2. the root property list does not always start at 0x18 -- the header length
   varies (0x15/0x18/0x1B/0x1D observed) and the root list may additionally
   be wrapped in a ``[0x09][i32 rel]`` extended block whose declared size can
   overshoot EOF.  The root preamble is therefore *located* by scanning for
   ``1b 01 00 00 00 00 00`` after 0x0E and validated by parsing the whole
   file to an exact EOF;
3. canvas blocks may carry flag 0x00: no ``u16`` pad and no property list,
   width/height/format/bitmap follow immediately (observed on the
   ``910000000.img`` miniMap canvas, 246x143 fmt=2).

Pure functions only; stdlib only.  ``parse_img(data)`` returns the same node
shape as ``extract_079_data.parse_img`` (see that module / FORMAT.md).
"""
from __future__ import annotations

import struct
import zlib

HEADER_MAGIC = bytes.fromhex("73f851bc8ad8ac1093c2")
LIST_PREAMBLE = bytes.fromhex("1b010000000000")  # 1b 01 00 00 00 00 00

# WZ "key_kms" XOR-cipher key (2048-byte prefix) -- see img079_key.py for
# provenance; String/Map.img needs >64 bytes for long UTF-16 event dialogs.
from img079_key import KMS_KEY

T_NULL, T_U16, T_INT, T_F32, T_F64, T_STR, T_EXT = 0x00, 0x02, 0x03, 0x04, 0x05, 0x08, 0x09

FMT_ARGB4444, FMT_ARGB8888, FMT_RGB565 = 1, 2, 513
BPP = {FMT_ARGB4444: 2, FMT_ARGB8888: 4, FMT_RGB565: 2}

SCAN_START, SCAN_LIMIT = 0x0E, 0x100


# ---------------------------------------------------------------------------
# primitive readers (copied from extract_079_data.py)
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
# tree readers (canvas extended with the flag==0 no-properties layout)
# ---------------------------------------------------------------------------

def read_canvas(data: bytes, pos: int) -> tuple[dict, int]:
    """Canvas block: pad, u8 flag, [u16 pad + property list when flag==1],
    then the bitmap header."""
    pos += 1  # pad
    flag = data[pos]
    pos += 1
    children: list = []
    if flag == 1:
        pos += 2  # u16 pad
        count, pos = read_cint(data, pos)
        children, pos = read_entries(data, pos, count)
    elif flag != 0:
        raise ValueError(f"canvas flag 0x{flag:02x} at 0x{pos - 1:x}")
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
    if len(raw) < need:
        raise ValueError(f"canvas {width}x{height}: got {len(raw)} px bytes, want {need}")
    # Map-family imgs inflate to exactly 2*w*h*4 with the image in the first
    # w*h*bpp bytes and the rest zero padding (observed on every canvas of
    # Obj/Back/Map9 910000000, all fmt=2); Npc imgs are exact.  Truncate.
    return {"kind": "canvas", "children": children, "width": width, "height": height,
            "format": fmt, "pixels": raw[:need]}, pos


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
        elif ext_type == "Shape2D#Convex2D":
            # convex polygon: [cint count] then count x ([type string, often an
            # absolute offset ref][cint x][cint y]); block-end validated above
            # (observed on Obj/chewchewIsland, e.g. block 0x93141 rel=0x47)
            count, p = read_cint(data, p)
            children = []
            for i in range(count):
                vtype, p = read_prop_string(data, p)
                if vtype != "Shape2D#Vector2D":
                    raise ValueError(f"bad convex member {vtype!r} at 0x{p:x}")
                x, p = read_cint(data, p)
                y, p = read_cint(data, p)
                children.append((str(i), {"kind": "vector", "x": x, "y": y}))
            node = {"kind": "convex", "children": children}
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


# ---------------------------------------------------------------------------
# root location + driver
# ---------------------------------------------------------------------------

def parse_from(data: bytes, start: int) -> tuple[list, int]:
    """Parse [root list at `start`] + unnamed root entries to EOF (best effort)."""
    count, pos = read_cint(data, start + 7)
    children, pos = read_entries(data, pos, count)
    while pos < len(data):  # unnamed root-level entries (stand/say/miniMap/...)
        more, pos = read_entries(data, pos, 1)
        children.extend(more)
    return children, pos


def iter_roots(data: bytes):
    """Candidate root-list offsets: LIST_PREAMBLE hits after the magic/key."""
    limit = min(len(data), SCAN_LIMIT)
    return [i for i in range(SCAN_START, limit) if data[i : i + 7] == LIST_PREAMBLE]


def parse_img(data: bytes) -> dict:
    """Parse a full .img; returns {"kind": "list", "children": [(name, node)]]}.

    The root preamble offset varies across img families, so every candidate
    is validated by a full parse; the first one consuming exactly to EOF wins.
    """
    if data[:10] != HEADER_MAGIC:
        raise ValueError("not a 079MAX4 data img (bad magic)")
    problems = []
    for start in iter_roots(data):
        try:
            children, pos = parse_from(data, start)
        except Exception as exc:  # noqa: BLE001 - try the next candidate
            problems.append(f"root@0x{start:x}: {exc}")
            continue
        if pos == len(data):
            return {"kind": "list", "children": children}
        problems.append(f"root@0x{start:x}: stopped at 0x{pos:x} of 0x{len(data):x}")
    detail = "; ".join(problems) or "no root list preamble found after 0x0e"
    raise ValueError(f"unparsable 079MAX4 img ({detail})")
