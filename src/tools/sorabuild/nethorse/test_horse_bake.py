"""Host test for nethorse/horse_bake.py's coat AS (s291 horsefix): synthetic BFRES / AS / AS list bytes (no game bytes).

  python -E -B tools/sorabuild/nethorse/test_horse_bake.py [--romfs <BotW romfs> --out <scratch dir>]
Checks: fmaa_frames reads each FMAA's name + frame count (and {} for a non-BFRES); coat_family maps the Collar clips' FileName
to D / T / E / Z (None for another AS); coat_as holds every shader-param / texture-pattern clip at the frame (Rate 0,
StartFrame n) and leaves the other elements alone; coat_files makes one AS per frame 0..the frame count, one family once,
cut at 32 frames, sorted D T E Z, none without the model's anim; the letters / frame cap equal sora_net_objs_core.h's.
With --romfs (read only) it also bakes into --out and checks GameRomHorse01's puppet: 64 coat AS (D 0-30, T 0-30, E 0, Z 0),
OE_Coat_D_07 held at frame 7, and the horse's own AS list entries first and unchanged.
"""
import argparse
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import oead  # noqa: E402
import horse_bake as hb  # noqa: E402

FAILS = []
RATE, START = 2145292295, 1509428672  # (crc32 "Rate" / "StartFrame": as BotW's AS FrameCtrl0 keys)


def check(cond, what):
    print("%s  %s" % ("ok  " if cond else "FAIL", what))
    if not cond:
        FAILS.append(what)


def fres(anims):
    """a little BFRES: "FRES", then per anim an FMAA block (+0x10 the name's offset, +0x68 the frame count) + its name"""
    d = bytearray(b"FRES" + b"\0" * 0x1c)
    for name, frames in anims:
        o = len(d)
        d += b"FMAA" + b"\0" * 0x6c
        no = len(d)
        d += struct.pack("<H", len(name)) + name.encode() + b"\0"
        struct.pack_into("<Q", d, o + 0x10, no)
        struct.pack_into("<i", d, o + 0x68, frames)
    return bytes(d)


def bas(clips):
    """an AS: Elements: Element0 the root (TypeIndex 80), then one element per (TypeIndex, FileName, has FrameCtrl)"""
    io = oead.aamp.ParameterIO()
    io.type = "xml"
    els = oead.aamp.ParameterList()
    for i, (ti, fn, fc) in enumerate([(80, None, False)] + clips):
        e = oead.aamp.ParameterList()
        par = oead.aamp.ParameterObject()
        par.params["TypeIndex"] = oead.aamp.Parameter(oead.S32(ti))
        if fn:
            par.params["FileName"] = oead.aamp.Parameter(oead.FixedSafeString64(fn))
        e.objects["Parameters"] = par
        if fc:
            ext, fcl, o = oead.aamp.ParameterList(), oead.aamp.ParameterList(), oead.aamp.ParameterObject()
            o.params["Rate"] = oead.aamp.Parameter(oead.F32(1.0))
            o.params["EndFrame"] = oead.aamp.Parameter(oead.F32(30.0))
            fcl.objects["FrameCtrl0"] = o
            ext.lists["FrameCtrl"] = fcl
            e.lists["Extend"] = ext
        els.lists["Element%d" % i] = e
    io.lists["Elements"] = els
    return bytes(io.to_binary())


def asl(defs):
    io = oead.aamp.ParameterIO()
    io.type = "xml"
    d = oead.aamp.ParameterList()
    for i, (nm, fn) in enumerate(defs):
        o = oead.aamp.ParameterObject()
        o.params["Name"] = oead.aamp.Parameter(oead.FixedSafeString32(nm))
        o.params["Filename"] = oead.aamp.Parameter(oead.FixedSafeString64(fn))
        d.objects["ASDefine_%d" % i] = o
    io.lists["ASDefines"] = d
    return hb.pio(bytes(io.to_binary()))


class Pack:
    def __init__(self, files):
        self.files = files

    def get_file(self, name):
        if name not in self.files:
            return None
        return type("F", (), {"name": name, "data": self.files[name]})()


class FakeRstb:
    def get(self, name):
        return {"Actor/AS/Player_Wait.bas": 0x800, "Actor/AS/Horse_Collar_D.bas": 0x900}.get(name)


def frames_of(io_bytes):
    io = hb.pio(io_bytes)
    out = []
    for e in io.lists["Elements"].lists.values():
        ti = int(e.objects["Parameters"].params["TypeIndex"].v)
        fc = None
        if "Extend" in e.lists and "FrameCtrl" in e.lists["Extend"].lists:
            o = e.lists["Extend"].lists["FrameCtrl"].objects["FrameCtrl0"]
            fc = {k.hash: float(v.v) for k, v in o.params.items()}
        out.append((ti, fc))
    return out


