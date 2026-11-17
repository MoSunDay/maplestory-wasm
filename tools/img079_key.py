"""WZ ``key_kms`` XOR-cipher table used by the 079MAX4 packed ``data/*.img`` files.

``tools/data/key_kms.bin`` holds the full ``key_kms[0x20000]`` table from
``.cache/tools/NoLifeWzToNx/Keys.cpp`` (the C array leaves its last entry
uninitialised, so it is stored as 0x00).  The table used to live here as a
base64-encoded 0x8000-byte prefix, which was enough for Map/Npc imgs but not
for ``UI/Login.img``: its 800x600 canvases use a chunked zlib layout whose
chunks reach 25 KB and restart the XOR stream at key offset 0.

Regenerate from the converter clone with::

    python3 - <<'PY'
    import re
    from pathlib import Path
    src = Path('.cache/tools/NoLifeWzToNx/Keys.cpp').read_text()
    body = re.search(r'key_kms\\[0x20000\\] = \\{(.*?)\\};', src, re.S).group(1)
    key = bytes(int(v, 16) for v in re.findall(r'0x([0-9a-fA-F]{1,2})', body))
    Path('tools/data/key_kms.bin').write_bytes(key + b'\\0' * (0x20000 - len(key)))
    PY
"""
from pathlib import Path

KMS_KEY = (Path(__file__).resolve().parent / 'data' / 'key_kms.bin').read_bytes()
