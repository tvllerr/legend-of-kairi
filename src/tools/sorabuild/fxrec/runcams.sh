#!/bin/sh
# s158: the 5 runs of the camera trick for one element -> <out>/<tag>_{A,B,C,X,Z}.fxd (+ .txt logs)
#   runcams.sh <fx file> <element> <out dir> <tag> [frames] [extra fxrec args, e.g. "--pre 0,1,2"]
# A identity, B yaw 37 pitch 23, C yaw -61 pitch -14 (generic angles), X / Z = identity + owner moved 1/frame on x / z.
FX="$1"; EL="$2"; OUT="$3"; TAG="$4"; FR="${5:-600}"; EXTRA="$6"
D="$(cd "$(dirname "$0")" && pwd)"; EXE="$D/build/fxrec_boot.exe"
TEX=D:/KH2Extract/kh2_first/original/effect/texcommon.dpd
mkdir -p "$OUT"; rm -f "$OUT/${TAG}.failed"
# s160: HD texture tables (hd_lookup answers) for texcommon + the effect file; HD=0 runs without them (old behaviour)
if [ "${HD:-1}" = 1 ]; then
  python -E -P "$D/hdtab.py" "$TEX" --out "$OUT/${TAG}.texhd" && python -E -P "$D/hdtab.py" "$FX" --out "$OUT/${TAG}.fxhd" || exit 1
  echo "${FX#*/original/}" > "$OUT/${TAG}.fxname"
  EXTRA="$EXTRA --texhd $OUT/${TAG}.texhd --fxhd $OUT/${TAG}.fxhd"
fi
run() { "$EXE" --tex "$TEX" --fx "$FX" --elem "$EL" --frames "$FR" --dump "$OUT/${TAG}_$1.fxd" $EXTRA $2 > "$OUT/${TAG}_$1.txt" 2>&1 || { echo "run $1 FAILED (see $OUT/${TAG}_$1.txt)"; touch "$OUT/${TAG}.failed"; }; }
run A "" & run B "--cam 37,23" & run C "--cam -61,-14" & run X "--owner x" & run Z "--owner z" & wait
tail -n 1 "$OUT/${TAG}"_?.txt | grep -v "^$"
[ ! -e "$OUT/${TAG}.failed" ] || exit 1
