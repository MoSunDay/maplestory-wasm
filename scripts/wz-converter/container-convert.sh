#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="/work"
WZ_DIR="$ROOT_DIR/wz"
NX_DIR="$ROOT_DIR/nxFiles"
CACHE_DIR="$ROOT_DIR/.cache/tools"
SRC_DIR="$CACHE_DIR/NoLifeWzToNx"
BIN_DIR="$CACHE_DIR/bin"
BIN_PATH="$BIN_DIR/NoLifeWzToNx"

REPO_URL="${NO_LIFE_WZ_TO_NX_REPO:-https://github.com/ryantpayton/NoLifeWzToNx}"
REPO_REF="${NO_LIFE_WZ_TO_NX_REF:-master}"
SKIP_LIST_WZ="${SKIP_LIST_WZ:-1}"
MODE="${MODE:-client}" # client|server
LZ4HC="${LZ4HC:-0}"

mkdir -p "$CACHE_DIR" "$BIN_DIR" "$NX_DIR"

shopt -s nullglob
wz_files=("$WZ_DIR"/*.wz)
if [[ ${#wz_files[@]} -eq 0 ]]; then
  echo "No WZ files found in: $WZ_DIR" >&2
  exit 1
fi

# Optional WZ_FILTER: space-separated WZ basenames to convert (unset = all).
# Example: WZ_FILTER="String.wz Etc.wz UI.wz"
WZ_FILTER="${WZ_FILTER:-}"
if [[ -n "$WZ_FILTER" ]]; then
  selected=()
  for wz in "${wz_files[@]}"; do
    base="$(basename "$wz")"
    keep=0
    for f in $WZ_FILTER; do
      if [[ "$base" == "$f" ]]; then
        keep=1
        break
      fi
    done
    if [[ "$keep" == "1" ]]; then
      selected+=("$wz")
    fi
  done
  if [[ ${#selected[@]} -eq 0 ]]; then
    echo "WZ_FILTER '$WZ_FILTER' matched no WZ files in: $WZ_DIR" >&2
    exit 1
  fi
  wz_files=("${selected[@]}")
  echo "WZ_FILTER active: converting only: ${WZ_FILTER}"
fi

if [[ ! -d "$SRC_DIR/.git" ]]; then
  echo "Cloning NoLifeWzToNx into $SRC_DIR"
  git clone "$REPO_URL" "$SRC_DIR"
fi

pushd "$SRC_DIR" >/dev/null

echo "Fetching latest from origin"
git fetch --all --tags --prune

echo "Checking out ref: $REPO_REF"
git checkout "$REPO_REF"
if git rev-parse --verify --quiet "origin/$REPO_REF" >/dev/null; then
  git reset --hard "origin/$REPO_REF"
fi

# Upstream currently includes <filesystem> but aliases std::experimental::filesystem.
# Patch in-container for modern GCC toolchains.
if grep -q "std::experimental::filesystem" NoLifeWzToNx.cpp; then
  sed -i 's/std::experimental::filesystem/std::filesystem/g' NoLifeWzToNx.cpp
fi
# Harden string conversion: malformed decoded text should not abort conversion.
if grep -q "return convert.to_bytes(ptr, ptr + p_str.size());" NoLifeWzToNx.cpp; then
  perl -0777 -i -pe "s@return convert\\.to_bytes\\(ptr, ptr \\+ p_str\\.size\\(\\)\\);@try { return convert.to_bytes(ptr, ptr + p_str.size()); } catch (...) { std::string fallback; fallback.reserve(p_str.size()); for (auto ch : p_str) fallback.push_back(ch < 0x80 ? static_cast<char>(ch) : '?'); return fallback; }@g" NoLifeWzToNx.cpp
fi
# Harden bitmap processing: some modern WZ bitmaps fail strict size checks.
# Fallback to blank bitmap data instead of aborting the entire conversion.
if grep -q "throw std::runtime_error(\"halp!\");" NoLifeWzToNx.cpp; then
  perl -0777 -i -pe "s@if \\(check != pixels \\* 4\\) \\{\\s*std::cerr << \"Size mismatch: \" << std::dec << width << \",\" << height << \",\" << decompressed << \",\" << f1 << \",\" << f2 << std::endl;\\s*throw std::runtime_error\\(\"halp!\"\\);\\s*\\}@if (check != pixels * 4) { std::cerr << \"Size mismatch: \" << std::dec << width << \",\" << height << \",\" << decompressed << \",\" << f1 << \",\" << f2 << std::endl; f1 = 2; f2 = 0; decompressed = size; std::fill(output.begin(), output.begin() + size, '\\\\0'); }@g" NoLifeWzToNx.cpp
fi

# Fix CJK string destruction (probed 2026-10 on wz/{String,Etc,UI}.wz):
# the 2-byte encrypted strings hold genuine UTF-16 (e.g. Mob.img/100100/name
# units 8717 725B = U+8717 U+725B = "蜗牛"). Upstream convert_str() does
# reinterpret_cast<wchar_t const*>(u16string::c_str()) - on Linux wchar_t is
# 32-bit, so two UTF-16 units merge into one bogus scalar (0x725B8717) and
# codecvt throws for every non-ASCII string, which the hardened fallback then
# flattens to '?'. Route the raw UTF-16LE bytes through iconv first instead.
# ASCII-only output stays byte-identical (UTF-16LE iconv is ASCII-transparent).
if ! grep -q "include <iconv.h>" NoLifeWzToNx.cpp; then
  perl -0777 -i -pe 's@(#include <lz4hc\.h>)@$1\n#include <iconv.h>@' NoLifeWzToNx.cpp
fi
if ! grep -q "wz_iconv" NoLifeWzToNx.cpp; then
  perl -0777 -i -pe 's@(// The main class itself)@static std::string wz_iconv(char const * enc, char const * data, size_t len) {\n    std::string out;\n    if (len == 0) return out;\n    iconv_t cd = iconv_open("UTF-8", enc);\n    if (cd == reinterpret_cast<iconv_t>(-1)) return out;\n    std::vector<char> buf(len * 4 + 16);\n    char * ip = const_cast<char *>(data);\n    char * op = buf.data();\n    size_t ileft = len;\n    size_t oleft = buf.size();\n    size_t rc = iconv(cd, \&ip, \&ileft, \&op, \&oleft);\n    iconv_close(cd);\n    if (rc == static_cast<size_t>(-1) || ileft != 0) return out;\n    out.assign(buf.data(), buf.size() - oleft);\n    return out;\n}\n$1@' NoLifeWzToNx.cpp
fi
if ! grep -q "UTF-16LE" NoLifeWzToNx.cpp; then
  perl -0777 -i -pe 's@(auto ptr = reinterpret_cast<wchar_t const \*>\(p_str\.c_str\(\)\);)@{\n            std::string raw;\n            raw.resize(p_str.size() * 2);\n            if (!p_str.empty()) std::memcpy(\&raw[0], p_str.data(), raw.size());\n            auto utf8 = wz_iconv("UTF-16LE", raw.data(), raw.size());\n            if (!utf8.empty()) return utf8;\n        }\n        $1@' NoLifeWzToNx.cpp
fi

echo "Building converter binary (g++)"
g++ -std=c++17 -O2 -Iincludes/libsquish \
  NoLifeWzToNx.cpp Keys.cpp includes/libsquish/*.cpp \
  -lz -llz4 -o "$BIN_PATH"

popd >/dev/null

if [[ ! -x "$BIN_PATH" ]]; then
  echo "Failed to build converter: $BIN_PATH not found" >&2
  exit 1
fi

echo "Using converter binary: $BIN_PATH"

# Clean stale outputs in target dir (only the selected subset when filtering)
if [[ -n "$WZ_FILTER" ]]; then
  for wz in "${wz_files[@]}"; do
    base="$(basename "$wz")"
    rm -f "$NX_DIR/${base%.wz}.nx" "$WZ_DIR/${base%.wz}.nx"
  done
else
  rm -f "$NX_DIR"/*.nx
fi

for wz in "${wz_files[@]}"; do
  base="$(basename "$wz")"
  if [[ "$base" == "List.wz" && "$SKIP_LIST_WZ" == "1" ]]; then
    echo "Skipping $base (non-PKG1 metadata file)"
    continue
  fi

  echo "Converting $base"
  args=("$wz")
  if [[ "$MODE" == "server" ]]; then
    args+=("-s")
  else
    args+=("-c")
  fi
  if [[ "$LZ4HC" == "1" ]]; then
    args+=("-h")
  fi

  "$BIN_PATH" "${args[@]}"

done

# Move produced .nx files (tool writes next to input WZ); only the selected subset
for wz in "${wz_files[@]}"; do
  base="$(basename "$wz")"
  nx="$WZ_DIR/${base%.wz}.nx"
  if [[ -f "$nx" ]]; then
    cp -f "$nx" "$NX_DIR"/
  else
    echo "Warning: expected output not found: $nx" >&2
  fi
done

echo "Done. NX files in $NX_DIR:"
ls -lh "$NX_DIR"/*.nx
