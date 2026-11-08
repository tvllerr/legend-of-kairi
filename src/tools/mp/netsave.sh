#!/bin/sh
# netsave.sh: copy the BotW saves between the PC (Savage Wind 4.1 folder) and the Deck (native linux_sora) for
# multiplayer slice 1 (plan docs/plans/2026-11-05-multiplayer-slice1.md Task 8), so both Continue into the same world.
#   sh tools/mp/netsave.sh          PC -> Deck        (canonical copy: this one, in git; D:/SoraBotW/tools/netsave.sh = a copy)
#   sh tools/mp/netsave.sh --back   Deck -> PC
#   --dry                           print what it would copy, change nothing
#   --check                         only compare the two sides (md5), change nothing
# Mirrors save_0's slot folders 0..5 + tracker/ (those that exist on the source; option.sav = each side's own settings,
# not copied), so the newest slot (printed) is the newest on both sides. Safety (final review I2):
# - refuses while savagewind runs on EITHER side, or when that can't be checked (ssh / tasklist / ps failing);
# - the target's whole save folder is tarred first (<data>/save.pre-netsave-<date>.tar);
# - the copy goes into save_0.netsave-new next to save_0 and is md5-checked against the source BEFORE anything in
#   save_0 changes; a failed / partial copy leaves the target untouched; only then the old items move out and the new in.
# Tests: sh tools/mp/test/netsave_test.sh (local fake sides, shims for ssh / ps / tasklist / tar).
set -eu
PCSAVE=${NETSAVE_PCSAVE:-"D:/recompiled games/SavageWind-0.4.1-alpha.4-windows/Savage Wind/save"}  # (env: tests)
DKDATA=${NETSAVE_DKDATA:-/home/deck/D/linux_sora/data}
ITEMS="0 1 2 3 4 5 tracker"
back=0 dry=0 check=0
for a in "$@"; do
    case $a in
    --back) back=1 ;;
    --dry) dry=1 ;;
    --check) check=1 ;;
    *) echo "usage: netsave.sh [--back] [--dry] [--check]"; exit 2 ;;
    esac
done
stamp=$(date +%Y%m%d-%H%M%S)
DKSAVE=$DKDATA/save

