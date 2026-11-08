#!/bin/bash
# Native Linux Savage Wind (no Proton). Started by deck_wrap.sh when play/deck_native.txt exists.
# deck_native.txt line 1 picks the build:
#   appimage (default) = official 0.4.1-alpha.4 AppImage baseline (data root linux_test/data)
#   sora               = our fork (deck-linux, linux_build/.../game/savagewind) + Sora mod (sora_mod.so)
#   fork               = our fork exe with Sora OFF (A/B: exe changes vs the mod)
#   upstream           = upstream tag v0.4.1-alpha.4 built in the same container/deps/make line as the fork
#                        (linux_up/src, session 7 A/B: build env vs our code); data root linux_up/data
#   bis                = session 8 Deck-ONLY patched copy linux_bis/src (fork, Sora off; BIS_* env knobs), data root linux_sora/data
#   fork/sora data root = linux_sora/data (extracted/ = hard links of linux_test/data/extracted, its own cache + ini)
# deck_native.txt line 2 "ab" = A/B run: fresh copy of /home/deck/D/ab_save, script clock, scripted Continue.
# deck_native_env.sh (optional, sourced last) = extra env for a test.
T=/home/deck/D/linux_test
S=/home/deck/D/linux_sora
G=/home/deck/D/linux_build/src/build/linux/x86_64
U=/home/deck/D/linux_up
R=/home/deck/D/SoraBotW
mode=$(sed -n 1p $R/play/deck_native.txt 2>/dev/null | tr -d '\r ')
ab=$(sed -n 2p $R/play/deck_native.txt 2>/dev/null | tr -d '\r ')
mode=${mode:-appimage}
case "$mode" in
  sora|fork|bis) LOGDIR=$S/logs; export SAVAGEWIND_DATA_ROOT=$S/data ;;
  upstream)  LOGDIR=$U/logs; export SAVAGEWIND_DATA_ROOT=$U/data ;;
  *)         LOGDIR=$T/logs; export SAVAGEWIND_DATA_ROOT=$T/data ;;
esac
LOG=$LOGDIR/native_$mode${ab:+_$ab}.log
if [ "$ab" = ab ]; then
  rm -rf "$LOGDIR/ab_save_run"
  cp -a /home/deck/D/ab_save "$LOGDIR/ab_save_run"
  export SAVAGEWIND_SAVE_DIR=$LOGDIR/ab_save_run
  export SAVAGEWIND_SCRIPT_CLOCK=1
  export SAVAGEWIND_INPUT=900:A:8,1400:A:8,1900:A:8
fi
if [ "$mode" = sora ]; then
  # = "D:/SoraBotW/Play Sora in BotW 4.1.bat" with Linux paths
  export SORA_ROOT=$R
  export SAVAGEWIND_SORA=1
  export SAVAGEWIND_SORA_DIR=$G/game
  export SAVAGEWIND_SORA_LIVE=$R/play/live_test.txt
  export SAVAGEWIND_LOGO_TEX=$R/data/logo/Logo3DGameTitle_00.bin
  export SAVAGEWIND_LOGO2D_TEX=$R/data/logo/Logo2D_00.bin
  export SAVAGEWIND_ROMFS_OVERLAY=$R/data/overlay
  # (mp slice 1) play/deck_net.txt present = multiplayer on (UDP 7790), its first line = the name (default Deck)
  if [ -f $R/play/deck_net.txt ]; then
    export SAVAGEWIND_SORA_NET=1
    export SAVAGEWIND_SORA_NET_NAME="$(head -1 $R/play/deck_net.txt | tr -cd 'A-Za-z0-9 _-' | cut -c1-15)"
    [ -n "$SAVAGEWIND_SORA_NET_NAME" ] || export SAVAGEWIND_SORA_NET_NAME=Deck
  fi
fi
[ -f $T/deck_native_env.sh ] && . $T/deck_native_env.sh
[ -f "$LOG" ] && mv -f "$LOG" "$LOG.prev"
echo "[deck_native] $(date) mode=$mode ab=${ab:-0} env: $(env | grep -E '^(SAVAGEWIND_|SORA_)' | tr '\n' ' ')" > "$LOG"
case "$mode" in
  sora|fork)
    export LD_LIBRARY_PATH=$G/deps/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
    cd "$SAVAGEWIND_DATA_ROOT" || exit 1
    exec "$G/game/savagewind" "$@" >> "$LOG" 2>&1 ;;
  bis)
    B=/home/deck/D/linux_bis/src/build/linux/x86_64
    export LD_LIBRARY_PATH=$B/deps/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
    cd "$SAVAGEWIND_DATA_ROOT" || exit 1
    exec "$B/game/savagewind" "$@" >> "$LOG" 2>&1 ;;
  upstream)
    export LD_LIBRARY_PATH=$U/src/build/linux/x86_64/deps/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
    cd "$SAVAGEWIND_DATA_ROOT" || exit 1
    exec "$U/src/build/linux/x86_64/game/savagewind" "$@" >> "$LOG" 2>&1 ;;
  *) exec "$T/Savage_Wind-0.4.1-alpha.4-x86_64.AppImage" "$@" >> "$LOG" 2>&1 ;;
esac
