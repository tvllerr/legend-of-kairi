#!/bin/sh
# netsave_test.sh: tools/mp/netsave.sh against two local fake save folders (PC + "Deck"), with shims for ssh (runs the
# command locally), ps / tasklist (fake "game running") and tar (fake failure mid-extract). Run from anywhere:
#   sh tools/mp/test/netsave_test.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
NS="$HERE/../netsave.sh"
REALTAR=$(command -v tar)
W=$(mktemp -d)
pass=0 fail=0
check() { if eval "$2"; then pass=$((pass + 1)); else fail=$((fail + 1)); echo "FAIL: $1"; fi; }

mkdir -p "$W/bin"
cat > "$W/bin/ssh" <<'EOF'
#!/bin/sh
while [ $# -gt 0 ]; do case $1 in -o) shift 2 ;; deck) shift; break ;; *) shift ;; esac; done
[ -n "${FAKE_SSH_FAIL:-}" ] && exit 255
exec sh -c "$*"
EOF
cat > "$W/bin/ps" <<'EOF'
#!/bin/sh
[ -n "${FAKE_PS_FAIL:-}" ] && exit 2
echo "/usr/bin/bash"
[ -n "${FAKE_DECK_RUN:-}" ] && echo "/home/deck/D/linux_build/src/build/linux/x86_64/game/savagewind"
exit 0
EOF
cat > "$W/bin/tasklist" <<'EOF'
#!/bin/sh
[ -n "${FAKE_TL_FAIL:-}" ] && exit 1
echo "Image Name                     PID"
[ -n "${FAKE_PC_RUN:-}" ] && echo "savagewind.exe                9708"
exit 0
EOF
cat > "$W/bin/tar" <<EOF
#!/bin/sh
case "\$1" in x*) if [ -n "\${FAKE_TAR_FAIL:-}" ]; then cat > /dev/null; echo "tar: fake failure" >&2; exit 2; fi ;; esac  # (stream lost: nothing extracted)
exec "$REALTAR" "\$@"
EOF
chmod +x "$W/bin/"*

# fresh sides: PC slots 0..2 "pc<n>", Deck slots 0..1 "deck<n>"
reset() {
    rm -rf "$W/pc" "$W/dk"
    mkdir -p "$W/pc/save/save_0/tracker" "$W/dk/save/save_0/tracker"
    for s in 0 1 2; do mkdir -p "$W/pc/save/save_0/$s"; echo "pc$s" > "$W/pc/save/save_0/$s/game_data.sav"; done
    for s in 0 1; do mkdir -p "$W/dk/save/save_0/$s"; echo "deck$s" > "$W/dk/save/save_0/$s/game_data.sav"; done
    echo pct > "$W/pc/save/save_0/tracker/t.sav"; echo dkt > "$W/dk/save/save_0/tracker/t.sav"
    echo pcopt > "$W/pc/save/save_0/option.sav"; echo dkopt > "$W/dk/save/save_0/option.sav"
}
run() { env PATH="$W/bin:$PATH" NETSAVE_PCSAVE="$W/pc/save" NETSAVE_DKDATA="$W/dk" "$@" sh "$NS" ${ARGS:-} > "$W/out" 2>&1; echo $?; }
deck_is() { [ "$(cat "$W/dk/save/save_0/0/game_data.sav")" = "$1" ]; }
pc_is() { [ "$(cat "$W/pc/save/save_0/0/game_data.sav")" = "$1" ]; }

# 1 normal PC -> Deck: copied, md5 OK, option.sav kept, backup tar, no staging left
reset; ARGS=; rc=$(run); cp "$W/out" "$W/out1"
check "normal rc 0 ($rc)" '[ "$rc" = 0 ]'
check "normal copied" 'deck_is pc0 && [ -f "$W/dk/save/save_0/2/game_data.sav" ]'
check "normal md5 OK" 'grep -q "md5 OK" "$W/out"'
check "normal option.sav kept" '[ "$(cat "$W/dk/save/save_0/option.sav")" = dkopt ]'
check "normal backup tar" 'ls "$W/dk"/save.pre-netsave-*.tar >/dev/null 2>&1'
check "normal no staging left" '! ls -d "$W/dk/save/"save_0.netsave* >/dev/null 2>&1'

# 2 game running on the TARGET (Deck) -> refused, Deck untouched
reset; ARGS=; rc=$(run FAKE_DECK_RUN=1)
check "target running refused ($rc)" '[ "$rc" = 1 ] && deck_is deck0'
# 3 game running on the SOURCE (PC) -> refused too (a save mid-write)
reset; ARGS=; rc=$(run FAKE_PC_RUN=1)
check "source running refused ($rc)" '[ "$rc" = 1 ] && deck_is deck0'
# 4 the running check itself fails (Deck ps) -> refused, not "not running"
reset; ARGS=; rc=$(run FAKE_PS_FAIL=1)
check "deck check failed refused ($rc)" '[ "$rc" = 1 ] && deck_is deck0'
reset; ARGS=; rc=$(run FAKE_TL_FAIL=1)
check "pc check failed refused ($rc)" '[ "$rc" = 1 ] && deck_is deck0'
# 5 copy fails mid-extract -> target untouched (all old slots there), restore hint printed, nonzero
reset; ARGS=; rc=$(run FAKE_TAR_FAIL=1)
check "tar fail nonzero ($rc)" '[ "$rc" != 0 ]'
check "tar fail target untouched" 'deck_is deck0 && [ "$(cat "$W/dk/save/save_0/1/game_data.sav")" = deck1 ] && [ ! -e "$W/dk/save/save_0/2" ]'
check "tar fail no staging left" '! ls -d "$W/dk/save/"save_0.netsave* >/dev/null 2>&1'
# 6 --back Deck -> PC normal + refused while the PC runs
reset; ARGS=--back; rc=$(run)
check "back rc 0 ($rc)" '[ "$rc" = 0 ] && pc_is deck0 && [ ! -e "$W/pc/save/save_0/2" ] && [ "$(cat "$W/pc/save/save_0/option.sav")" = pcopt ]'
reset; ARGS=--back; rc=$(run FAKE_PC_RUN=1)
check "back target running refused ($rc)" '[ "$rc" = 1 ] && pc_is pc0'
reset; ARGS=--back; rc=$(run FAKE_TAR_FAIL=1)
check "back tar fail untouched ($rc)" '[ "$rc" != 0 ] && pc_is pc0 && [ -e "$W/pc/save/save_0/2" ]'
# 7 --dry / --check change nothing
reset; ARGS=--dry; rc=$(run)
check "dry unchanged ($rc)" '[ "$rc" = 0 ] && deck_is deck0'
reset; ARGS=--check; rc=$(run)
check "check mismatch reported ($rc)" '[ "$rc" = 1 ] && deck_is deck0 && grep -q MISMATCH "$W/out"'

[ -n "${KEEP:-}" ] && echo "kept $W" || rm -rf "$W"
echo "netsave_test: $pass/$((pass + fail)) passed, $fail fails"
[ $fail = 0 ]
