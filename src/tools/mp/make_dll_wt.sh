#!/bin/bash
# (mp4) the DLL half of `make sora` for THIS worktree (wt152-merge): never touches sw-a41; output build_mp4/sora_mod.dll.
# The import lib is the main tree's build/savagewind.dll.a, read only. Extra clang flags: $@ (e.g. -D__linux__ is NOT for this build).
export PATH=/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH
export TMP=$HOME/AppData/Local/Temp TEMP=$HOME/AppData/Local/Temp
cd D:/SoraBotW/src/wt152-merge || exit 1
mkdir -p build_mp4/rt
clang -O2 -g -fno-strict-aliasing -Wall -Wno-unused-function -Iruntime -Iruntime/nvn -isystem third_party -isystem third_party/stb -D_FILE_OFFSET_BITS=64 -D_POSIX_THREAD_SAFE_FUNCTIONS -D_CRT_RAND_S -DWIN32_LEAN_AND_MEAN -DNOMINMAX "$@" -c runtime/sora.c -o build_mp4/rt/sora_mod.o &&
clang -shared -fuse-ld=lld -o build_mp4/sora_mod.dll.tmp.dll build_mp4/rt/sora_mod.o build_mp4/savagewind.dll.a -lws2_32 &&
mv -f build_mp4/sora_mod.dll.tmp.dll build_mp4/sora_mod.dll && echo "sora_mod.dll rebuilt (mp4, wt152-merge/build_mp4)"
