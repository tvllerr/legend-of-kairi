#!/bin/sh
# s159: run + solve + compare ONE oracle element (5 camera runs, earlier elements as --pre like PHH's whole-file run).
#   cmpone.sh <oracle name> <fx file> <element> [out dir, default D:/SoraBotW/sb/s159]
# needs recon/s156_work/oraclev_<name>.json (python -E -P recon/s156_work/phf2v.py <kh2_fx.bin> <name> --out ...)
D="$(cd "$(dirname "$0")" && pwd)"; R=D:/SoraBotW
N=$1; F=$2; E=$3; OUT="${4:-$R/sb/s159}"; PRE=""
[ "$E" -gt 0 ] && PRE="--pre $(seq -s, 0 $((E-1)))"
sh "$D/runcams.sh" "$F" "$E" "$OUT" "$N" 600 "$PRE" >/dev/null || { echo "$N RUN FAILED"; exit 1; }
python -E -P "$D/fxsolve.py" "$OUT" "$N" --out "$OUT/$N.json" > "$OUT/$N.solve.txt" || { echo "$N SOLVE FAILED"; exit 1; }
python -E -P "$R/recon/s156_work/fxv_cmp.py" "$OUT/$N.json" "$R/recon/s156_work/oraclev_${N%x}.json" > "$OUT/$N.cmp.txt" 2>&1
echo "== $N"; sed 1d "$OUT/$N.solve.txt"; grep -E "frames ok|RESULT|not 1:1|INCONS" "$OUT/$N.cmp.txt"
