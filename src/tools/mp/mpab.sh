#!/bin/bash
# mpab.sh <name> <tag>=<lines> ... : perf windows in the RUNNING game. Each step appends <lines> ('|' = newline) to
# live_test.txt, settles 10 s, shot, measures 60 s ([vk] fps avg/min, worst frame, sclk, gpu busy).
# FINAL env var (lines, '|' = newline) is applied before live_test.txt is restored.
# e.g. mpab.sh s18c "base=set net 1|set shcast 1" "cast2=set shcast 2"
# Output /home/deck/D/linux_sora/logs/mpab_<name>.{txt,log,<tag>.png}
set -u
name=$1; shift
LIVE=/home/deck/D/SoraBotW/play/live_test.txt
L=/home/deck/D/linux_sora/data/savagewind.log; O=/home/deck/D/linux_sora/logs/mpab_$name
P=$(ps -eo pid,args | grep -E "[g]ame/savagewind" | awk '{print $1}' | head -1)
[ -z "$P" ] && { echo "no game process"; exit 1; }
G=$(ls /sys/class/drm/card*/device/gpu_busy_percent | head -1); D=$(dirname $G)
cp -a $LIVE $LIVE.ab_bak
now_t() { grep '^\[vk\] t=' $L | tail -1 | sed 's/^\[vk\] t=//; s/ .*//'; }
: > $O.txt
for step in "$@"; do
  tag=${step%%=*}; lines=${step#*=}
  echo "$lines|net diag" | tr '|' '\n' >> $LIVE
  sleep 10
  rm -f /tmp/sw_shot.png; gamescopectl screenshot /tmp/sw_shot.png >/dev/null 2>&1; sleep 2
  [ -f /tmp/sw_shot.png ] && cp /tmp/sw_shot.png $O.$tag.png
  t0=$(now_t); : > $O.$tag.clk
  for i in $(seq 30); do echo "$(grep '\*' $D/pp_dpm_sclk | awk '{print $2}' | tr -d 'Mhz') $(cat $G)" >> $O.$tag.clk; sleep 2; done
  t1=$(now_t)
  sh=$(grep "\[net\] avatar" $L | tail -1 | grep -o "shown [01]")
  grep '^\[vk\] t=' $L | sed 's/(//' | awk -v a=$t0 -v b=$t1 -v tag="$tag" '
    { t = substr($2, 3) + 0; if (t <= a || t > b) next; fps = $5 + 0; ms = $8; sub(/ms/, "", ms); d = $9 + 0
      n++; s += fps; if (!mn || fps < mn) mn = fps; if (ms + 0 > mx) mx = ms + 0; dd += d }
    END { printf "%-7s t=%.0f..%.0f (%d): %.2f fps avg, min %.1f, max %d ms, %.0f draws/frame", tag, a, b, n, s / n, mn, mx, dd / s / 2 }' >> $O.txt
  awk -v sh="$sh" '{c += $1; g += $2; n++} END {printf ", sclk %.0f, gpu %.0f%%, avatar %s\n", c / n, g / n, sh}' $O.$tag.clk >> $O.txt
  tail -1 $O.txt
done
echo "${FINAL:-net diag}|net diag" | tr '|' '\n' >> $LIVE; sleep 4
cp -a $LIVE.ab_bak $LIVE; rm -f $LIVE.ab_bak
cp $L $O.log
echo done
