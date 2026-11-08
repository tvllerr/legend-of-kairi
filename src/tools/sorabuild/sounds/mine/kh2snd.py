"""Shared KH2 readers for the sound-recipe miners (s182, no PHH data): SCD sound banks, PAX effect elements, BAR files.

Everything here reads the player's own vanilla KH2 extract (<kh2>/kh2_<pkg>/{original,remastered}/...).

SCD (Square Enix sound bank, KH2 PC = SEDBSSCF v3/v4):
  0x0E u16                 offset of the table header (hs)
  hs+0x00 u16 n_sound, u16 n_track, u16 n_audio, u16 -, u32 track table, u32 audio table
  hs+0x20 u32[n_sound]     sound entry offsets (0 = none)
  sound entry: u8 n_tracks, u8 bus, u8 priority, u8 type (16 = empty slot), u16 attributes (bit 0 = loop), u16 -,
               f32 volume, u16 local number (= the entry index), u8, u8,
               [v4 only: a 0x30-byte block with the sound's name], then n_tracks x (u16 track, u16 audio stream)
               (types 12 / 13 = random / sequence: 8 bytes per track, + u16 weight, u16 slot); a recipe sound = track 0
  audio entry: u32 size, u32 channels, u32 rate, u32 codec (12 = MS ADPCM, 6 = Ogg Vorbis), u32 loop start, u32 loop end, ...
A KH2 sound id is bank * 1000 + index (se 999016 = se/se999.win32.scd sound 16; bank 0 = se000).
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
import build_sounds as bs  # noqa: E402  (scd_audio_streams: vgmstream's subsong numbering)

PKG_ORDER = ["kh2_first", "kh2_second", "kh2_third", "kh2_fourth", "kh2_fifth", "kh2_sixth"]
CODEC = {12: "adpcm", 6: "vorbis"}


def find_scd(kh2, rel):
    """'se/se000.win32.scd' -> 'kh2_first/original/se/se000.win32.scd' (same search as gen_recipe: packages in name order,
    original before remastered, first hit), or None."""
    for pk in sorted(d for d in os.listdir(kh2) if d.startswith("kh2_")):
        for sub in ("original", "remastered"):
            if os.path.exists(os.path.join(kh2, pk, sub, rel)):
                return pk + "/" + sub + "/" + rel.replace("\\", "/")
    return None


def kh2_file(kh2, rel):
    """an original/ file, last package wins (fxrec's rule, phf2write.kh2_path)"""
    hit = None
    for p in PKG_ORDER:
        c = os.path.join(kh2, p, "original", rel)
        if os.path.isfile(c):
            hit = c
    return hit


class Scd:
    def __init__(self, path):
        self.path = path
        d = self.d = open(path, "rb").read()
        if d[:8] != b"SEDBSSCF":
            raise ValueError("%s: not an SCD" % path)
        self.version = struct.unpack_from("<I", d, 8)[0]
        hs = struct.unpack_from("<H", d, 0x0E)[0]
        ns, nt, na, _, otrk, oaud = struct.unpack_from("<HHHHII", d, hs)
        self.snd_offs = struct.unpack_from("<%dI" % ns, d, hs + 0x20)
        self.aud_offs = struct.unpack_from("<%dI" % na, d, oaud)
        self.trk0 = struct.unpack_from("<I", d, otrk)[0] if nt else None
        self.streams = bs.scd_audio_streams(d)  # [(audio index, codec)] = subsong order
        self.subsong = {a: i + 1 for i, (a, _) in enumerate(self.streams)}

    def sound(self, i):
        """sound entry i -> dict(tracks=[(track, audio)], volume, attr, type) or None"""
        if i < 0 or i >= len(self.snd_offs) or not self.snd_offs[i]:
            return None
        d, o = self.d, self.snd_offs[i]
        n, typ = d[o], d[o + 3]
        attr = struct.unpack_from("<H", d, o + 4)[0]
        vol = struct.unpack_from("<f", d, o + 8)[0]
        core = o + 0x10 + (0x30 if self.version >= 4 else 0)
        step = 8 if typ in (12, 13) else 4  # random / sequence sounds: (track, audio, u16 weight, u16 slot) per track
        tracks = [struct.unpack_from("<HH", d, core + step * k) for k in range(n)]
        return {"tracks": tracks, "volume": vol, "attr": attr, "type": typ}

    def audio(self, a):
        """audio stream a -> (channels, rate, codec) or None"""
        if a >= len(self.aud_offs) or not self.aud_offs[a]:
            return None
        _, ch, rate, codec = struct.unpack_from("<IIII", self.d, self.aud_offs[a])
        return ch, rate, codec

    def playable(self, i):
        """the sound's first track resolves to an audio stream with a codec we decode (what makes a sound 'exist')"""
        s = self.sound(i)
        if not s or not s["tracks"]:
            return None
        a = s["tracks"][0][1]
        au = self.audio(a)
        if not au or au[2] not in CODEC or a not in self.subsong:
            return None
        return s, a, au


class ScdCache:
    def __init__(self, kh2):
        self.kh2, self.c = kh2, {}

    def get(self, rel):
        """bank path below the package ('se/se000.win32.scd') -> (rel path from the extract root, Scd) or (None, None)"""
        if rel not in self.c:
            p = find_scd(self.kh2, rel)
            self.c[rel] = (p, Scd(os.path.join(self.kh2, p))) if p else (None, None)
        return self.c[rel]


def se_key(se):
    """KH2 sound id -> recipe sound key"""
    bank, idx = divmod(int(se), 1000)
    return "se/se%03d.win32.scd#%d" % (bank, idx)


def sound_record(cache, key):
    """recipe key 'se/se705.win32.scd#20' -> the recipe "sounds" record from the SCD, or None if KH2 has no playable sound there.
    Field order = the recipe's (file, rate, channels, volume, [loop], scd, stream, subsong, codec); "loop" is added by the caller."""
    rel, idx = key.split("#")
    idx = int(idx)
    p, scd = cache.get(rel)
    if not scd:
        return None
    hit = scd.playable(idx)
    if not hit:
        return None
    s, a, (ch, rate, codec) = hit
    return {"file": rel[:-len(".win32.scd")] + "/%03d.wav" % idx, "rate": rate, "channels": ch,
            "volume": round(s["volume"], 2), "scd": p, "stream": a, "subsong": scd.subsong[a], "codec": CODEC[codec],
            "_loop_flag": bool(s["attr"] & 1)}


# ------------------------------------------------------------------------------------------------ BAR / PAX
def bar_entries(d):
    """[(type, name, offset, size)] of a BAR file, or None"""
    if d[:4] != b"BAR\x01":
        return None
    n = struct.unpack_from("<I", d, 4)[0]
    out = []
    for i in range(n):
        t, _, nm, off, sz = struct.unpack_from("<HH4sII", d, 16 + 16 * i)
        out.append((t, nm.split(b"\0")[0].decode("latin1"), off, sz))
    return out


def pax_elements(path):
    """the first PAX of a file (BAR entry type 0x12, or the file itself): [(element index, element id, 0x50 bytes)]"""
    d = open(path, "rb").read()
    base = 0
    ents = bar_entries(d)
    if ents is not None:
        pax = [off for t, _, off, _ in ents if t == 0x12]
        if not pax:
            raise ValueError("%s: no PAX entry" % path)
        base = pax[0]
    if d[base:base + 4] != b"PAX_":
        raise ValueError("%s: not a PAX" % path)
    n = struct.unpack_from("<I", d, base + 8)[0]
    out = []
    for i in range(n):
        at = base + 0x10 + 0x50 * i
        e = d[at:at + 0x50]
        out.append((i, struct.unpack_from("<H", e, 2)[0], e))
    return out
