#!/bin/bash
# mptest7.sh <name>: multiplayer Task 7 (name plate) on the ab save as BASE SORA (SORA_CHAR=2), loopback:
# idle (plate "Deck" over the copy + KH2 HUD), walk (plate follows), camera turned (copy in view),
# net 0 (plate gone) / net 1 (back), then the KH2 pause menu (plate gone, menu drawn; left open: B does not close it in scripts). gamescopectl shots at each step.
# Restores live_test.txt + deck_native.txt. Output /home/deck/D/linux_sora/logs/mp_<name>.{log,*.png}
set -u
name=$1
R=/home/deck/D/SoraBotW; LIVE=$R/play/live_test.txt; NT=$R/play/deck_native.txt; ENVF=/home/deck/D/linux_test/deck_native_env.sh
L=/home/deck/D/linux_sora/data/savagewind.log; O=/home/deck/D/linux_sora/logs/mp_$name
cp -a $LIVE $LIVE.mp_bak; cp -a $NT $NT.mp_bak
printf "sora\nab\n" > $NT
printf "export SAVAGEWIND_SORA_NET_NAME=Deck\nexport SORA_CHAR=2\n" > $ENVF
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
shot() { rm -f /tmp/sw_shot.png; gamescopectl screenshot /tmp/sw_shot.png >/dev/null 2>&1; sleep 2; [ -f /tmp/sw_shot.png ] && cp /tmp/sw_shot.png $O.$1.png && echo "$(date +%T) shot $1" || echo "no shot $1"; }
B="set net 1\nset netloop 1\n"
w "$B"; sleep 6
w "${B}name a1\nnet diag\n"; sleep 2; shot idle
w "${B}name a2\ninput 0:STICK_UP:150\n"; sleep 1.5; shot walk; sleep 3
w "${B}name a3\ninput 0:RSTICK_RIGHT:70\n"; sleep 2; shot away
w "${B}name a4\ninput 0:RSTICK_LEFT:70\n"; sleep 3; shot facing
w "${B}name a3c\ninput 0:RSTICK_RIGHT:70\n"; sleep 3; shot away2
w "set net 0\nset netloop 1\nnet diag\n"; sleep 3; shot net_off
w "${B}name a7\nnet diag\n"; sleep 4; shot net_on
w "${B}name a5\ninput 0:PLUS:4\n"; sleep 3; shot menu
kill $P; sleep 3; ps -p $P >/dev/null && echo "STILL RUNNING $P"
cp $L $O.log
cp -a $LIVE.mp_bak $LIVE; cp -a $NT.mp_bak $NT; rm -f $ENVF $LIVE.mp_bak $NT.mp_bak
grep -n "\[net\]\|label\|camp:" $O.log | grep -v "unknown line" | head -60
echo done
