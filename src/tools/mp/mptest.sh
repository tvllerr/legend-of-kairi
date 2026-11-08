#!/bin/bash
# mptest.sh <name>: multiplayer Task 5 check (no avatar): ab save copy, then live set net 1 / netloop 1 / net diag /
# scripted attacks / set net 0. Restores live_test.txt + deck_native.txt. Output /home/deck/D/linux_sora/logs/mp_<name>.*
set -u
name=$1
R=/home/deck/D/SoraBotW; LIVE=$R/play/live_test.txt; NT=$R/play/deck_native.txt; ENVF=/home/deck/D/linux_test/deck_native_env.sh
L=/home/deck/D/linux_sora/data/savagewind.log; O=/home/deck/D/linux_sora/logs/mp_$name
cp -a $LIVE $LIVE.mp_bak; cp -a $NT $NT.mp_bak
printf "sora\nab\n" > $NT
echo "export SAVAGEWIND_SORA_NET_NAME=Deck" > $ENVF
steam steam://rungameid/11564611641114886144 >/dev/null 2>&1 &
P=
for i in $(seq 60); do
  P=$(ps -eo pid,args | grep -E "[g]ame/savagewind" | awk "{print \$1}" | head -1)
  [ -n "$P" ] && break; sleep 1
done
[ -z "$P" ] && { echo "no game process"; cp -a $LIVE.mp_bak $LIVE; cp -a $NT.mp_bak $NT; rm -f $ENVF; exit 1; }
echo "pid $P"
sleep 5
until grep -q "^\[vk\] t=11[0-9]." $L || ! ps -p $P >/dev/null; do sleep 2; done
w() { printf "%b" "$1" > $LIVE; echo "$(date +%T) live: $(echo "$1" | tr "\n" "|")"; }
w "set net 1\nset netloop 1\n"; sleep 10
w "set net 1\nset netloop 1\nnet diag\n"; sleep 2
w "set net 1\nset netloop 1\ninput 0:A:4,25:A:4,50:A:4,75:A:4,160:A:4,185:A:4,210:A:4\n"; sleep 8
w "set net 1\nset netloop 1\nname mp2\nnet diag\n"; sleep 2
w "set net 0\nnet diag\n"; sleep 3
w "set net 0\nname mp3\nnet diag\n"; sleep 2
kill $P; sleep 3; ps -p $P >/dev/null && echo "STILL RUNNING $P"
cp $L $O.log
cp -a $LIVE.mp_bak $LIVE; cp -a $NT.mp_bak $NT; rm -f $ENVF $LIVE.mp_bak $NT.mp_bak
grep -n "\[net\]\|live test\|socket" $O.log | head -40
echo done
