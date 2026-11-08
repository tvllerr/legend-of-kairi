#!/usr/bin/env python3
"""Resolve the 'exe+<rva>' names of the '[esa] prof' log lines (sora_esa_samp.h) to savagewind.exe symbols.
usage: python resolve_samples.py last41.log [savagewind.exe]   (needs nm from MSYS2 ucrt64 on PATH)
swgame code is already logged as f_<guest addr>+<host offset> (gamelib tables); other modules as name+0xrva."""
import re, subprocess, sys, bisect
log = sys.argv[1]
exe = sys.argv[2] if len(sys.argv) > 2 else r"D:/SoraBotW/src/sw-a41/build/savagewind.exe"
out = subprocess.run(["nm", "-n", exe], capture_output=True, text=True).stdout
syms = []
for l in out.splitlines():
    p = l.split()
    if len(p) == 3 and p[1] in "TtWwDdBb":
        syms.append((int(p[0], 16), p[2]))
addrs = [a for a, _ in syms]
base = 0x140000000  # PE image base of the link (nm addresses are base + rva)
def res(m):
    a = base + int(m.group(1), 16)
    i = bisect.bisect_right(addrs, a) - 1
    return f"exe+{m.group(1)} ({syms[i][1]}+{a - syms[i][0]:#x})" if i >= 0 else m.group(0)
for l in open(log, errors="replace"):
    if "[esa] prof" in l:
        print(re.sub(r"exe\+([0-9a-f]+)", res, l.rstrip()))
