#!/bin/bash
# mptest6b.sh <name>: multiplayer Task 6 Steps 2-4 on the ab save as BASE SORA (SORA_CHAR=2):
# loopback idle + combo, Valor (set form 1) + revert, net 0/1, then a high fall -> Game Over -> Continue (= a load
# screen) -> avatar back. Screenshots via gamescopectl at each step. Restores live_test.txt + deck_native.txt.
# Output /home/deck/D/linux_sora/logs/mp_<name>.{log,*.png}
set -u
name=$1
GX=${GX:--850}; GY=${GY:-420}; GZ=${GZ:-1900}
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
w "${B}input 0:A:4,25:A:4,50:A:4\n"; sleep 0.6; shot combo; sleep 6
w "${B}name a2\nnet diag\n"; sleep 2
w "${B}set form 1\n"; sleep 1.5; shot valor_early; sleep 6; shot valor
w "${B}name a3\nnet diag\n"; sleep 2
w "${B}set form 0\n"; sleep 6; shot base_again
w "set net 0\nset netloop 1\nnet diag\n"; sleep 3
w "${B}name a4\n"; sleep 4
w "${B}name a5\nnet diag\n"; sleep 2; shot back
# Game Over: high fall (god mode is off on the ab save), then Continue = load screen
w "${B}goto $GX $GY $GZ\n"; sleep 4; shot fall
for i in $(seq 40); do grep -q "Game Over screen 0x30 up" $L && break; sleep 1; done
grep -q "Game Over screen 0x30 up" $L && echo "$(date +%T) game over up" || echo "NO GAME OVER"
sleep 3; shot gameover
w "${B}name a6\nnet diag\n"; sleep 2
w "${B}name a7\ninput 0:A:6\n"; sleep 4; shot loading
w "${B}name a7b\ninput 0:A:6\n"; sleep 1
for i in $(seq 60); do grep -q "load over at present" $L && break; sleep 1; done
echo "$(date +%T) $(grep -c "load over at present" $L) load-over lines"
sleep 8; shot after_load
w "${B}name a8\nnet diag\n"; sleep 2
kill $P; sleep 3; ps -p $P >/dev/null && echo "STILL RUNNING $P"
cp $L $O.log
cp -a $LIVE.mp_bak $LIVE; cp -a $NT.mp_bak $NT; rm -f $ENVF $LIVE.mp_bak $NT.mp_bak
grep -n "\[net\]\|\[form\]\|\[roxas\]\|Game Over\|load over\|goto" $O.log | grep -v "unknown line" | head -80
echo done
