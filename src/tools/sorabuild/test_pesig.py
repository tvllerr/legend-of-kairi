"""Host test for pesig.py (s201): a signed copy of a native file matches its unsigned pin, a changed file does not.

  python -I tools/sorabuild/test_pesig.py [--unsigned <dir>] [--signed <dir>]
      --unsigned / --signed: two folders with the same native files, the second as a signer left them (default: the kit's
      vgmstream and the copy Melty installed and signed, s201). Only read. Temp files go to the TMP folder.
Also runs the synthetic cases (a copy signed here by hand, broken PEs, a non-PE) and build_sounds.find_vgmstream on both exes.
"""
import argparse
import contextlib
import hashlib
import io
import os
import shutil
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "sounds"))
import pesig  # noqa: E402

UNSIGNED = "D:/SoraBotW/sb/kit185/vgmstream"
SIGNED = os.path.join(os.environ.get("APPDATA", ""), "Melty", "mods", "managed", "legend-of-kairi", "kairi", "kit", "vgmstream")
FAILS = []


def check(cond, what):
    print("%s  %s" % ("ok  " if cond else "FAIL", what))
    if not cond:
        FAILS.append(what)


def sha(b):
    return hashlib.sha256(b).hexdigest()


def put(d, name, b):
    p = os.path.join(d, name)
    with open(p, "wb") as f:
        f.write(b)
    return p


def fake_sign(b, ck_mode="recompute"):
    """what a signer does: zero-pad to 8, append a table, point the Security entry at it, CheckSum recomputed / left / zeroed"""
    lay = pesig.layout(b)
    ck_off, sec = lay[0], lay[1]
    out = bytearray(b) + b"\0" * (-len(b) % 8)
    table = struct.pack("<IHH", 8 + 40, 0x200, 2) + os.urandom(40)
    struct.pack_into("<II", out, sec, len(out), len(table))
    out += table
    if ck_mode == "recompute":
        struct.pack_into("<I", out, ck_off, pesig.pe_checksum(out, ck_off))
    elif ck_mode == "zero":
        struct.pack_into("<I", out, ck_off, 0)
    return bytes(out)


def code_offset(b):
    """file offset of the middle of the first section with code (IMAGE_SCN_CNT_CODE)"""
    pe = struct.unpack_from("<I", b, 0x3C)[0]
    nsec, opt_size = struct.unpack_from("<H", b, pe + 6)[0], struct.unpack_from("<H", b, pe + 20)[0]
    st = pe + 24 + opt_size
    for i in range(nsec):
        rsize, rptr = struct.unpack_from("<II", b, st + 40 * i + 16)
        if struct.unpack_from("<I", b, st + 40 * i + 36)[0] & 0x20 and rsize:
            return rptr + rsize // 2
    raise SystemExit("no code section")


def real_pairs(ud, sd):
    names = sorted(n for n in os.listdir(ud) if n.lower().endswith((".exe", ".dll")))
    check(len(names) == 12 and sorted(os.listdir(sd)) == sorted(os.listdir(ud)), "real pair: 12 native files in both folders (%d)" % len(names))
    for n in names:
        u, s = os.path.join(ud, n), os.path.join(sd, n)
        ub, sb_ = open(u, "rb").read(), open(s, "rb").read()
        lay = pesig.layout(sb_)
        pin = sha(ub)
        ck_u = struct.unpack_from("<I", ub, lay[0])[0]
        ck_s = struct.unpack_from("<I", sb_, lay[0])[0]
        pad = lay[2] - len(ub)
        check(pesig.signed(s) and not pesig.signed(u) and sha(sb_) != pin, "%-26s signed copy differs (+%d bytes, pad %d, CheckSum %08x -> %08x)"
              % (n, len(sb_) - len(ub), pad, ck_u, ck_s))
        check(pesig.stripped_sha256(s) == pesig.stripped_sha256(u), "%-26s stripped sha256 signed == unsigned (%s)" % (n, pesig.stripped_sha256(s)[:12]))
        check(pesig.matches(s, pin, len(ub)) == "signed" and pesig.matches(s, pin) == "signed", "%-26s signed copy matches the plain pin" % n)
        check(pesig.matches(u, pin, len(ub)) == "unsigned", "%-26s original matches its pin (unsigned)" % n)
        if ck_u:
            check(pesig.pe_checksum(ub, lay[0]) == ck_u, "%-26s pe_checksum() == the linker's CheckSum %08x" % (n, ck_u))
        check(pesig.pe_checksum(sb_, lay[0]) == ck_s, "%-26s pe_checksum() == the signer's CheckSum %08x" % (n, ck_s))
    return names


