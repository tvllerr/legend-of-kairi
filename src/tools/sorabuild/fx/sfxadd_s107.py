#!/usr/bin/env python3
"""s107 abilfx (from s98 sfxadd.py): append the combo-ability effect elements' own SEs (abil_fx_<N> = sora.N) to an existing sfx.bin; s98 potion text follows. Append KH2's item-use SEs to an EXISTING sfx.bin (SKSF v1) without rebuilding it, so every older sound
index / name row / trigger / vtbl row / hit row stays byte-identical (the live pack carries the s92 formse rows, which
tools/sfxpack.py alone would lose).

Adds (names -> sounds, all VERIFIED in PHH's mined kh2_sounds.kh.json + RE notes, see recon/S98_HELPER_POTION.md):
  item_user    se/se000.win32.scd#100   00effect.bar system element 0 = the item task's user effect (re:0x3F6B30)
  item_potion  se/se000.win32.scd#101   system element 1 = Potion's effect (03system item row +6), at the heal

  python sfxadd.py <in sfx.bin> <out sfx.bin>
Never writes over the input; the swap is a manual step (backup first)."""
import json, os, struct, sys, wave
import sys as _sys, os as _os; _sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__))); import sndsrc  # (s156) sound inputs: recipe tree or PHH cache

CACHE = os.environ["SB_FXCACHE"] if "SB_FXCACHE" in os.environ else os.environ["SB_PHH"]  # fx cache dir (SB_FXCACHE; SB_PHH = old name), passed in by sorabuild (ctx.fxcache)
ADD = [  # (name, json key, wav, fallback volume): sora.N's own SE (kh2_sounds.kh.json effects "sora.N" +0x18), only those that have one
    ("abil_fx_32", "se/se999.win32.scd#5", "se/se999/005.wav", 1.0),
    ("abil_fx_33", "se/se999.win32.scd#38", "se/se999/038.wav", 1.0),
    ("abil_fx_34", "se/se999.win32.scd#39", "se/se999/039.wav", 1.0),
    ("abil_fx_35", "se/se999.win32.scd#7", "se/se999/007.wav", 1.0),
    ("abil_fx_36", "se/se999.win32.scd#8", "se/se999/008.wav", 1.0),
    ("abil_fx_37", "se/se999.win32.scd#6", "se/se999/006.wav", 1.0),
    ("abil_fx_38", "se/se999.win32.scd#151", "se/se999/151.wav", 1.0),
    ("abil_fx_39", "se/se999.win32.scd#1", "se/se999/001.wav", 1.0),
    ("abil_fx_41", "se/se999.win32.scd#40", "se/se999/040.wav", 1.0),
    ("abil_fx_44", "se/se999.win32.scd#38", "se/se999/038.wav", 1.0),
    ("abil_fx_48", "se/se999.win32.scd#42", "se/se999/042.wav", 1.0),
    ("abil_fx_50", "se/se999.win32.scd#3", "se/se999/003.wav", 1.0),
    ("abil_fx_51", "se/se000.win32.scd#169", "se/se000/169.wav", 1.0),
    ("abil_fx_52", "se/se999.win32.scd#21", "se/se999/021.wav", 1.0),
]


def main():
    src, dst = sys.argv[1], sys.argv[2]
    if os.path.abspath(src) == os.path.abspath(dst):
        sys.exit("refusing to overwrite the input")
    d = open(src, "rb").read()
    magic, ver, ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes, _ = struct.unpack_from("<4s11I", d, 0)
    assert magic == b"SKSF" and ver == 1
    js = json.load(open(sndsrc.sounds_json()))["sounds"]
    o = 48
    sounds = d[o:o + 72 * ns]; o += 72 * ns
    mid = d[o:o + 28 * nt + 4 * nts + 16 * nv + 8 * nc]; o += len(mid)
    names = d[o:o + 36 * nn]; o += 36 * nn
    hits = d[o:o + 8 * nh]
    pcm = bytearray(d[data_off:data_off + data_bytes])
    have = {sounds[i * 72:i * 72 + 48].split(b"\0")[0].decode() for i in range(ns)}
    haven = {names[i * 36:i * 36 + 32].split(b"\0")[0].decode() for i in range(nn)}
    new_s, new_n = bytearray(), bytearray()
    for name, key, wav, vol in ADD:
        if name in haven:
            print("name already present:", name); continue
        if key in have:  # sound already packed: just a new name row
            idx = [sounds[i * 72:i * 72 + 48].split(b"\0")[0].decode() for i in range(ns)].index(key)
        else:
            with wave.open(sndsrc.wavrel(wav), "rb") as w:
                assert w.getsampwidth() == 2
                rate, ch, n, fr = w.getframerate(), w.getnchannels(), w.getnframes(), w.readframes(w.getnframes())
            while len(pcm) % 4: pcm.append(0)
            vol = float(js[key].get("volume", vol))
            new_s += struct.pack("<48sIIfIII", key.encode()[:47], rate, ch, vol, 0, len(pcm), n)
            pcm += fr
            idx = ns + len(new_s) // 72 - 1
            print(f"{name}: {key} rate {rate} ch {ch} vol {vol} frames {n} -> sound {idx}")
        new_n += struct.pack("<32sI", name.encode()[:31], idx)
    ns2, nn2 = ns + len(new_s) // 72, nn + len(new_n) // 36
    body = sounds + bytes(new_s) + mid + names + bytes(new_n) + hits
    body = bytearray(body)
    while (48 + len(body)) % 16: body.append(0)
    hdr = struct.pack("<4s11I", b"SKSF", 1, ns2, nt, nts, nv, nc, nn2, nh, 48 + len(body), len(pcm), 0)
    open(dst, "wb").write(hdr + body + pcm)
    print(f"{dst}: {ns2} sounds, {nn2} names, {len(pcm) / 1e6:.1f} MB PCM16")


if __name__ == "__main__":
    main()
