"""uisfx tests (no real KH2 needed): SCD subsong mapping on a synthetic bank, silence trim, cache stamp, error paths never raise."""
import array
import json
import os
import struct
import sys
import tempfile
import wave

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import uisfx  # noqa: E402

fails = []


def check(name, cond):
    print(("ok   " if cond else "FAIL ") + name)
    if not cond:
        fails.append(name)


def fake_scd():
    """3 sounds (v4 name block); audio 0 = codec 12, audio 1 = empty slot, audio 2 = codec 6 -> subsongs {0: 1, 2: 2}"""
    hs = 0x40
    d = bytearray(0x400)
    d[:8] = b"SEDBSSCF"
    struct.pack_into("<I", d, 8, 4)
    struct.pack_into("<H", d, 0x0E, hs)
    otrk, oaud = 0x100, 0x120
    struct.pack_into("<HHHHII", d, hs, 3, 1, 3, 0, otrk, oaud)
    snds = [0x200, 0, 0x260]                  # sound 1 is an empty slot
    struct.pack_into("<3I", d, hs + 0x20, *snds)
    struct.pack_into("<I", d, otrk, 0x1F0)
    struct.pack_into("<3I", d, oaud, 0x300, 0, 0x340)
    struct.pack_into("<IIII", d, 0x300, 100, 2, 48000, 12)
    struct.pack_into("<IIII", d, 0x340, 100, 2, 48000, 6)
    for o, aud in ((0x200, 0), (0x260, 2)):
        d[o], d[o + 3] = 1, 1                 # n tracks, type 1
        struct.pack_into("<HH", d, o + 0x10 + 0x30, 0, aud)
    return bytes(d)


def write_wav(p, frames, rate=8000):
    with wave.open(p, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(frames.tobytes())


def main():
    check("subsong map", uisfx.scd_subsongs(fake_scd()) == {0: 1, 2: 2})
    try:
        uisfx.scd_subsongs(b"nope" * 10)
        check("bad scd raises", False)
    except ValueError:
        check("bad scd raises", True)
    with tempfile.TemporaryDirectory() as t:
        a = array.array("h", [3000, -3000] * 4000 + [0, 0] * 8000)     # 0.5 s of sound + 1 s of silence at 8 kHz stereo
        src, dst = os.path.join(t, "a.wav"), os.path.join(t, "b.wav")
        write_wav(src, a)
        uisfx.trim_wav(src, dst)
        with wave.open(dst) as w:
            n = w.getnframes()
        check("trim cuts the silent tail", 4000 < n < 4000 + 8000 * 0.1)
        write_wav(src, array.array("h", [0, 0] * 4000))
        try:
            uisfx.trim_wav(src, dst)
            check("silent file rejected", False)
        except ValueError:
            check("silent file rejected", True)

        root, img = os.path.join(t, "root"), os.path.join(t, "dt")
        os.makedirs(img)
        open(os.path.join(img, "kh2_first.hed"), "wb").write(b"x")
        open(os.path.join(img, "kh2_first.pkg"), "wb").write(b"yy")
        logs = []
        check("not cached at first", not uisfx.cached(root, img) and not uisfx.ready(root))
        # error path: a kit that does not exist -> False, no exception, nothing half-written that claims to be ready
        check("missing kit = False", uisfx.build(root, img, os.path.join(t, "nokit"), logs.append) is False)
        check("no stamp after a failure", not uisfx.ready(root) and not uisfx.cached(root, img))
        check("failure logged", any("uisfx" in l for l in logs))
        check("temp dir cleaned", not [x for x in os.listdir(uisfx.sfx_dir(root)) if x.startswith("w")])
        check("bad dt = False", uisfx.build(root, os.path.join(t, "nodt"), os.path.join(t, "nokit")) is False)
        # cache: stamp + files present -> build returns True at once (the bad kit would fail otherwise)
        d = uisfx.sfx_dir(root)
        for n in uisfx.NAMES:
            write_wav(os.path.join(d, n + ".wav"), array.array("h", [100, 100] * 100))
        json.dump(uisfx.stamp_for(img), open(os.path.join(d, "stamp.json"), "w"))
        check("cached", uisfx.cached(root, img) and uisfx.ready(root))
        check("cache hit skips the work", uisfx.build(root, img, os.path.join(t, "nokit")) is True)
        open(os.path.join(img, "kh2_first.pkg"), "wb").write(b"zzz")    # the KH2 file changed -> stale
        check("stamp tracks the KH2 file sizes", not uisfx.cached(root, img))
        check("stale + bad kit = False", uisfx.build(root, img, os.path.join(t, "nokit")) is False)
    print("FAILED: %s" % fails if fails else "all ok")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