def flipped(ud, sd, tmp):
    for n in ("vgmstream-cli.exe", "libvorbis.dll"):
        ub, sb_ = open(os.path.join(ud, n), "rb").read(), open(os.path.join(sd, n), "rb").read()
        o = code_offset(sb_)
        bad = bytearray(sb_)
        bad[o] ^= 0x01
        p = put(tmp, "flip_" + n, bytes(bad))
        check(pesig.matches(p, sha(ub)) is None and pesig.matches(p, sha(ub), len(ub)) is None,
              "%s signed copy with one code byte flipped (0x%x) is refused" % (n, o))
        check(pesig.stripped_sha256(p) != pesig.stripped_sha256(os.path.join(ud, n)), "%s flipped: stripped sha256 differs" % n)
        ubad = bytearray(ub)
        ubad[o] ^= 0x01
        p = put(tmp, "flipu_" + n, bytes(ubad))
        check(pesig.matches(p, sha(ub), len(ub)) is None, "%s unsigned with one code byte flipped is refused" % n)
    a, b = os.path.join(sd, "libvorbis.dll"), open(os.path.join(ud, "vgmstream-cli.exe"), "rb").read()
    check(pesig.matches(a, sha(b)) is None, "another signed native file (libvorbis.dll) does not match the exe pin")


def synthetic(ud, tmp):
    for n, base in (("vgmstream-cli.exe", None), ("libmpg123-0.dll", None)):
        ub = open(os.path.join(ud, n), "rb").read()
        for mode in ("recompute", "keep", "zero"):
            p = put(tmp, "%s.%s" % (mode, n), fake_sign(ub, mode))
            check(pesig.matches(p, sha(ub), len(ub)) == "signed" and pesig.matches(p, sha(ub)) == "signed",
                  "synthetic sign (%s CheckSum) of %s matches" % (mode, n))
            check(pesig.stripped_sha256(p) == pesig.stripped_sha256(os.path.join(ud, n)), "synthetic %s %s: stripped equal" % (mode, n))
    ub = open(os.path.join(ud, "vgmstream-cli.exe"), "rb").read()
    sg = fake_sign(ub)
    # the wrong size given: refused
    p = put(tmp, "s.exe", sg)
    check(pesig.matches(p, sha(ub), len(ub) + 8) is None, "signed copy with a wrong pinned size is refused")
    # non-PE files: the plain sha256
    txt = b"just some data, not a PE\n" * 100
    p = put(tmp, "data.bin", txt)
    check(pesig.stripped_sha256(p) == sha(txt) and pesig.matches(p, sha(txt), len(txt)) == "unsigned" and pesig.matches(p, "0" * 64) is None,
          "non-PE: stripped == plain sha256, plain pin matches, a wrong one does not")
    p = put(tmp, "empty.bin", b"")
    check(pesig.stripped_sha256(p) == sha(b"") and pesig.matches(p, sha(b"")) == "unsigned", "empty file: plain sha256")
    # broken / truncated PEs: no crash, plain sha256, a pin of the original is refused
    lay = pesig.layout(sg)
    cases = {"truncated signed (table cut short)": sg[:-100], "truncated to 300 bytes": sg[:300], "MZ only": b"MZ",
             "MZ + garbage": b"MZ" + os.urandom(4000), "e_lfanew past the end": b"MZ" + b"\0" * 58 + struct.pack("<I", 0x7FFFFFF0),
             "PE header cut": sg[:struct.unpack_from("<I", sg, 0x3C)[0] + 30],
             "table past the end": sg[:lay[1]] + struct.pack("<II", lay[2], lay[3] + 64) + sg[lay[1] + 8:],
             "table offset 0, size set": sg[:lay[1]] + struct.pack("<II", 0, lay[3]) + sg[lay[1] + 8:],
             "random bytes": os.urandom(5000)}
    for what, b in cases.items():
        p = put(tmp, "broken.bin", b)
        try:
            r = pesig.matches(p, sha(ub)), pesig.matches(p, sha(ub), len(ub)), pesig.stripped_sha256(p), pesig.signed(p)
            ok = r[0] is None and r[1] is None and len(r[2]) == 64
        except Exception as e:  # noqa: BLE001
            ok, r = False, repr(e)
        check(ok, "broken PE (%s): refused, no crash" % what)


