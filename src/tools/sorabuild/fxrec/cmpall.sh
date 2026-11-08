#!/bin/sh
# s160: the 12 oracle regression cases (s157-s160), one after another (each = 5 parallel runs + solve + compare).
#   cmpall.sh [out dir, default D:/SoraBotW/sb/s160]   -> one RESULT line per case, exit 1 if any is not ALL MATCH
D="$(cd "$(dirname "$0")" && pwd)"; X=D:/KH2Extract; OUT="${1:-D:/SoraBotW/sb/s160}"; bad=0
while read -r N F E; do
  [ -n "$N" ] || continue
  r=$(sh "$D/cmpone.sh" "$N" "$X/$F" "$E" "$OUT" 2>&1 | grep -E "RESULT|FAILED|not 1:1" | tr '\n' ' ')
  echo "$N: $r"; case "$r" in *"ALL MATCH"*) case "$r" in *"not 1:1"*) bad=1;; esac;; *) bad=1;; esac
done <<LIST
fire_1.1x kh2_fifth/original/magic/FIRE_1.mag 1
w_ex010.1x kh2_first/original/obj/W_EX010.a.us 1
sora.14x kh2_sixth/original/obj/P_EX100.a.us 14
fire_1.8 kh2_fifth/original/magic/FIRE_1.mag 8
valor.18 kh2_sixth/original/obj/P_EX100_BTLF.a.us 18
magnet_1.4 kh2_fifth/original/magic/MAGNET_1.mag 4
magnet_1.1 kh2_fifth/original/magic/MAGNET_1.mag 1
cure_3.4 kh2_fifth/original/magic/CURE_3.mag 4
thunder_3.4 kh2_fifth/original/magic/THUNDER_3.mag 4
anti.24 kh2_sixth/original/obj/P_EX100_HTLF.a.us 24
fire_2.3 kh2_fifth/original/magic/FIRE_2.mag 3
thunder_1.2 kh2_fifth/original/magic/THUNDER_1.mag 2
LIST
[ $bad = 0 ] && echo "ALL 12 MATCH" || { echo "SOME CASES FAIL"; exit 1; }
