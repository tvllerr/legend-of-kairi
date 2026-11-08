#!/bin/sh
# copy2.sh: builds the 2nd game folder D:/SoraBotW/play2/game for a second Savage Wind copy on this PC (multiplayer test).
#   sh copy2.sh [--dry] [--redo] [--cache copy|empty]
# Makes (nothing under the real game folder is written; it is only read):
#   play2/game/<small top-level files>   copied (dlls, fonts, ini, icon ...; NOT savagewind.exe: the launcher uses the sw-a41 build)
#   play2/game/extracted, luts, colour-luts   Windows junctions to the real ones (read only use)
#   play2/game/gamelib-src               copied (tiny; only used when swgame.dll has to be rebuilt)
#   play2/game/save/                     OWN copy of the main save (the main save is backed up first to play2/save_backup_<ts>)
#   play2/game/cache/                    OWN copy (--cache copy, default: ~560 MB so the first start does not re-precompile
#                                        shaders) or empty (--cache empty: the game rebuilds it, minutes)
#   play2/game/savagewind.ini            copy with window_size 1280x720 windowed (two 2560x1440 windows are too much)
#   play2/logs/  play2/live_test.txt     own log dir + live file
# Refuses when play2/game exists unless --redo (which deletes ONLY play2/game: junction targets are not followed;
# COPY2's own save is copied to play2/save_backup_redo_<ts> first).
# Then start the main game, then "Play_COPY2.bat" (install it as D:/SoraBotW/Play Sora in BotW 4.1 COPY2.bat).
set -eu
DRY=0 REDO=0 CACHE=copy
while [ $# -gt 0 ]; do
    case $1 in
    --dry) DRY=1 ;;
    --redo) REDO=1 ;;
    --cache) shift; CACHE=$1 ;;
    *) echo "usage: sh copy2.sh [--dry] [--redo] [--cache copy|empty]"; exit 2 ;;
    esac
    shift
done
SRC=${SRC:-"D:/recompiled games/SavageWind-0.4.1-alpha.4-windows/Savage Wind"}
P2=${P2:-D:/SoraBotW/play2}
DST=$P2/game
TS=$(date +%Y%m%d_%H%M%S)

run() { if [ "$DRY" = 1 ]; then echo "[dry] $*"; else "$@"; fi; }
say() { echo "copy2: $*"; }
winpath() { cygpath -w "$1"; }

[ -d "$SRC/extracted" ] || { echo "copy2: no game folder at $SRC"; exit 1; }
if [ -e "$DST" ]; then
    if [ "$REDO" = 1 ]; then
        # COPY2's own save is backed up first (play2/save_backup_redo_<ts>): --redo must never lose progress made in COPY2
        if [ -d "$DST/save" ]; then
            say "--redo: backing up COPY2's own save -> $P2/save_backup_redo_$TS"
            run mkdir -p "$P2/save_backup_redo_$TS"
            run cp -rp "$DST/save/." "$P2/save_backup_redo_$TS/"
        fi
        say "--redo: removing $DST (junctions are removed, their targets are not touched)"
        if [ "$DRY" = 1 ]; then echo "[dry] rmdir junctions, rm -rf $DST"; else
            for j in extracted luts colour-luts; do
                if [ -e "$DST/$j" ] || [ -L "$DST/$j" ]; then cmd //c rmdir "$(winpath "$DST/$j")" >/dev/null; fi
            done
            rm -rf "$DST"
        fi
    else
        echo "copy2: $DST exists - refusing (use --redo to rebuild it)"
        exit 1
    fi
fi

say "making $DST"
run mkdir -p "$DST" "$P2/logs"
[ -e "$P2/live_test.txt" ] || { if [ "$DRY" = 1 ]; then echo "[dry] create $P2/live_test.txt"; else printf 'name copy2\n' > "$P2/live_test.txt"; fi; }

# small top-level files
for f in "$SRC"/*; do
    [ -f "$f" ] || continue
    b=$(basename "$f")
    case $b in savagewind.exe | savagewind.log | save.lock | savagewind.dll.a) say "skip $b"; continue ;; esac
    run cp -p "$f" "$DST/$b"
done
run cp -rp "$SRC/gamelib-src" "$DST/gamelib-src"

# window size of the copy
if [ "$DRY" = 1 ]; then echo "[dry] sed window_size -> 1280x720, fullscreen 0 in $DST/savagewind.ini"; else
    sed -e 's/^window_size = .*/window_size = 1280x720/' -e 's/^fullscreen = .*/fullscreen = 0/' "$SRC/savagewind.ini" > "$DST/savagewind.ini"
fi

# junctions
for j in extracted luts colour-luts; do
    if [ "$DRY" = 1 ]; then echo "[dry] mklink /J $DST/$j -> $SRC/$j"; else
        cmd //c mklink //J "$(winpath "$DST/$j")" "$(winpath "$SRC/$j")" >/dev/null || { echo "copy2: mklink $j failed"; exit 1; }
    fi
done

# save: backup of the main one first, then the copy
say "main save backup -> $P2/save_backup_$TS (the main game may be running: a save written right now can be caught half-way; copy again with --redo later)"
run mkdir -p "$P2/save_backup_$TS"
run cp -rp "$SRC/save/." "$P2/save_backup_$TS/"
run mkdir -p "$DST/save"
run cp -rp "$SRC/save/." "$DST/save/"
[ "$DRY" = 1 ] || rm -f "$DST/save/save.lock"

# cache
if [ "$CACHE" = copy ]; then
    say "cache: copying (about 560 MB)"
    run cp -rp "$SRC/cache" "$DST/cache"
else
    say "cache: empty (first start precompiles shaders, takes minutes)"
    run mkdir -p "$DST/cache"
fi

say "done. Next: start the main game, then run Play_COPY2.bat (live file $P2/live_test.txt, log $P2/logs/last41.log)"
