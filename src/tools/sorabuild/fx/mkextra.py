#!/usr/bin/env python3
"""sorabuild: the two hand-made WAVs of data/sfx/extra, now scripted.

  python -E -P mkextra.py scd  --scd <se500.win32.scd> --index 10 --out <wav>
      Decodes ONE audio stream of a KH2 SCD bank (MS ADPCM, codec 2 in the WAVEFORMATEX / 12 in the SCD) to a PCM16 WAV,
      bit-exact to vgmstream-cli (the original se500 #10 'wall_hit' was `vgmstream-cli -s 11`; stream index N = subsong N+1).
      The decoder is the plain Microsoft ADPCM of vgmstream: the prediction (h1*c1 + h2*c2) is divided with >> 8 (floor),
      NOT truncated (ffmpeg's adpcm_ms truncates and is off by up to 70 LSB, so it is not used).  Proven on se500 #0, #10, #12
      against PHH's own vgmstream WAVs (all identical).
  python -E -P mkextra.py vol  --in <PHH wav> --volume 0.9 --out <wav>
      The runtime level-up chime = KH2 se000 #25 with the json's volume applied: every PCM16 sample * volume, truncated
      toward zero (int()), header unchanged.  Identical to the hand-made file.
"""
import argparse
import os
import struct
import wave

ADJ = [230, 230, 230, 230, 307, 409, 512, 614, 768, 614, 512, 409, 307, 230, 230, 230]


def scd_stream(scd, i, d=None):
    if d is None:
        d = open(scd, "rb").read()
    hs = struct.unpack_from("<H", d, 0x0E)[0]
    nsnd, ntrk, nattr, _, otrk, osnd = struct.unpack_from("<HHHHII", d, hs)
    offs = struct.unpack_from("<%dI" % nsnd, d, osnd)
    o = offs[i]
    assert o, "dummy stream %d" % i
    ssz, ch, rate, codec, ls, le, first, aux, _ = struct.unpack_from("<IIIIIIIHH", d, o)
    assert codec == 12, "stream %d is codec %d, not MS ADPCM" % (i, codec)
    ext = o + 0x20
    tag, nch, sr, abr, ba, bps, cb = struct.unpack_from("<HHIIHHH", d, ext)
    assert tag == 2 and nch == ch
    ex = d[ext + 18:ext + 18 + cb]
    ncoef = struct.unpack_from("<HH", ex, 0)[1]
    coefs = [struct.unpack_from("<hh", ex, 4 + 4 * k) for k in range(ncoef)]
    data = d[ext + 18 + cb:ext + 18 + cb + ssz]
    return ch, rate, ba, coefs, data


def decode_msadpcm(ch, ba, coefs, data):
    out = []
    for bo in range(0, len(data), ba):
        blk = data[bo:bo + ba]
        if len(blk) < 7 * ch:
            break
        pi = [blk[c] for c in range(ch)]
        delta = [struct.unpack_from("<h", blk, ch + 2 * c)[0] for c in range(ch)]
        s1 = [struct.unpack_from("<h", blk, 3 * ch + 2 * c)[0] for c in range(ch)]
        s2 = [struct.unpack_from("<h", blk, 5 * ch + 2 * c)[0] for c in range(ch)]
        samp = [[s2[c], s1[c]] for c in range(ch)]
        nibs = []
        for b in blk[7 * ch:]:
            nibs.append(b >> 4)
            nibs.append(b & 15)
        for k in range(len(nibs) // ch):
            for c in range(ch):
                nb = nibs[k * ch + c]
                sn = nb - 16 if nb & 8 else nb
                c1, c2 = coefs[pi[c]]
                p = (samp[c][-1] * c1 + samp[c][-2] * c2) >> 8
                p = max(-32768, min(32767, p + sn * delta[c]))
                samp[c].append(p)
                delta[c] = max(16, (ADJ[nb] * delta[c]) >> 8)
        for k in range(len(samp[0])):
            for c in range(ch):
                out.append(samp[c][k])
    return struct.pack("<%dh" % len(out), *out)


def write_wav(path, ch, rate, pcm):
    tmp = path + ".tmp"
    with wave.open(tmp, "wb") as w:
        w.setnchannels(ch)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm)
    os.replace(tmp, path)


def main():
    ap = argparse.ArgumentParser()
    sp = ap.add_subparsers(dest="cmd", required=True)
    a = sp.add_parser("scd")
    a.add_argument("--scd", required=True)
    a.add_argument("--index", type=int, required=True)
    a.add_argument("--out", required=True)
    b = sp.add_parser("vol")
    b.add_argument("--in", dest="inp", required=True)
    b.add_argument("--volume", type=float, required=True)
    b.add_argument("--out", required=True)
    n = ap.parse_args()
    os.makedirs(os.path.dirname(os.path.abspath(n.out)), exist_ok=True)
    if n.cmd == "scd":
        ch, rate, ba, coefs, data = scd_stream(n.scd, n.index)
        write_wav(n.out, ch, rate, decode_msadpcm(ch, ba, coefs, data))
    else:
        with wave.open(n.inp, "rb") as w:
            assert w.getsampwidth() == 2
            ch, rate, nf = w.getnchannels(), w.getframerate(), w.getnframes()
            raw = w.readframes(nf)
        s = struct.unpack("<%dh" % (len(raw) // 2), raw)
        pcm = struct.pack("<%dh" % len(s), *[max(-32768, min(32767, int(x * n.volume))) for x in s])
        write_wav(n.out, ch, rate, pcm)
    print("wrote", n.out)


if __name__ == "__main__":
    main()
