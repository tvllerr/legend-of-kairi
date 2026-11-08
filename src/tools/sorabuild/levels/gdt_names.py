"""gamedata_flag_names.txt from the player's own BotW romfs (s154; the s32 list had no script).

  python -E -P gdt_names.py <romfs> <out.txt>
Pack/Bootup.pack -> GameData/gamedata.ssarc (Yaz0 SARC, 27 *.bgdata BYMLs) -> every run of [A-Za-z0-9_.-]{3,} in
each bgdata's bytes, unique, sorted, CRLF-joined, no final newline. That rule reproduces the s32 list byte for byte
(46,331 names: the 45,027 DataName flags + ~1.3k short byte-run junk strings, harmless: the DLL only looks up "_TBox_"
names, runtime/sora_chestabil.h). Needs oead (pip install oead).
"""
import re
import sys

import oead

romfs, out = sys.argv[1], sys.argv[2]
boot = oead.Sarc(open(romfs + "/Pack/Bootup.pack", "rb").read())
gd = oead.Sarc(oead.yaz0.decompress(boot.get_file("GameData/gamedata.ssarc").data))
names = set()
for f in gd.get_files():
    names |= {m.decode() for m in re.findall(rb"[A-Za-z0-9_.\-]{3,}", bytes(f.data))}
with open(out, "wb") as f:
    f.write("\r\n".join(sorted(names)).encode())
print("gamedata_flag_names: %d names" % len(names))
