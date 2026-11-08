#!/bin/sh
# bots.sh: 1-3 net bots against the local game (127.0.0.1:7790), ports 7801..7803, names Bot1..Bot3, offsets 3 m apart along x
# (BotW unit u = 0.01 * g_k per KH cm, g_k = 1 by default, so 3 u = 3 m).
#   sh bots.sh start [N=3] [file]   replays <file> (default $BOTDIR/walk.nbr) with N bots, looping
#   sh bots.sh stop                 stop file: every bot sends BYE and exits
#   sh bots.sh record [secs=60]     records the game's own SNAPs to $BOTDIR/walk.nbr: in the game `net peer 127.0.0.1:7800`, walk about
# Env: NETBOT (exe, default <tree>/build_mp4/netbot.exe), BOTDIR (default D:/SoraBotW/mpbots), GAME (default 127.0.0.1:7790),
#      BOT_PEERS (extra targets, space separated, e.g. "127.0.0.1:7791" = COPY2: every bot talks to it too)
# Slots: a game copy holds 3 remote players. Main + COPY2 + 3 bots = 5 players = one is refused: with COPY2 use N <= 2.
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
NETBOT=${NETBOT:-$ROOT/build_mp4/netbot.exe}
BOTDIR=${BOTDIR:-D:/SoraBotW/mpbots}
GAME=${GAME:-127.0.0.1:7790}
BOT_PEERS=${BOT_PEERS:-}
PEERS="--peer $GAME"
for p in $BOT_PEERS; do PEERS="$PEERS --peer $p"; done
mkdir -p "$BOTDIR"
case "${1:-}" in
start)
    N=${2:-3}
    F=${3:-$BOTDIR/walk.nbr}
    [ "$N" -ge 1 ] && [ "$N" -le 3 ] || { echo "N = 1..3"; exit 2; }
    [ -f "$F" ] || { echo "no recording $F (sh bots.sh record, or pass a file)"; exit 1; }
    [ "$N" -ge 3 ] && echo "note: if COPY2 runs too: main + COPY2 + $N bots = $((N + 2)) players, a game copy holds 3 remote players, so one is refused (N <= 2 with COPY2)"
    rm -f "$BOTDIR/stop"
    [ -f "$NETBOT" ] || sh "$HERE/make_netbot.sh" || exit 1
    i=1
    while [ "$i" -le "$N" ]; do
        "$NETBOT" play "$F" --port $((7800 + i)) --name "Bot$i" $PEERS --offset "$((3 * i)),0" --loop --stopfile "$BOTDIR/stop" >"$BOTDIR/bot$i.log" 2>&1 &
        echo "Bot$i: port $((7800 + i)) offset $((3 * i)) m, log $BOTDIR/bot$i.log"
        i=$((i + 1))
    done
    ;;
stop)
    : > "$BOTDIR/stop"   # the bots poll this file, send BYE and exit
    sleep 2
    rm -f "$BOTDIR/stop"
    echo "stopped (bots said BYE)"
    ;;
record)
    S=${2:-60}
    "$NETBOT" record 7800 "$BOTDIR/walk.nbr" --name Rec --secs "$S" --stopfile "$BOTDIR/stop"
    ;;
*)
    sed -n 2,8p "$0"
    exit 2
    ;;
esac