def unit():
    fr = hb.fmaa_frames(fres([("Horse_Link_Collar_Docile_fsp", 30), ("Horse_Link_Collar_Epona_fsp", 0), ("Horse_Link_Foot_fvm", 2)]))
    check(fr == {"Horse_Link_Collar_Docile_fsp": 30, "Horse_Link_Collar_Epona_fsp": 0, "Horse_Link_Foot_fvm": 2}, "fmaa_frames: names + frame counts %r" % fr)
    check(hb.fmaa_frames(b"SARC" + b"\0" * 64) == {}, "fmaa_frames: not a BFRES -> {}")

    d = bas([(62, "Horse_Link_Collar_Docile", True), (83, "Horse_Link_Collar_Docile", False), (67, "Horse_Link_Run", True)])
    check(hb.coat_family(hb.pio(d)) == ("D", "Horse_Link_Collar_Docile"), "coat_family: Docile -> D")
    for k, v in (("Touchy", "T"), ("Epona", "E"), ("Zelda", "Z")):
        check(hb.coat_family(hb.pio(bas([(62, "Horse_Link_Collar_" + k, False)])))[0] == v, "coat_family: %s -> %s" % (k, v))
    check(hb.coat_family(hb.pio(bas([(67, "Horse_Link_Collar_Docile", False)])))[0] is None, "coat_family: a TypeIndex 67 clip is no coat")
    check(hb.coat_family(hb.pio(bas([(62, "Horse_Link_Mane", False)])))[0] is None, "coat_family: another shader anim is no coat")

    got = frames_of(hb.coat_as(d, 7))
    check(got[0] == (80, None), "coat_as: the root element untouched")
    check(got[1] == (62, {RATE: 0.0, hb.crc("EndFrame"): 30.0, START: 7.0}), "coat_as: shader-param clip Rate 0, StartFrame 7, its EndFrame kept %r" % (got[1],))
    check(got[2] == (83, {RATE: 0.0, START: 7.0}), "coat_as: texture-pattern clip gets a FrameCtrl0 at 7 %r" % (got[2],))
    check(got[3] == (67, {RATE: 1.0, hb.crc("EndFrame"): 30.0}), "coat_as: a TypeIndex 67 clip untouched")
    check(hb.coat_as(d, 7) == hb.coat_as(d, 7), "coat_as: the same input -> the same bytes")

    b = object.__new__(hb.Baker)
    b.rstb = FakeRstb()
    pack = Pack({"Actor/AS/Horse_Collar_D.bas": d, "Actor/AS/Horse_Collar_D2.bas": d,
                 "Actor/AS/Horse_Collar_T.bas": bas([(62, "Horse_Link_Collar_Touchy", True)]),
                 "Actor/AS/Horse_Collar_E.bas": bas([(62, "Horse_Link_Collar_Epona", True)]),
                 "Actor/AS/Horse_Collar_Z.bas": bas([(62, "Horse_Link_Collar_Zelda", True)])})
    a2 = asl([("Run", "Horse_Run"), ("Collar_Shader", "Horse_Collar_D"), ("Collar_Shader_Coward", "Horse_Collar_T"),
              ("Collar_Shader_Impatient", "Horse_Collar_D2"), ("Collar_Shader_ZeldaHorse", "Horse_Collar_Z"),
              ("Collar_Shader_EponaHorse", "Horse_Collar_E"), ("Collar_Shader_Missing", "Horse_Nope")])
    frames = {"Horse_Link_Collar_Docile_fsp": 30, "Horse_Link_Collar_Docile_ftp": 30, "Horse_Link_Collar_Touchy_ftp": 40,
              "Horse_Link_Collar_Epona_fsp": 0}
    out, note = b.coat_files(pack, a2, frames)
    names = [x[0] for x in out]
    want = ["Coat_D_%02d" % i for i in range(31)] + ["Coat_T_%02d" % i for i in range(32)] + ["Coat_E_00"]
    check(names == want, "coat_files: D 0-30, T cut to 0-31, E 0, sorted D T E (%d AS)" % len(names))
    check(all(x[1] == "OE_" + x[0] for x in out), "coat_files: file OE_<name>")
    check(out[0][3] == 0x900 and out[-1][3] == 0x800, "coat_files: RSTB size = max(the source AS, Player_Wait)")
    check(frames_of(out[12][2])[1][1][START] == 12.0, "coat_files: Coat_D_12 held at frame 12")
    check("Z: no Horse_Link_Collar_Zelda_fsp / _ftp in the model" in note and "cut to 31" in note, "coat_files: the note says Z had no anim, T was cut (%s)" % note)
    out2, note2 = b.coat_files(pack, asl([("Run", "Horse_Run")]), frames)
    check(out2 == [] and note2 == "", "coat_files: no Collar_Shader AS -> none")

    # (s291 coat) BotW's lookup: a binary search; the s291e bake's order (horse AS, then Coat_*) loses most of them
    s291e = ["Appear_Epona", "Courbette", "Slip", "SlipPost", "Surprised", "Swim", "Wait_Search", "Warp_In"] + ["Coat_D_%02d" % i for i in range(31)]
    lost = [n for n in s291e if hb.bot_find(s291e, n) < 0]
    check({"Slip", "SlipPost", "Surprised", "Swim", "Wait_Search"} <= set(lost), "bot_find: the s291e order loses Slip / Surprised / Swim / Wait_Search (%d lost)" % len(lost))
    srt = sorted(s291e, key=hb.as_name_key)
    check(all(hb.bot_find(srt, n) == i for i, n in enumerate(srt)) and hb.bot_find(srt, "Wait") == -1 and hb.bot_find([], "x") == -1,
          "bot_find: sorted, every name found at its index; a missing one / an empty list -1")
    check(hb.bot_find(["Wait"], "Wait") == 0 and hb.bot_find(["Wait"], "Wai") == -1, "bot_find: one entry")
    check(hb.as_name_key("Wait_A") > hb.as_name_key("Wait") and hb.as_name_key("Z") < hb.as_name_key("a"), "as_name_key: bytes (upper case first)")
    rows = [(oead.aamp.Parameter(oead.FixedSafeString32(n)), "OE_" + n) for n in ("Wait", "Coat_D_00", "Appear", "Coat_T_01")]
    d = hb.as_defines(rows)
    got = [(str(o.params["Name"].v), str(o.params["Filename"].v)) for o in d.objects.values()]
    check(got == [("Appear", "OE_Appear"), ("Coat_D_00", "OE_Coat_D_00"), ("Coat_T_01", "OE_Coat_T_01"), ("Wait", "OE_Wait")]
          and list(d.objects.keys()) == [oead.aamp.Name("ASDefine_%d" % i) for i in range(4)], "as_defines: sorted, ASDefine_0..3 %r" % got)

    # (s291 coat) flat_as: the "default" path first (as the PR); a dead end there -> the other children, depth first
    def tree(spec):
        io = oead.aamp.ParameterIO()
        io.type = "xml"
        els = oead.aamp.ParameterList()
        for i, (ti, fn, kids, strs) in enumerate(spec):
            e = oead.aamp.ParameterList()
            par = oead.aamp.ParameterObject()
            par.params["TypeIndex"] = oead.aamp.Parameter(oead.S32(ti))
            if fn:
                par.params["FileName"] = oead.aamp.Parameter(oead.FixedSafeString64(fn))
            e.objects["Parameters"] = par
            if kids:
                ch = oead.aamp.ParameterObject()
                for j, k in enumerate(kids):
                    ch.params["Child%d" % j] = oead.aamp.Parameter(oead.S32(k))
                e.objects["Children"] = ch
            if strs:
                ext, sa, o = oead.aamp.ParameterList(), oead.aamp.ParameterList(), oead.aamp.ParameterObject()
                for j, s in enumerate(strs):
                    o.params["Value%d" % j] = oead.aamp.Parameter(oead.FixedSafeString32(s))
                sa.objects["StringArray0"] = o
                ext.lists["StringArray"] = sa
                e.lists["Extend"] = ext
            els.lists["Element%d" % i] = e
        io.lists["Elements"] = els
        return bytes(io.to_binary())

    def clip_of(b):
        return None if b is None else str(hb.pio(b).lists["Elements"].lists["Element0"].objects["Parameters"].params["FileName"].v)
    t1 = tree([(61, None, [1, 2], None), (67, "First", None, None), (67, "Second", None, None)])
    check(clip_of(hb.flat_as(t1)) == "First", "flat_as: no default -> the first child's clip")
    t2 = tree([(61, None, [1, 2], ["x", "default"]), (67, "First", None, None), (67, "Second", None, None)])
    check(clip_of(hb.flat_as(t2)) == "Second", "flat_as: the \"default\" child's clip (the PR's pick)")
    t3 = tree([(61, None, [1, 3], ["x", "default"]), (47, None, [2], None), (67, "Deep", None, None), (41, None, None, None)])
    check(clip_of(hb.flat_as(t3)) == "Deep", "flat_as: the default ends in an empty element (Wait) -> the other branch's clip")
    t4 = tree([(61, None, [1], None), (41, None, None, None)])
    check(hb.flat_as(t4) is None, "flat_as: no clip anywhere -> None")
    t5 = tree([(61, None, [1], None), (47, None, [0], None)])
    check(hb.flat_as(t5) is None, "flat_as: a loop -> None (no hang)")

    core = open(os.path.join(HERE, "..", "..", "..", "runtime", "sora_net_objs_core.h"), encoding="utf-8").read()
    m = re.search(r"NHC_LETTER\[NHC_N\] = \{'(.)', *'(.)', *'(.)', *'(.)'\}", core)
    check(m is not None and "".join(m.groups()) == hb.COAT_ORDER, "COAT_ORDER = sora_net_objs_core.h NHC_LETTER")
    m = re.search(r"#define NH_COAT_FRAMES (\d+)", core)
    check(m is not None and int(m.group(1)) == hb.COAT_FRAMES, "COAT_FRAMES = NH_COAT_FRAMES")


