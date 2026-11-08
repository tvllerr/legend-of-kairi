#!/bin/sh
# s182 proof that data/ragmap/*.bin == the arrays that used to be typed into runtime/sora_ragmap*.h (commit f2c1995 and before).
#   sh proof.sh <built data dir containing ragmap/> [scratch dir]      (dev only; needs clang + git, run inside the repo)
set -e
OUT="$1"; T="${2:-proof_tmp}"; HERE=$(cd "$(dirname "$0")" && pwd -W 2>/dev/null || pwd); REPO=$(git -C "$HERE" rev-parse --show-toplevel)
mkdir -p "$T/old" && cd "$T"
export TMP="$(pwd -W 2>/dev/null || pwd)" TEMP="$(pwd -W 2>/dev/null || pwd)"   # (clang needs a Windows temp dir)
for f in sora_ragmap sora_ragmap_roxas sora_ragmap_mickey; do git -C "$REPO" show f2c1995:runtime/$f.h > old/$f.h; done
clang -Wall -O1 -o dump_old.exe "$HERE/dump_old.c" -I. && ./dump_old.exe
clang -Wall -O1 -I"$REPO/runtime" -include stdio.h -o dump_new.exe "$HERE/dump_new.c" && ./dump_new.exe "$OUT/ragmap"
for c in sora roxas mickey; do cmp old_$c.bin "$OUT/ragmap/$c.bin" && cmp new_$c.bin "$OUT/ragmap/$c.bin" && echo "$c IDENTICAL"; done