# "run" / "idle"; anything else (the check failed) = "unknown"
pc_state() { out=$(tasklist //fi "imagename eq savagewind.exe" 2>/dev/null) || { echo unknown; return 0; }
    if echo "$out" | grep -qi savagewind.exe; then echo run; else echo idle; fi; }
deck_state() { ssh deck 'out=$(ps -eo args) || { echo unknown; exit 0; }
    if echo "$out" | grep -Eq "[g]ame/savagewind|[u]sr/bin/savagewind"; then echo run; else echo idle; fi' 2>/dev/null || echo unknown; }
# the ITEMS present in a save_0 folder (local / Deck)
pc_items() { (cd "$1" && for i in $ITEMS; do [ -e "$i" ] && printf '%s ' "$i"; done; true); }
deck_items() { ssh deck "cd $1 && for i in $ITEMS; do [ -e \$i ] && printf '%s ' \$i; done; true"; }
# md5 list of <items> under a folder (both sides print "<md5>  <path>")
pc_md5() { [ -n "$2" ] || return 0; (cd "$1" && find $2 -type f -print0 | sort -z | xargs -0 md5sum) | sed 's/ \*/  /'; }
deck_md5() { [ -n "$2" ] || return 0; ssh deck "cd $1 && find $2 -type f -print0 | sort -z | xargs -0 md5sum" | sed 's/ \*/  /'; }
# newest slot by game_data.sav mtime: "<slot> <date>"
pc_newest() { (cd "$PCSAVE/save_0" && ls -t --time-style=+%F_%T -l */game_data.sav 2>/dev/null | head -1 | awk '{print substr($NF,1,1), $(NF-1)}'); }
deck_newest() { ssh deck "cd $DKSAVE/save_0 && ls -t --time-style=+%F_%T -l */game_data.sav 2>/dev/null | head -1 | awk '{print substr(\$NF,1,1), \$(NF-1)}'"; }

ssh -o BatchMode=yes -o ConnectTimeout=5 deck true || { echo "netsave: no ssh to the Deck"; exit 1; }
[ -d "$PCSAVE/save_0" ] || { echo "netsave: no PC save at $PCSAVE/save_0"; exit 1; }
ssh deck "test -d $DKSAVE/save_0" || { echo "netsave: no Deck save at $DKSAVE/save_0"; exit 1; }
echo "PC   newest slot: $(pc_newest)"
echo "Deck newest slot: $(deck_newest)"

compare() {
    a=$(pc_md5 "$PCSAVE/save_0" "$(pc_items "$PCSAVE/save_0")") b=$(deck_md5 "$DKSAVE/save_0" "$(deck_items "$DKSAVE/save_0")")
    if [ "$a" = "$b" ]; then echo "md5 OK: $(echo "$a" | grep -c .) files identical"; return 0; fi
    echo "md5 MISMATCH"; t=${TMPDIR:-/tmp}/netsave.$$
    echo "$a" > "$t.pc"; echo "$b" > "$t.deck"; diff "$t.pc" "$t.deck" || true; rm -f "$t.pc" "$t.deck"; return 1
}
if [ $check = 1 ]; then compare; exit $?; fi

if [ $back = 0 ]; then
    echo "PC -> Deck ($DKSAVE/save_0: $ITEMS)"
    src=$(pc_items "$PCSAVE/save_0")
    if [ $dry = 1 ]; then (cd "$PCSAVE/save_0" && find $src -type f -printf "%10s %TF %TH:%TM  %p\n" | sort -k4); echo "(dry run: nothing copied)"; exit 0; fi
else
    echo "Deck -> PC ($PCSAVE/save_0: $ITEMS)"
    src=$(deck_items "$DKSAVE/save_0")
    if [ $dry = 1 ]; then ssh deck "cd $DKSAVE/save_0 && find $src -type f -printf '%10s %TF %TH:%TM  %p\n' | sort -k4"; echo "(dry run: nothing copied)"; exit 0; fi
fi
[ -n "$src" ] || { echo "netsave: no slot folders on the source side, nothing copied"; exit 1; }
ps_=$(pc_state) ds_=$(deck_state)
[ "$ps_" = idle ] || { echo "netsave: savagewind on the PC is $ps_ (quit it first; unknown = tasklist failed)"; exit 1; }
[ "$ds_" = idle ] || { echo "netsave: savagewind on the Deck is $ds_ (quit it first; unknown = ssh / ps failed)"; exit 1; }

if [ $back = 0 ]; then
    # target = the Deck
    ssh deck "cd $DKDATA && tar cf save.pre-netsave-$stamp.tar save" && echo "backup $DKDATA/save.pre-netsave-$stamp.tar"
    ssh deck "rm -rf $DKSAVE/save_0.netsave-new $DKSAVE/save_0.netsave-old && mkdir $DKSAVE/save_0.netsave-new"
    (cd "$PCSAVE/save_0" && tar cf - $src) | ssh deck "tar xf - -C $DKSAVE/save_0.netsave-new" || true
    if [ "$(pc_md5 "$PCSAVE/save_0" "$src")" != "$(deck_md5 "$DKSAVE/save_0.netsave-new" "$src")" ]; then
        ssh deck "rm -rf $DKSAVE/save_0.netsave-new" || true
        echo "netsave: copy FAILED (staging md5 differs); the Deck's save is unchanged"; exit 1
    fi
    ssh deck "cd $DKSAVE && mkdir save_0.netsave-old && for i in $ITEMS; do [ -e save_0/\$i ] && mv save_0/\$i save_0.netsave-old/; done; mv save_0.netsave-new/* save_0/ && rm -rf save_0.netsave-new save_0.netsave-old" || {
        echo "netsave: swap FAILED; restore the Deck with: ssh deck \"cd $DKDATA && rm -rf save && tar xf save.pre-netsave-$stamp.tar\""; exit 1; }
else
    # target = the PC
    (cd "$PCSAVE/.." && tar cf "save.pre-netsave-$stamp.tar" save) && echo "backup $PCSAVE.pre-netsave-$stamp.tar"
    rm -rf "$PCSAVE/save_0.netsave-new" "$PCSAVE/save_0.netsave-old"; mkdir "$PCSAVE/save_0.netsave-new"
    ssh deck "cd $DKSAVE/save_0 && tar cf - $src" | (cd "$PCSAVE/save_0.netsave-new" && tar xf -) || true
    if [ "$(deck_md5 "$DKSAVE/save_0" "$src")" != "$(pc_md5 "$PCSAVE/save_0.netsave-new" "$src")" ]; then
        rm -rf "$PCSAVE/save_0.netsave-new"
        echo "netsave: copy FAILED (staging md5 differs); the PC's save is unchanged"; exit 1
    fi
    ( cd "$PCSAVE" && mkdir save_0.netsave-old && for i in $ITEMS; do if [ -e "save_0/$i" ]; then mv "save_0/$i" save_0.netsave-old/; fi; done \
      && mv save_0.netsave-new/* save_0/ && rm -rf save_0.netsave-new save_0.netsave-old ) || {
        echo "netsave: swap FAILED; restore the PC with: cd \"$PCSAVE/..\" && rm -rf save && tar xf save.pre-netsave-$stamp.tar"; exit 1; }
fi
compare
echo "PC   newest slot: $(pc_newest)"
echo "Deck newest slot: $(deck_newest)"
