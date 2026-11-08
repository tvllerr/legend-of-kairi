#!/bin/sh
# make_netbot.sh: builds the net bot (+ the test sender) into build_mp4/ of this tree (or $1). Run from anywhere.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
OUT=${1:-$ROOT/build_mp4}
export PATH=/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH
mkdir -p "$OUT"
clang -O2 -Wall -o "$OUT/netbot.exe" "$HERE/netbot.c" -lws2_32
clang -O2 -Wall -o "$OUT/fakesend.exe" "$HERE/test/fakesend.c" -lws2_32
echo "built $OUT/netbot.exe $OUT/fakesend.exe"
