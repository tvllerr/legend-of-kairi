#!/bin/sh
# netbot_test.sh: fakesend (player 42) -> netbot record A -> netbot play (offset 3,-2) -> netbot record B; checks counts and
# that only WORLD places moved by the offset. Run: sh tools/mp/test/netbot_test.sh   (builds first)
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../.." && pwd)
sh "$HERE/../make_netbot.sh" >/dev/null || { echo "build failed"; exit 1; }
B="$ROOT/build_mp4"
W=$(mktemp -d)
pass=0 fail=0
check() { if eval "$2"; then pass=$((pass + 1)); else fail=$((fail + 1)); echo "FAIL: $1"; fi; }
PA=17801 PB=17802 PP=17803
"$B/netbot.exe" record $PA "$W/a.nbr" --secs 6 >"$W/ra.log" 2>&1 &
sleep 1
"$B/fakesend.exe" $PA 30
sleep 1
"$B/netbot.exe" record $PA "$W/a.nbr" --secs 1 >/dev/null 2>&1 || true   # (no-op: port busy or done)
wait
"$B/netbot.exe" dump "$W/a.nbr" > "$W/a.txt"
check "recorded 30 snaps" 'grep -q "^total 30 snaps" "$W/a.txt"'
check "recorded 6 world events" '[ "$(grep -c "ev seq .* anc 0 " "$W/a.txt")" = 6 ]'
# play it back into record B
"$B/netbot.exe" record $PB "$W/b.nbr" --secs 5 >"$W/rb.log" 2>&1 &
sleep 1
"$B/netbot.exe" play "$W/a.nbr" --port $PP --name TBot --peer 127.0.0.1:$PB --offset 3,-2 >"$W/p.log" 2>&1
sleep 1
wait
"$B/netbot.exe" dump "$W/b.nbr" > "$W/b.txt"
check "played back 30 snaps" 'grep -q "^total 30 snaps" "$W/b.txt"'
check "own player id (not 42)" '! grep -q "player 42 " "$W/b.txt"'
check "feet x shifted +3 (first snap 13)" 'grep -q "feet 13.00 5.00 18.00" "$W/b.txt"'
check "feet x last 42, z 18" 'grep -q "feet 42.00 5.00 18.00" "$W/b.txt"'
check "world event shifted (4,2,1)" 'grep "anc 0 " "$W/b.txt" | grep -q "pos 4.00 2.00 1.00"'
check "anchored event unshifted" 'grep "ev seq .* anc 1 " "$W/b.txt" | grep -q "pos 1.00 2.00 3.00"'
check "no unshifted world event" '! grep "ev seq .* anc 0 " "$W/b.txt" | grep -q "pos 1.00 2.00 3.00"'
check "world move shifted" 'grep "mv ev .* anc 0 " "$W/b.txt" | grep -q "pos 4.00 2.00 1.00"'
check "anchored move unshifted" 'grep "mv ev .* anc 1 " "$W/b.txt" | grep -q "pos 1.00 2.00 3.00"'
check "party feet + shield shifted" 'grep "pm who" "$W/b.txt" | grep -q "feet 10.00 8.00 7.00 shield 7.00 5.00 4.00"'
check "event count same (12)" '[ "$(grep -c "  ev seq" "$W/b.txt")" = 12 ]'
# event seqs remapped: the world event's seq and its paired move share a new seq
S1=$(grep "ev seq .* anc 0 " "$W/b.txt" | head -1 | awk '{print $3}')
M1=$(grep "mv ev .* anc 0 " "$W/b.txt" | head -1 | awk '{print $3}')
check "event/move seq stay paired ($S1 $M1)" '[ "$S1" = "$M1" ]'
check "event seqs remapped (first is 0, anchored 1)" '[ "$S1" = 0 ]'
check "bye ignored cleanly" 'grep -q "done" "$W/rb.log"'
echo "netbot_test: $pass passed, $fail failed (work dir $W)"
[ "$fail" = 0 ]
