#!/usr/bin/env python3
"""(s110 skate 1b) Appends the KH2 skateboard's sounds (F_TT010.a.us / se/obj/se334, an SCD with 11 MS-ADPCM sounds) to an EXISTING
sfx.bin (SKSF v1), like tools_s110/roxsfx/sfxroxas.py: every older sound / trigger / vtbl / choice / name / hit row and the old PCM stay
byte-identical (asserted). Sounds only, no triggers: runtime/sora_skate.h plays them itself.

  keys  "se/se334.win32.scd#<15 + i>"  for SCD sound i = 0..10  (the board's frame triggers say se334 #18..#24 = SCD sounds 3..9: index - 15,
        the same offset as PHH's se705: index 20 = subsong 5; 15 = the SCD's track count)
  sound 0 is the roll loop: its loop region (the SCD's loop start / end bytes, scaled to samples) is cut out and flagged `loop`
  name "skatese_v1" (id = sound 0) marks the pack.
Decoding: the SCD (codec 12 = MS ADPCM, WAVEFORMATEX in the 0x20 extradata) is wrapped into a WAV by hand and decoded by ffmpeg (mono, 48 kHz, s16).
Usage: python -I sfxskate.py <in sfx.bin> <out sfx.bin> [se334 scd]      (refuses to overwrite the input)
"""
import os, struct, subprocess, sys, tempfile, wave

SCD = os.path.join(os.environ["SB_KH2"], "kh2_first/remastered/obj/F_TT010.a.us/se/obj/se334")
FFMPEG = os.environ.get("SB_FFMPEG", "ffmpeg")  # ffmpeg.exe (config "ffmpeg" or PATH): decodes the SCD MS-ADPCM, mixes to mono and resamples to 48 kHz (swr: not reproducible in python)


def load(path):
    d = open(path, "rb").read()
    magic, ver, ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes, _ = struct.unpack_from("<4s11I", d, 0)
    assert magic == b"SKSF" and ver == 1
    return d, (ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes)


def scd_sounds(path, tmp):
    d = open(path, "rb").read()
    hs = struct.unpack_from("<H", d, 0x0E)[0]
    nsnd, ntrk, nattr, _, otrk, osnd = struct.unpack_from("<HHHHII", d, hs)
    offs = struct.unpack_from("<%dI" % nsnd, d, osnd)
    out = []
    for i, o in enumerate(offs):
        if not o:
            continue
        ssz, ch, rate, codec, ls, le, first, aux, _ = struct.unpack_from("<IIIIIIIHH", d, o)
        if codec != 12 or ssz > 5_000_000:
            continue
        ext = o + 0x20
        cb = struct.unpack_from("<H", d, ext + 16)[0]
        fmt = d[ext:ext + 18 + cb]
        data = d[ext + 18 + cb:ext + 18 + cb + ssz]
        w = b"RIFF" + struct.pack("<I", 4 + 8 + len(fmt) + 8 + len(data)) + b"WAVEfmt " + struct.pack("<I", len(fmt)) + fmt + b"data" + struct.pack("<I", len(data)) + data
        src, dst = os.path.join(tmp, "s%d.wav" % i), os.path.join(tmp, "m%d.wav" % i)
        open(src, "wb").write(w)
        subprocess.run([FFMPEG, "-v", "error", "-y", "-i", src, "-ac", "1", "-ar", "48000", "-acodec", "pcm_s16le", dst], check=True)
        with wave.open(dst, "rb") as r:
            assert r.getsampwidth() == 2 and r.getnchannels() == 1
            n = r.getnframes()
            fr = r.readframes(n)
        loop = False
        if le > ls and ls > 0:  # the loop region in stream bytes -> samples (the stream is uniform ADPCM)
            a, b = int(ls / ssz * n), int(le / ssz * n)
            fr = fr[2 * a:2 * b]
            n = b - a
            loop = True
        out.append((i, 48000, n, fr, loop))
    return out


def main():
    src, dst = sys.argv[1], sys.argv[2]
    scd = sys.argv[3] if len(sys.argv) > 3 else SCD
    if os.path.abspath(src) == os.path.abspath(dst):
        sys.exit("refusing to overwrite the input")
    d, (ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes) = load(src)
    o = 48
    sec_sounds = d[o:o + 72 * ns]; o += 72 * ns
    sec_trig = d[o:o + 28 * nt]; o += 28 * nt
    sec_ts = d[o:o + 4 * nts]; o += 4 * nts
    sec_vt = d[o:o + 16 * nv]; o += 16 * nv
    sec_ch = d[o:o + 8 * nc]; o += 8 * nc
    sec_names = d[o:o + 36 * nn]; o += 36 * nn
    sec_hits = d[o:o + 8 * nh]
    pcm = bytearray(d[data_off:data_off + data_bytes])
    names = {sec_names[i * 36:i * 36 + 32].split(b"\0")[0].decode() for i in range(nn)}
    keys = {sec_sounds[i * 72:i * 72 + 48].split(b"\0")[0].decode() for i in range(ns)}
    assert "skatese_v1" not in names, "already has the skateboard sounds"
    new_s = bytearray(); new_keys = []
    with tempfile.TemporaryDirectory() as tmp:
        snds = scd_sounds(scd, tmp)
        assert len(snds) == 11, len(snds)
        for i, rate, n, fr, loop in snds:
            key = "se/se334.win32.scd#%d" % (15 + i)
            assert key not in keys, key
            while len(pcm) % 4:
                pcm.append(0)
            new_s.extend(struct.pack("<48sIIfIII", key.encode(), rate, 1, 1.0, 1 if loop else 0, len(pcm), n))
            pcm.extend(fr)
            new_keys.append(key)
    marker = ns  # sound 0 (the roll loop)
    new_names = struct.pack("<32sI", b"skatese_v1", marker)
    body = bytearray(sec_sounds + bytes(new_s) + sec_trig + sec_ts + sec_vt + sec_ch + sec_names + new_names + sec_hits)
    while (48 + len(body)) % 16:
        body.append(0)
    hdr = struct.pack("<4s11I", b"SKSF", 1, ns + len(new_keys), nt, nts, nv, nc, nn + 1, nh, 48 + len(body), len(pcm), 0)
    open(dst, "wb").write(hdr + body + pcm)
    print(f"{dst}: +{len(new_keys)} sounds ({len(pcm) - data_bytes} bytes PCM16), +1 name (skatese_v1)")
    n2, (ns_, nt_, nts_, nv_, nc_, nn_, nh_, do2, db2) = load(dst)
    o2 = 48
    assert n2[o2:o2 + 72 * ns] == sec_sounds
    o2 += 72 * ns_
    assert n2[o2:o2 + 28 * nt] == sec_trig
    o2 += 28 * nt_
    assert n2[o2:o2 + 4 * nts] == sec_ts
    o2 += 4 * nts_
    assert n2[o2:o2 + 16 * nv] == sec_vt
    o2 += 16 * nv_
    assert n2[o2:o2 + 8 * nc] == sec_ch
    o2 += 8 * nc_
    assert n2[o2:o2 + 36 * nn] == sec_names and nn_ == nn + 1
    o2 += 36 * nn_
    assert n2[o2:o2 + 8 * nh] == sec_hits and nh_ == nh
    assert n2[do2:do2 + data_bytes] == d[data_off:data_off + data_bytes]
    print("VERIFIED: old sounds / trigger / trig-sound / vtbl / choice / name / hit rows and the old PCM are byte-identical")


if __name__ == "__main__":
    main()