def vgm(ud, sd, tmp):
    import build_sounds
    for label, path, want in (("unsigned", os.path.join(ud, "vgmstream-cli.exe"), "pinned build)"),
                              ("signed", os.path.join(sd, "vgmstream-cli.exe"), "pinned build, signed)")):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            r = build_sounds.find_vgmstream(path)
        print("     " + out.getvalue().strip())
        check(r == path and out.getvalue().strip().endswith(want), "find_vgmstream accepts the %s exe" % label)
    wrong = os.path.join(sd, "libvorbis.dll")
    flip = os.path.join(tmp, "flip_vgmstream-cli.exe")
    for label, path in (("a different signed PE", wrong), ("the signed exe with a flipped code byte", flip)):
        try:
            with contextlib.redirect_stdout(io.StringIO()):
                build_sounds.find_vgmstream(path)
            check(False, "find_vgmstream refuses %s" % label)
        except SystemExit as e:
            print("     " + str(e))
            check("is not the pinned build" in str(e), "find_vgmstream refuses %s" % label)
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        r = build_sounds.find_vgmstream(wrong, any_build=True)
    check(r == wrong and "UNPINNED" in out.getvalue(), "--vgmstream-any still takes an unpinned build")


def bin_checks(ud, tmp):
    """installer gameside.install_game_side + melty-setup build.game_side on a fake release whose bin/ holds a signed copy"""
    import importlib.util
    import json
    sys.path.insert(0, os.path.join(HERE, "..", "installer"))
    import gameside
    spec = importlib.util.spec_from_file_location("melty_build", os.path.join(HERE, "..", "..", "melty-setup", "build.py"))
    mb = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mb)
    ub = open(os.path.join(ud, "vgmstream-cli.exe"), "rb").read()
    flip = bytearray(fake_sign(ub))
    flip[code_offset(ub)] ^= 1
    for label, exe, ok in (("unsigned", ub, True), ("signed", fake_sign(ub), True), ("signed + flipped byte", bytes(flip), False)):
        rel = os.path.join(tmp, "rel_" + label.replace(" ", "_").replace("+", ""))
        os.makedirs(os.path.join(rel, "bin"))
        for n in gameside.NEEDED:
            put(os.path.join(rel, "bin"), n, exe)
        with open(os.path.join(rel, "manifest.json"), "w") as f:
            json.dump({"bin": {n: {"size": len(ub), "sha256": sha(ub)} for n in gameside.NEEDED}}, f)
        lines = []
        try:   # a missing Savage Wind folder stops it right after bin/ (before launchers / shortcuts are written)
            gameside.install_game_side(os.path.join(rel, "root"), os.path.join(rel, "no_sw"), log=lines.append, release=rel)
            got = "no error"
        except RuntimeError as e:
            got = str(e)
        passed = "Savage Wind folder" in got
        check(passed == ok and (not ok or label != "signed" or "with a signature added" in lines[0]),
              "gameside bin check, %s: %s | %s" % (label, lines[0] if lines else "-", got[:60]))
        # melty-setup build.py game_side: with the release's pesig.py, and an old release without it (plain check)
        for with_pesig in (True, False):
            os.makedirs(os.path.join(rel, "tools", "sorabuild"), exist_ok=True)
            pp = os.path.join(rel, "tools", "sorabuild", "pesig.py")
            if with_pesig:
                shutil.copy2(os.path.join(HERE, "pesig.py"), pp)
            elif os.path.exists(pp):
                os.remove(pp)
            game, data = os.path.join(rel, "game%d" % with_pesig), os.path.join(rel, "data%d" % with_pesig)
            os.makedirs(data, exist_ok=True)
            out = io.StringIO()
            try:
                with contextlib.redirect_stdout(out):
                    mb.game_side(rel, os.path.join(rel, "stock"), data, game)
                got = "copied"
            except mb.Fail as e:
                got = str(e)
            want = ok if with_pesig else label == "unsigned"
            check((got == "copied") == want, "melty build.game_side, %s, %s: %s %s"
                  % (label, "release pesig.py" if with_pesig else "old release (plain check)", got[:50], out.getvalue().strip()[:70]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--unsigned", default=UNSIGNED)
    ap.add_argument("--signed", default=SIGNED)
    a = ap.parse_args()
    for d in (a.unsigned, a.signed):
        if not os.path.isdir(d):
            raise SystemExit("test_pesig: no folder %s" % d)
    tmp = tempfile.mkdtemp(prefix="test_pesig_")
    try:
        real_pairs(a.unsigned, a.signed)
        flipped(a.unsigned, a.signed, tmp)
        synthetic(a.unsigned, tmp)
        vgm(a.unsigned, a.signed, tmp)
        bin_checks(a.unsigned, tmp)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print("--\ntest_pesig: %s" % ("ALL OK" if not FAILS else "%d FAILED" % len(FAILS)))
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
