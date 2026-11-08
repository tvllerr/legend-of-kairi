#!/bin/sh
# s160: compare a whole-file recording (runfile.sh) to the PHH oracle, every recording of the file.
#   cmpfile.sh <out dir> <tag = oracle name prefix, e.g. fire_1> [kh2_fx.bin]
# Oracle jsons are made once into recon/s156_work/oraclev/ (phf2v.py). Prints one line per recording + a summary.
OUT="$1"; TAG="$2"; BIN="${3:-%LOCALAPPDATA%/ProjectHeartHunters/cache/kh2_fx.bin}"
R=D:/SoraBotW/recon/s156_work; OV="$R/oraclev"; mkdir -p "$OV"
ok=0; bad=0
while read -r id; do
  n="$TAG.$id"
  [ -s "$OV/$n.json" ] || python -E -P "$R/phf2v.py" "$BIN" "$n" --out "$OV/$n.json" > /dev/null 2>&1 || { echo "$n: NO ORACLE"; bad=$((bad+1)); continue; }
  python -E -P "$R/fxv_cmp.py" "$OUT/$n.json" "$OV/$n.json" > "$OUT/$n.cmp.txt" 2>&1
  r=$(grep -E "RESULT|not 1:1" "$OUT/$n.cmp.txt" | tr '\n' ' ')
  case "$r" in *"ALL MATCH"*) case "$r" in *"not 1:1"*) bad=$((bad+1)); echo "$n: $r";; *) ok=$((ok+1));; esac;; *) bad=$((bad+1)); echo "$n: $r $(grep -m1 -E 'frames ok' "$OUT/$n.cmp.txt")";; esac
done < "$OUT/$TAG.list"
echo "$TAG: $ok ALL MATCH, $bad not"
[ $bad = 0 ]
