#!/bin/sh
# s160: whole-file recording (PHH worker order): every element id sorted, one process per camera run, loop-end tails.
#   runfile.sh <fx file> <out dir> <tag> [extra fxrec args, e.g. "--tails 120" / "--pool 0" / "--isotails 1"]
# -> <out>/<tag>.<id>_{A,B,C,X,Z}.fxd (+ <id>~<at> for tails), <out>/<tag>_<cam>.txt, then fxsolve per recording
#    -> <out>/<tag>.<id>.json; <out>/<tag>.list = the recordings (id or id~at); <out>/<tag>.texsrc = "<recording> <run
#    txt>" = whose TEXID table the recording's tex ids index. Exit 1 on a failed run / solve.
# s161 --isotails 1 (King Mickey P_EX200 / P_EX220, PHH's ISOLATED_TAIL_FILES): the main runs print NEEDTAILS <id> <n>
#    for an alive element; its tails are then recorded in 5 FRESH processes (--tailsof id,n) -> <tag>_t<id>_<cam>.txt.
FX="$1"; OUT="$2"; TAG="$3"; EXTRA="$4"
D="$(cd "$(dirname "$0")" && pwd)"; EXE="${FXREC_BOOT:-$D/build/fxrec_boot.exe}"
KHX="${FXREC_KH2:-D:/KH2Extract}"; PY="${FXREC_PYTHON:-python}"   # sorabuild sets FXREC_KH2 / FXREC_PYTHON / FXREC_GAMEEXE
TEX="$KHX/kh2_first/original/effect/texcommon.dpd"
mkdir -p "$OUT"; rm -f "$OUT/${TAG}.failed"
"$PY" -E -P "$D/hdtab.py" "$TEX" --out "$OUT/${TAG}.texhd" && "$PY" -E -P "$D/hdtab.py" "$FX" --out "$OUT/${TAG}.fxhd" || exit 1
echo "${FX#*/original/}" > "$OUT/${TAG}.fxname"
# run <cam> <cam args> [<txt name> <more args>]: one process; its txt = <out>/<txt name>_<cam>.txt (default <tag>)
run() { T="${3:-$TAG}"; "$EXE" --tex "$TEX" --texhd "$OUT/${TAG}.texhd" --fxhd "$OUT/${TAG}.fxhd" --fx "$FX" --frames 600 \
        --all "$OUT/${TAG}.%s_$1.fxd" $EXTRA $2 $4 ${FXREC_GAMEEXE:+"$FXREC_GAMEEXE"} > "$OUT/${T}_$1.txt" 2>&1 \
        && grep -q "^FILE DONE" "$OUT/${T}_$1.txt" || { echo "run ${T}_$1 FAILED (see $OUT/${T}_$1.txt)"; touch "$OUT/${TAG}.failed"; }; }
cams() { run A "" "$@" & run B "--cam 37,23" "$@" & run C "--cam -61,-14" "$@" & run X "--owner x" "$@" & run Z "--owner z" "$@" & wait; }
# same <txt name>: the 5 runs must have recorded the same elements / tails for the same frame counts
# (s166) temp files, not bash's <( ): plain POSIX sh, so busybox-w32 ash runs it too (Deck / Proton bake)
same() { grep -E "^(ELEMENT|TAIL|NEEDTAILS) " "$OUT/${1}_A.txt" > "$OUT/${1}.sameA"
for c in B C X Z; do
  grep -E "^(ELEMENT|TAIL|NEEDTAILS) " "$OUT/${1}_$c.txt" > "$OUT/${1}.same$c"
  cmp -s "$OUT/${1}.sameA" "$OUT/${1}.same$c" \
    || { echo "run ${1}_$c recorded other elements / lengths than run ${1}_A"; return 1; }
  rm -f "$OUT/${1}.same$c"
done; rm -f "$OUT/${1}.sameA"; }
cams
[ ! -e "$OUT/${TAG}.failed" ] || exit 1
same "$TAG" || exit 1
: > "$OUT/${TAG}.list"; : > "$OUT/${TAG}.texsrc"
grep -E "^(ELEMENT|TAIL|NEEDTAILS) " "$OUT/${TAG}_A.txt" > "$OUT/${TAG}.recs"
while read -r K ID N REST; do
  case "$K" in
    ELEMENT|TAIL) echo "$ID" >> "$OUT/${TAG}.list"; echo "$ID ${TAG}_A.txt" >> "$OUT/${TAG}.texsrc";;
    NEEDTAILS)
      cams "${TAG}_t$ID" "--tailsof $ID,$N" < /dev/null
      [ ! -e "$OUT/${TAG}.failed" ] || exit 1
      same "${TAG}_t$ID" || exit 1
      for r in $(grep -E "^TAIL " "$OUT/${TAG}_t${ID}_A.txt" | awk '{print $2}'); do
        echo "$r" >> "$OUT/${TAG}.list"; echo "$r ${TAG}_t${ID}_A.txt" >> "$OUT/${TAG}.texsrc"; done;;
  esac
done < "$OUT/${TAG}.recs"
echo "$(wc -l < "$OUT/${TAG}.list") recordings"
fail=0
cat "$OUT/${TAG}.list" | xargs -P ${SOLVEP:-8} -I{} sh -c '"$3" -E -P "$0/fxsolve.py" "$1" "$2.{}" --out "$1/$2.{}.fxv" > "$1/$2.{}.solve.txt" 2>&1 || echo "SOLVE FAILED {}"' "$D" "$OUT" "$TAG" "$PY" > "$OUT/${TAG}.solvefail0"
# (s167) a failed solve gets ONE more try, alone: under Wine/Proton a parallel spawn can fail ("python.exe: Invalid argument",
# Deck s166 lim_goofy2 3~72) = a launch error, not the data. A real failure fails again and is reported as before.
: > "$OUT/${TAG}.solvefail"
while read -r _s _f ID; do
  echo "solve retry $ID (once, alone)"
  "$PY" -E -P "$D/fxsolve.py" "$OUT" "$TAG.$ID" --out "$OUT/$TAG.$ID.fxv" > "$OUT/$TAG.$ID.solve.txt" 2>&1 \
    || echo "SOLVE FAILED $ID" | tee -a "$OUT/${TAG}.solvefail"
done < "$OUT/${TAG}.solvefail0"
rm -f "$OUT/${TAG}.solvefail0"
[ -s "$OUT/${TAG}.solvefail" ] && exit 1
exit 0
