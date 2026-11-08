"""Small readers for the vanilla KH2 files the sound miners need (BAR, mset motions + triggers, 00battle tables).

Only numpy-free standard Python, so it runs on the kit's embeddable Python.  Every format here is the plain on-disk
layout of the PC (FM) extract D:/KH2Extract/kh2_<pkg>/{original,remastered}/...
"""
import os
import struct

PKGS = ("kh2_first", "kh2_second", "kh2_third", "kh2_fourth", "kh2_fifth", "kh2_sixth")


class Kh2:
    """the player's KH2 extract; find(rel) = first kh2_<pkg>/original/<rel> that exists (sixth wins over first for obj/,
    the same order the trigjson tool uses), cached reads."""

    def __init__(self, root):
        self.root = root
        self._cache = {}

    def find(self, rel, prefer=("kh2_sixth", "kh2_first", "kh2_second", "kh2_third", "kh2_fourth", "kh2_fifth")):
        for pk in prefer:
            p = os.path.join(self.root, pk, "original", rel)
            if os.path.exists(p):
                return p
        return None

    def read(self, rel):
        if rel not in self._cache:
            p = self.find(rel)
            if p is None:
                raise SystemExit("KH2 file missing: %s (in %s)" % (rel, self.root))
            with open(p, "rb") as f:
                self._cache[rel] = f.read()
        return self._cache[rel]

    def exists(self, rel):
        return self.find(rel) is not None


def bar(d):
    """[(type, name, bytes)] of a BAR container ('BAR\x01', u32 count, 16-byte entries from 0x10)"""
    if d[:3] != b"BAR":
        raise ValueError("not a BAR")
    n = struct.unpack_from("<I", d, 4)[0]
    out = []
    for i in range(n):
        t, _dup, nm, o, s = struct.unpack_from("<HH4sII", d, 16 + 16 * i)
        out.append((t, nm.split(b"\0")[0].decode("latin1"), d[o:o + s]))
    return out


T_MOTION, T_TRIGGERS, T_ANB = 9, 16, 17


def triggers(t):
    """motion trigger entry (BAR type 16): u8 range count, u8 frame count, u16 byte offset of the frame list;
    range = i16 start, i16 end, u8 type, u8 nparams, i16 params[n]; frame = i16 frame, u8 type, u8 nparams, i16 params[n].
    -> (ranges [(type, start, end, params)], frames [(type, frame, params)])"""
    nr, nf, fo = struct.unpack_from("<BBH", t, 0)
    o, rng = 4, []
    for _ in range(nr):
        s, e, ty, n = struct.unpack_from("<hhBB", t, o)
        rng.append((ty, s, e, list(struct.unpack_from("<%dh" % n, t, o + 6))))
        o += 6 + 2 * n
    o, frm = fo, []
    for _ in range(nf):
        f, ty, n = struct.unpack_from("<hBB", t, o)
        frm.append((ty, f, list(struct.unpack_from("<%dh" % n, t, o + 4))))
        o += 4 + 2 * n
    return rng, frm


def motion_info(m):
    """interpolated motion (BAR type 9): 0x90 reserved bytes, 16-byte header, then the interpolated header at 0xa0:
    i16 bone count, i16 total bones, i32 total frame count, then at +0x80 the frame data
    f32 start, end, fps, return (loop point).  -> (total frames, start, end, return)"""
    h = 0xa0
    total = struct.unpack_from("<i", m, h + 4)[0]
    fs, fe, _fps, fr = struct.unpack_from("<4f", m, h + 0x80)
    return total, fs, fe, fr


def mset(d):
    """[(index, name, motion bytes or None, trigger bytes or None)] of every non-empty mset entry (a BAR of anb BARs)"""
    out = []
    for i, (t, nm, e) in enumerate(bar(d)):
        if not e:
            continue
        try:
            inner = bar(e)
        except ValueError:
            continue
        mo = next((x[2] for x in inner if x[0] == T_MOTION), None)
        tr = next((x[2] for x in inner if x[0] == T_TRIGGERS), None)
        out.append((i, nm, mo, tr))
    return out


def scd_cues(d):
    """{cue (sound) number: first audio stream index or None} of a KH2 PC .scd.  Header table at u16 @0x0E:
    u16 n_cues, n_tracks, n_streams, ?; u32 offsets: tracks, streams, cue entries, 0, ...; the table right after this
    0x20-byte header = u32 offset of each cue's track list.  A track list starts with u8 track count; with count > 0 the
    first (u16 track, u16 stream) pair is at +16 (count 0 = an empty cue, no audio).
    Verified on the voice/us/battle files (every recipe voice: cue -> stream as in sounds_recipe.json); the stream
    number is NOT right for some se/*.scd banks (e.g. se000), only use the "has audio" answer there."""
    hs = struct.unpack_from("<H", d, 0x0E)[0]
    ncue = struct.unpack_from("<H", d, hs)[0]
    olist = hs + 0x20
    out = {}
    for i in range(ncue):
        o = struct.unpack_from("<I", d, olist + 4 * i)[0]
        if not o or d[o] == 0:
            out[i] = None
        else:
            out[i] = struct.unpack_from("<H", d, o + 18)[0]
    return out
