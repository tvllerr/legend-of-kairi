#!/bin/bash
# mpperf.sh <name>: multiplayer Task 8 Step 4 Deck perf A/B in the RUNNING game (user placed both Soras, PC Sora in
# view, hands off): ABAB = net on / off / on / off, each 10 s settle + 60 s window, by appending "set net X" to
# live_test.txt (restored at the end). Per window: [vk] fps avg/min, worst frame, draws, sclk. Output
# /home/deck/D/linux_sora/logs/mpperf_<name>.{txt,log}
set -u
name=$1
R=/home/deck/D/SoraBotW; LIVE=$R/play/live_test.txt
L=/home/deck/D/linux_sora/data/savagewind.log; O=/home/deck/D/linux_sora/logs/mpperf_$name
P=$(ps -eo pid,args | grep -E "[g]ame/savagewind" | awk '{print $1}' | head -1)
[ -z "$P" ] && { echo "no game process"; exit 1; }
G=$(ls /sys/class/drm/card*/device/gpu_busy_percent | head -1); D=$(dirname $G)
cp -a $LIVE $LIVE.perf_bak
now_t() { grep '^\[vk\] t=' $L | tail -1 | sed 's/^\[vk\] t=//; s/ .*//'; }
: > $O.txt
for step in "1 on1" "0 off1" "1 on2" "0 off2"; do
  set -- $step; v=$1; tag=$2
  printf "set net %s\nnet diag\n" "$v" >> $LIVE
  sleep 10
  rm -f /tmp/sw_shot.png; gamescopectl screenshot /tmp/sw_shot.png >/dev/null 2>&1; sleep 2
  [ -f /tmp/sw_shot.png ] && cp /tmp/sw_shot.png $O.$tag.png
  t0=$(now_t)
  : > $O.$tag.clk
  for i in $(seq 30); do echo "$(grep '\*' $D/pp_dpm_sclk | awk '{print $2}' | tr -d 'Mhz') $(cat $G)" >> $O.$tag.clk; sleep 2; done
  t1=$(now_t)
  grep '^\[vk\] t=' $L | sed 's/(//' | awk -v a=$t0 -v b=$t1 -v tag=$tag -v net=$v '
    { t = substr($2, 3) + 0; if (t <= a || t > b) next; fps = $5 + 0; ms = $8; sub(/ms/, "", ms); d = $9 + 0
      n++; s += fps; if (!mn || fps < mn) mn = fps; if (ms + 0 > mx) mx = ms + 0; dd += d }
    END { printf "%s net %s t=%.0f..%.0f (%d windows): %.2f fps avg, min %.1f, max %d ms, %.0f draws/frame", tag, net, a, b, n, s / n, mn, mx, dd / s / 2 }' >> $O.txt
  awk '{c += $1; g += $2; n++} END {printf ", sclk %.0f MHz, gpu busy %.0f%%\n", c / n, g / n}' $O.$tag.clk >> $O.txt
  tail -1 $O.txt
done
# leave net ON (the restored file has no "set net" line, so the last applied value stays)
printf "set net 1\nnet diag\n" >> $LIVE; sleep 4
cp -a $LIVE.perf_bak $LIVE; rm -f $LIVE.perf_bak
cp $L $O.log
grep "\[net\] diag\|\[net\] avatar\|\[net\] off\|\[net\] on" $O.log | tail -12
echo done
