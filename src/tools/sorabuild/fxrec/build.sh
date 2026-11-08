#!/bin/sh
# build the fxrec tools (MSYS2 UCRT64 clang). Our exe must NOT sit at 0x140000000 (KH2 is mapped there):
# fixed low base, no ASLR. Big stack for KH2 code.
set -e
export PATH=/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH
cd "$(dirname "$0")"
mkdir -p build
clang -O1 -g -Wall -Wno-unused-function -o build/fxrec_boot.exe fxrec_boot.c fxrec_s2.c fxrec_ldr.c \
  -Wl,--image-base,0x10000000 -Wl,--disable-dynamicbase -Wl,--disable-high-entropy-va -Wl,--stack,0x4000000 -lbcrypt
echo built build/fxrec_boot.exe
