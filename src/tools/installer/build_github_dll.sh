#!/bin/bash
# (s223) sora_mod.dll for the GitHub release: built WITHOUT the Melty connector (NO_MELTY=1: runtime/sora_net_sock.h's no-op
# stand-ins; nothing of runtime/melty/ is compiled in), into its own build folder, then checked with meltycheck.py.
# Dev PC only (mkzip SKIP_TOOLS). Never touches the tree's build/ or the exe.
#
#   sh tools/installer/build_github_dll.sh <out dir> [<savagewind.dll.a of the release exe>] [melty]
#
#   <out dir>/build-nomelty/sora_mod.dll   the DLL for mkzip --track github --dll ...
#   melty (3rd arg): also <out dir>/build-melty/sora_mod.dll (WITH the connector) and <out dir>/melty_only_strings.txt =
#   the strings only the connector puts in (used once, s223, to fill meltycheck.BIN_MARKERS).
# The import library must be the one made with the release's savagewind.exe (the DLL links its exports by name);
# default: D:/SoraBotW/src/wt211-sw053/build/savagewind.dll.a (the 0.5.3 line's exe build, read only: it is copied).
set -e
export PATH=/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH
TREE=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
OUT=${1:?usage: build_github_dll.sh <out dir> [implib] [melty]}
IMPLIB=${2:-D:/SoraBotW/src/wt211-sw053/build/savagewind.dll.a}
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
case "$OUT" in "$TREE"|"$TREE"/*) echo "build_github_dll: the out dir must be outside the tree" >&2; exit 1 ;; esac
[ -f "$IMPLIB" ] || { echo "build_github_dll: no import library $IMPLIB" >&2; exit 1; }

build() {  # $1 = folder name, rest = make args
    local B="$OUT/$1"; shift
    mkdir -p "$B"
    cp -f "$IMPLIB" "$B/savagewind.dll.a"
    rm -f "$B/sora_mod.dll"
    (cd "$TREE" && make -o "$B/savagewind.exe" B="$B" "$@" "$B/sora_mod.dll") > "$B/make.log" 2>&1 || {
        tail -20 "$B/make.log" >&2; echo "build_github_dll: make FAILED ($B/make.log)" >&2; exit 1; }
    [ -f "$B/sora_mod.dll" ] || { echo "build_github_dll: no $B/sora_mod.dll" >&2; exit 1; }
}

build build-nomelty NO_MELTY=1
D="$OUT/build-nomelty/sora_mod.dll"
if objdump -p "$D" | grep -qi "libstdc++\|libgcc_s\|libwinpthread"; then
    echo "build_github_dll: $D imports a C++ / gcc runtime DLL (it should be plain C)" >&2; exit 1
fi
PY=${PY:-python}
"$PY" "$TREE/tools/installer/meltycheck.py" "$OUT/build-nomelty/sora_mod.dll"
if [ "$3" = "melty" ]; then
    build build-melty
    strings -n 8 "$OUT/build-melty/sora_mod.dll" | sort -u > "$OUT/strings-melty.txt"
    strings -n 8 "$D" | sort -u > "$OUT/strings-nomelty.txt"
    comm -23 "$OUT/strings-melty.txt" "$OUT/strings-nomelty.txt" > "$OUT/melty_only_strings.txt"
    echo "build_github_dll: $(wc -l < "$OUT/melty_only_strings.txt") strings only in the Melty build -> $OUT/melty_only_strings.txt"
fi
echo "build_github_dll: OK $D ($(stat -c %s "$D") bytes, sha256 $(sha256sum "$D" | cut -c1-16)..., tree $(git -C "$TREE" rev-parse --short HEAD))"