def romfs(r, out):
    rc = subprocess.run([sys.executable, "-E", "-B", os.path.join(HERE, "horse_bake.py"), r, out], capture_output=True, text=True)
    check(rc.returncode == 0, "bake into %s: exit %d %s" % (out, rc.returncode, rc.stdout.strip()[-120:]))
    p = os.path.join(out, "Actor", "Pack", "OnlineHorse_GameRomHorse01.sbactorpack")
    s = oead.Sarc(oead.yaz0.decompress(open(p, "rb").read()))
    names = [f.name for f in s.get_files()]
    coat = sorted(n for n in names if n.startswith("Actor/AS/OE_Coat_"))
    want = sorted("Actor/AS/OE_Coat_%s_%02d.bas" % (f, i) for f, n in (("D", 31), ("T", 31), ("E", 1), ("Z", 1)) for i in range(n))
    check(coat == want, "GameRomHorse01 puppet: 64 coat AS, D 0-30, T 0-30, E 0, Z 0 (%d)" % len(coat))
    fr = frames_of(bytes(s.get_file("Actor/AS/OE_Coat_D_07.bas").data))
    check([f for t, f in fr if t in (62, 83)] == [{RATE: 0.0, START: 7.0}] * 2, "OE_Coat_D_07: both coat clips at frame 7 %r" % (fr,))
    lst = hb.pio(bytes(s.get_file("Actor/ASList/OnlineHorse_GameRomHorse01.baslist").data))
    defs = [str(o.params["Name"].v) for o in lst.lists["ASDefines"].objects.values()]
    keys = list(lst.lists["ASDefines"].objects.keys())
    check(defs == sorted(defs, key=hb.as_name_key) and keys == [oead.aamp.Name("ASDefine_%d" % i) for i in range(len(defs))],
          "its AS list: sorted by name, ASDefine_0..%d in that order" % (len(defs) - 1))
    check(sum(n.startswith("Coat_") for n in defs) == 64, "its AS list: the 64 Coat_* (%d entries)" % len(defs))
    check(all(hb.bot_find(defs, n) == i for i, n in enumerate(defs)), "its AS list: BotW's lookup finds every one of its %d AS" % len(defs))
    lab = ["Wait", "Wait_Search", "Surprised", "Slip", "SlipPost", "GotOff_From_Wild", "Swim", "Move_Gear1", "Coat_D_10", "Coat_T_30"]
    check(all(hb.bot_find(defs, n) >= 0 for n in lab), "its AS list: the s291e lab's AS (and Wait) are found: %s" % " ".join(lab))
    check("Actor/AS/OE_Horse_Link_Wait.bas" in names, "GameRomHorse01 puppet: Wait has a clip now (OE_Horse_Link_Wait)")
    bad = []  # (every puppet the bake made: all its AS found)
    for fn in sorted(os.listdir(os.path.join(out, "Actor", "Pack"))):
        s2 = oead.Sarc(oead.yaz0.decompress(open(os.path.join(out, "Actor", "Pack", fn), "rb").read()))
        for f in s2.get_files():
            if f.name.startswith("Actor/ASList/"):
                d2 = [str(o.params["Name"].v) for o in hb.pio(bytes(f.data)).lists["ASDefines"].objects.values()]
                if any(hb.bot_find(d2, n) != i for i, n in enumerate(d2)):
                    bad.append(fn)
    check(not bad, "every baked AS list: BotW's lookup finds all its AS %s" % bad)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--romfs")
    ap.add_argument("--out")
    a = ap.parse_args()
    unit()
    if a.romfs:
        romfs(a.romfs, a.out)
    print("%d FAILED" % len(FAILS) if FAILS else "all passed")
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
