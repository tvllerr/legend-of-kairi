#!/bin/sh
# s160: whole-file runs vs the PHH oracle for a list of files (runfile.sh + cmpfile.sh, fxd dumps deleted after).
#   allfiles.sh <list> <out root> [filter regex]
# list lines: "<kh2 file name> <oracle prefix> <oracle tail count>". s161: loopend.py decides --tails 120 (the oracle
# count is only cross-checked: "TAILS DISAGREE"); obj/PRIZE.mdlx element 0 = pool-fed: --pool 0; ISOLATED files
# (King Mickey P_EX200 / P_EX220, PHH: tails in a fresh process per element) get --isotails.
# -> <out root>/<prefix>/, summary lines "FILE <name>: ...".
D="$(cd "$(dirname "$0")" && pwd)"; LIST="$1"; ROOT="$2"; FLT="${3:-.}"
mkdir -p "$ROOT"
grep -E "$FLT" "$LIST" | while read -r NAME PRE NT; do
  F=""; for p in kh2_first kh2_second kh2_third kh2_fourth kh2_fifth kh2_sixth; do
    [ -f "D:/KH2Extract/$p/original/$NAME" ] && F="D:/KH2Extract/$p/original/$NAME"; done
  [ -n "$F" ] || { echo "FILE $NAME: NOT IN KH2Extract"; continue; }
  case "$PRE" in *,*) echo "FILE $NAME: several prefixes ($PRE) - skipped"; continue;; esac
  LE=$(python -E -P "$D/loopend.py" "$NAME" | sed -n 's/.* loop_end=\([01]\).*/\1/p')
  [ "$LE" = 1 ] || [ "$LE" = 0 ] || { echo "FILE $NAME: loopend.py failed"; continue; }
  OT=0; [ "$NT" -gt 0 ] && OT=1
  [ "$LE" = "$OT" ] || echo "FILE $NAME: TAILS DISAGREE (loopend $LE, oracle $NT tails)"
  X=""; [ "$LE" = 1 ] && X="--tails 120"; [ "$NAME" = obj/PRIZE.mdlx ] && X="$X --pool 0"
  case "$NAME" in obj/P_EX200.a.us|obj/P_EX220.a.us) X="$X --isotails 1";; esac
  O="$ROOT/$PRE"; t0=$(date +%s)
  if sh "$D/runfile.sh" "$F" "$O" "$PRE" "$X" > "$O.run.txt" 2>&1; then
    r=$(sh "$D/cmpfile.sh" "$O" "$PRE" 2>&1 | tail -1)
  else r="RUN FAILED: $(tail -2 "$O.run.txt" | tr '\n' ' ')"; fi
  rm -f "$O"/*.fxd
  echo "FILE $NAME: $r ($(( $(date +%s) - t0 )) s)"
done
