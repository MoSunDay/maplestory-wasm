#!/bin/bash

# Collect runtime resources (web site, wasm build, .nx assets, service binaries)
# into desktop/resources/ for electron-builder extraResources packaging.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DESKTOP_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$DESKTOP_DIR/.." && pwd)"

BIN_DIR="$REPO_ROOT/target/release"
TARGET=""
OUT_DIR="$DESKTOP_DIR/resources"

usage() {
    echo "Usage: $0 [--bin-dir <dir>] [--target <platformTag>] [--out <dir>]"
    echo "  platformTag: linux-x64 linux-arm64 darwin-x64 darwin-arm64 windows-x64 windows-arm64"
}

detect_target() {
    local os arch
    os="$(uname -s)"
    arch="$(uname -m)"
    case "$os:$arch" in
        Linux:x86_64) echo "linux-x64" ;;
        Linux:aarch64 | Linux:arm64) echo "linux-arm64" ;;
        Darwin:x86_64) echo "darwin-x64" ;;
        Darwin:arm64) echo "darwin-arm64" ;;
        MINGW*:x86_64 | CYGWIN*:x86_64 | MSYS*:x86_64 | MINGW*:AMD64 | CYGWIN*:AMD64 | MSYS*:AMD64) echo "windows-x64" ;;
        *) return 1 ;;
    esac
}

die() {
    echo "error: $1" >&2
    exit 1
}

while [ $# -gt 0 ]; do
    case "$1" in
        --bin-dir) BIN_DIR="$2"; shift 2 ;;
        --target) TARGET="$2"; shift 2 ;;
        --out) OUT_DIR="$2"; shift 2 ;;
        --help | -h) usage; exit 0 ;;
        *) echo "error: unknown option: $1" >&2; usage >&2; exit 1 ;;
    esac
done

if [ -z "$TARGET" ]; then
    TARGET="$(detect_target)" || die "cannot detect platform tag for $(uname -s)/$(uname -m); pass --target explicitly"
fi

EXE=""
case "$TARGET" in
    windows-*) EXE=".exe" ;;
esac

# --- validate inputs ---
[ -f "$REPO_ROOT/web/index.html" ] || die "missing $REPO_ROOT/web/index.html"
[ -f "$REPO_ROOT/build/JourneyClient.js" ] || die "missing $REPO_ROOT/build/JourneyClient.js"
[ -f "$REPO_ROOT/build/JourneyClient.wasm" ] || die "missing $REPO_ROOT/build/JourneyClient.wasm"
[ -d "$BIN_DIR" ] || die "missing binary directory $BIN_DIR"
for svc in web-server ws-proxy assets-server; do
    [ -f "$BIN_DIR/$svc$EXE" ] || die "missing service binary $BIN_DIR/$svc$EXE (target: $TARGET)"
done

# --- assemble ---
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR/msweb/build" "$OUT_DIR/msweb/assets" "$OUT_DIR/bin/$TARGET"

cp -R "$REPO_ROOT/web" "$OUT_DIR/msweb/web"
rm -rf "$OUT_DIR/msweb/web/tests"
# config.json is virtualized at runtime by the Electron shell (see main/config-inject.js);
# a stale static copy would apply wrong connection settings if interception ever failed.
rm -f "$OUT_DIR/msweb/web/config.json"

cp -f "$REPO_ROOT/build/JourneyClient.js" "$OUT_DIR/msweb/build/"
cp -f "$REPO_ROOT/build/JourneyClient.wasm" "$OUT_DIR/msweb/build/"

NX_COUNT=0
if [ -d "$REPO_ROOT/assets" ]; then
    for nx in "$REPO_ROOT/assets"/*.nx; do
        [ -f "$nx" ] || continue
        cp -f "$nx" "$OUT_DIR/msweb/assets/"
        NX_COUNT=$((NX_COUNT + 1))
    done
fi
if [ "$NX_COUNT" -eq 0 ]; then
    echo "WARNING: no local .nx assets bundled; users must configure an assets server URL" >&2
fi

for svc in web-server ws-proxy assets-server; do
    cp -f "$BIN_DIR/$svc$EXE" "$OUT_DIR/bin/$TARGET/"
done

# --- summary ---
echo "assembled $OUT_DIR (target: $TARGET, nx files: $NX_COUNT)"
du -sh "$OUT_DIR/msweb" "$OUT_DIR/bin"
find "$OUT_DIR" -maxdepth 3 -type f | sort | sed -n '1,40p'
