#!/bin/bash
# mpprof.sh <name>: perf record 15 s net on / 15 s net off in the RUNNING game (live_test.txt restored, net left on).
# Output /home/deck/D/linux_sora/logs/mpprof_<name>.{on,off}.{data,dso,sym,thr}
set -u
name=$1; LIVE=/home/deck/D/SoraBotW/play/live_test.txt; O=/home/deck/D/linux_sora/logs/mpprof_$name
P=$(ps -eo pid,args | grep -E "[g]ame/savagewind" | awk '{print $1}' | head -1)
[ -z "$P" ] && { echo "no game process"; exit 1; }
cp -a $LIVE $LIVE.prof_bak
for step in "1 on" "0 off"; do
  set -- $step
  printf "set net %s\n" "$1" >> $LIVE; sleep 10
  top -H -b -n 1 -p $P | head -22 > $O.$2.thr
  perf record -F 999 -g -p $P -o $O.$2.data -- sleep 15 2> $O.$2.err
  perf report -i $O.$2.data --no-children --sort comm,dso -q 2>/dev/null | head -30 > $O.$2.dso
  perf report -i $O.$2.data --no-children --sort comm,sym -q 2>/dev/null | head -80 > $O.$2.sym
  perf report -i $O.$2.data --no-children --dso sora_mod.so --sort sym -q 2>/dev/null | head -40 > $O.$2.sora
done
printf "set net 1\nnet diag\n" >> $LIVE; sleep 4
cp -a $LIVE.prof_bak $LIVE; rm -f $LIVE.prof_bak
grep "\[net\] diag" /home/deck/D/linux_sora/data/savagewind.log | tail -1
echo done
