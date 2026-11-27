"""nethorse (s289 objs part 4): the horse a friend rides, seen under his Sora in a Melty room (`set nethorse 1`). This bakes
the puppets the DLL asks the game for (runtime/sora_net_horse.h): one model-only actor per horse a player can ride and per
saddle / reins, from the player's OWN BotW romfs, at install time (nothing from BotW is in the repo or a package).
After Savage Wind PR #294 "Online mode" by byluki (fixes by mstampfli), MIT: tools/online_mod.py's horse part (build,
flat_as, puppet_as, add_info, Rstb), the same steps; only the horses, not its players / gear / enemies / runes.

  python -E -P -B horse_bake.py <romfs dir> <out dir>

Reads (never writes) the romfs:
  Actor/Pack/GameRomHorse*.sbactorpack (the PR's list: GameRomHorse, 00L ... 23, Epona, Bone, Bone_AllDay, Nushi),
    GameROMMotorcycle, GameRomHorseSaddle_* / GameRomHorseReins_*   the horses and their tack
  Actor/Pack/Dm_Npc_Rito_HeroSoul_Kago (else in Pack/TitleBG.pack): a DemoNPC's actor link (an NPC the game draws, no AI)
  Actor/Pack/Dm_Animal_Heron_A: its AI program (one that only stands); Actor/Pack/PauseMenuPlayer: its ActorInfo entry
  Actor/ActorInfo.product.sbyml, System/Resource/ResourceSizeTable.product.srsizetable
Writes <out>/, laid out as a RomFS overlay (horse_install.py puts it into data/overlay; nothing else is touched):
  Actor/Pack/OnlineHorse_<actor>.sbactorpack   a horse: its ModelList, its AS list + each AS as one clip (OE_<file>: the
                                               PR's flat_as, the first clip down each choice's "default"), the DemoNPC's
                                               actor link: no AI of its own, no physics, nothing to ride, hit or pick up.
                                               A saddle / reins: its ModelList alone.
  Actor/ActorInfo.product.sbyml                the romfs' own + one entry per puppet (PauseMenuPlayer's, with the horse's
                                               bfres / model / bounds; profile DemoNPC; traverseDist 1000), by its hash
  System/Resource/ResourceSizeTable.product.srsizetable   the romfs' own + the new files' sizes (an existing size is never
                                               made smaller: a hash already in the table keeps the bigger one)
  horse.txt                                    "GameRomHorse12=OnlineHorse_GameRomHorse12" a line: the DLL's map
  horse_list.txt                               one line per actor: baked or skipped (why)
  horse_manifest.json                          sha256 of each file above (horse_install.py checks them) and of the two romfs
                                               files it was made from (the overlay's copies REPLACE the game's own
                                               ActorInfo / RSTB, so they must come from the romfs the game reads)
Deterministic: sorted names, oead's SARC / BYML / Yaz0 writers (the kit's oead 1.3.0). An <out> that already holds a
horse_manifest.json loses only the files that manifest lists; any other non-empty <out> is refused.
"""
import hashlib
import json
import os
import re
import struct
import sys
import zlib

import oead

PREFIX = "OnlineHorse_"
BASE_NPC = "Dm_Npc_Rito_HeroSoul_Kago"  # (PR BASE_NPC: its actor link)
BASE_LOOK = "PauseMenuPlayer"           # (PR BASE_LOOK: the ActorInfo entry the puppets get, its model list's size)
BASE_AI = "Dm_Animal_Heron_A"           # (PR BASE_AI: an AI program that only stands)
TRAVERSE = 1000.0                       # (PR TRAVERSE: the puppets' traverseDist, as far as the world is around)
HORSE_RE = re.compile(r"(GameRomHorse(\d\d[LS]?|Epona|Bone|Bone_AllDay|Nushi)?|GameROMMotorcycle)\.sbactorpack$")  # (PR)
TACK_RE = re.compile(r"GameRomHorse(Saddle|Reins)_\w+\.sbactorpack$")  # (PR)
INFO = "Actor/ActorInfo.product.sbyml"
RSTB = "System/Resource/ResourceSizeTable.product.srsizetable"
MANIFEST = "horse_manifest.json"
LIST = "horse_list.txt"
MAP = "horse.txt"
NAME_MAX = 47  # (runtime/sora_net_horse.h NhPiece name[48], sora_net_objs_core.h nh_map_parse: < 48 each side)


def crc(s):
    return zlib.crc32(s.encode())


def sha256(b):
    return hashlib.sha256(b).hexdigest()


def pio(data):
    return oead.aamp.ParameterIO.from_binary(bytes(data))


def unyaz(b):
    b = bytes(b)
    return bytes(oead.yaz0.decompress(b)) if b[:4] == b"Yaz0" else b


class Romfs:
    """the player's romfs, read only; Pack/TitleBG.pack opened only when an actor is not loose (147 MB)"""

    def __init__(self, root):
        self.root = root
        self._tbg = None

    def path(self, rel):
        return os.path.join(self.root, *rel.split("/"))

    def read(self, rel):
        with open(self.path(rel), "rb") as f:
            return f.read()

    def actor_pack(self, name):
        p = self.path("Actor/Pack/%s.sbactorpack" % name)
        if os.path.exists(p):
            with open(p, "rb") as f:
                data = f.read()
        else:
            if self._tbg is None:
                self._tbg = oead.Sarc(self.read("Pack/TitleBG.pack"))
            f2 = self._tbg.get_file("Actor/Pack/%s.sbactorpack" % name)
            if f2 is None:
                raise ValueError("%s: in neither Actor/Pack nor Pack/TitleBG.pack" % name)
            data = bytes(f2.data)
        return oead.Sarc(unyaz(data))


class Rstb:
    """System/Resource/ResourceSizeTable (little-endian): crc32 -> size, and the names whose crc32 collide (PR Rstb)"""

    def __init__(self, data):
        data = bytes(data)
        if data[:4] != b"RSTB":
            raise ValueError("not an RSTB")
        ncrc, nname = struct.unpack_from("<II", data, 4)
        o = 12
        self.crc = {}
        for _ in range(ncrc):
            c, s = struct.unpack_from("<II", data, o)
            self.crc[c] = s
            o += 8
        self.names = {}
        for _ in range(nname):
            n = data[o:o + 128].split(b"\0")[0].decode()
            self.names[n] = struct.unpack_from("<I", data, o + 128)[0]
            o += 132
        self.raised = []  # (paths whose hash was already in the table with another size: the bigger kept)

    def get(self, path):
        return self.names.get(path, self.crc.get(crc(path)))

    def set(self, path, size):
        if path in self.names:
            self.names[path] = max(self.names[path], size)
            return
        c = crc(path)
        if c in self.crc and self.crc[c] != size:
            self.raised.append((path, self.crc[c], size))
            size = max(self.crc[c], size)  # (s289: never smaller; the PR set it as is)
        self.crc[c] = size

    def binary(self):
        out = bytearray(b"RSTB" + struct.pack("<II", len(self.crc), len(self.names)))
        for c in sorted(self.crc):
            out += struct.pack("<II", c, self.crc[c])
        for n in sorted(self.names):
            out += n.encode().ljust(128, b"\0") + struct.pack("<I", self.names[n])
        return bytes(out)


def as_name_key(name):
    """(s291 coat) BotW's ASDefine order: its name's bytes, unsigned (the lookup's compare 0x71015002c0)"""
    return str(name).encode("utf-8")


def as_defines(rows):
    """(s291 coat) the AS list's ASDefines from [(Name parameter, file name)], SORTED by name (ASDefine_0.. in that order).
    BotW 1.9.0 finds an AS by a binary search over them (0x71014ffdb4, from the play 0x710149ed3c): an AS list out of order
    loses AS. Every romfs AS list is sorted. The s291 horsefix bake put the 64 Coat_* after the horse's 44 (..Warp_In,
    Coat_D_00..): 52 of 108 names not found, among them every AS the s291e lab sent (Wait_Search, Surprised, Slip, SlipPost,
    GotOff_From_Wild, Swim: "AS played 0"); sorted, all 108 are found (tools test: test_horse_bake.py bsearch)."""
    defs = oead.aamp.ParameterList()
    for name, fn in sorted(rows, key=lambda r: as_name_key(r[0].v)):
        o2 = oead.aamp.ParameterObject()
        o2.params["Name"] = name
        o2.params["Filename"] = oead.aamp.Parameter(oead.FixedSafeString64(fn))
        defs.objects["ASDefine_%d" % len(defs.objects)] = o2
    return defs


def bot_find(names, key):
    """(s291 coat) BotW 1.9.0's AS name lookup 0x71014ffdb4 (decompiled), on the ASDefines' names in file order: its index, -1
    not found (the tests check every baked AS list with it)"""
    def cmp(a, b):
        a, b = as_name_key(a), as_name_key(b)
        return 0 if a == b else (-1 if a < b else 1)
    if not names:
        return -1
    lo, hi = 0, len(names) - 1
    while lo < hi:
        mid = (hi + lo) >> 1
        c = cmp(names[mid], key)
        if c == 0:
            return mid
        if c < 0:
            lo = mid + 1
        else:
            hi = mid
    return lo if cmp(names[lo], key) == 0 else -1


def flat_as(data):
    """(PR flat_as) an AS as one clip: its element tree followed from the first, through each choice's "default" (or its
    first) child, to the first clip (TypeIndex 67); the choices ask the actor's AI, which a puppet has none of. None: no clip
    (s291 coat) when that path ends in no clip (Horse_Link_Wait: its "default" goes to an empty TypeIndex 41 element), the
    other children are tried in their order, depth first: the PR's path first, so every AS it found keeps its clip (same
    bytes); only the AS it found none for (Wait, Wait_Light) now get the first clip down another branch"""
    io = pio(data)
    els = io.lists["Elements"].lists

    def clip(k, depth, seen):
        if depth >= 32 or k in seen:  # (the PR's 32 steps at most)
            return None
        seen.add(k)
        e = els.get("Element%d" % k)
        if e is None:
            return None
        par = e.objects["Parameters"]
        if int(par.params["TypeIndex"].v) == 67 and "FileName" in par.params:
            return par
        ch = e.objects.get("Children")
        if ch is None or not len(ch.params):
            return None
        kids = [int(v.v) for v in ch.params.values()]
        pick = kids[0]
        ext = e.lists.get("Extend")
        sa = ext.lists.get("StringArray") if ext is not None else None
        if sa is not None:
            vals = [str(v.v) for o in sa.objects.values() for v in o.params.values()]
            if "default" in vals and vals.index("default") < len(kids):
                pick = kids[vals.index("default")]
        for kid in [pick] + [x for x in kids if x != pick]:
            got = clip(kid, depth + 1, seen)
            if got is not None:
                return got
        return None

    par = clip(0, 0, set())
    if par is None:
        return None
    out_el = oead.aamp.ParameterList()
    out_el.objects["Parameters"] = par
    out_els = oead.aamp.ParameterList()
    out_els.lists["Element0"] = out_el
    bas = oead.aamp.ParameterIO()
    bas.type = "xml"
    if "CommonParams" in io.objects:
        bas.objects["CommonParams"] = io.objects["CommonParams"]
    bas.lists["Elements"] = out_els
    return bas.to_binary()


# (s291 horsefix) the coat. BotW rolls a horse's coat per horse, not per actor name (1.9.0 code 0x71011a7e0c, static only):
# the Horse plays Collar_Shader / _Coward / _Impatient (its GParam Horse.Nature 0 / 1 / 2) in AS slot 4 at Rate 0 and sets
# the slot's frame to its own int at actor +0xb78 (rolled 0..the AS's frame count once). Those AS play the material anims
# Horse_Link_Collar_<Docile|Touchy|Epona|Zelda>_fsp / _ftp of the horse's model. The model-only puppet played none, so it
# showed Horse_Link's bare material: the dark coat with white socks (s291 lab: under a cream horse). Each horse puppet now
# gets one AS per coat, Coat_<D|T|E|Z>_<frame>: the source Collar AS with its anim clips held at that frame (FrameCtrl Rate 0,
# StartFrame = the frame: GUESS that StartFrame holds a Rate-0 clip there); the DLL (live nethorsecoat) plays the one the
# rider's horse wears in slot 4 (runtime/sora_net_objs_core.h nh_coat_*: the same letters, frames 0..31).
COAT_FAMILY = {"Docile": "D", "Touchy": "T", "Epona": "E", "Zelda": "Z"}  # (Horse_Link_Collar_<x>: the AS clips' FileName)
COAT_ORDER = "DTEZ"   # (= runtime/sora_net_objs_core.h NHC_DOCILE.. NHC_ZELDA)
COAT_FRAMES = 32      # (= NH_COAT_FRAMES: frames 0..31 fit the record's coat byte)
COAT_CLIPS = (62, 83)  # (AS element TypeIndex: the shader-param / texture-pattern clip, as in Horse_Link_Collar_Shader.bas)


def fmaa_frames(bfres):
    """a Switch BFRES (v5, BotW's): its material anims' names -> frame count (FMAA +0x10 name offset, the string a u16 length
    then the bytes; +0x68 the frame count: Horse_Link's Collar_Docile 30, Collar_Epona 0, Foot_fvm 2). {} = not a BFRES"""
    d = unyaz(bfres)
    out = {}
    if d[:4] != b"FRES":
        return out
    i = d.find(b"FMAA")
    while i >= 0:
        if i + 0x6c <= len(d):
            no = struct.unpack_from("<Q", d, i + 0x10)[0]
            if 0 < no < len(d) - 2:
                n = struct.unpack_from("<H", d, no)[0]
                name = d[no + 2:no + 2 + n].decode("ascii", "replace")
                out[name] = struct.unpack_from("<i", d, i + 0x68)[0]
        i = d.find(b"FMAA", i + 4)
    return out


def coat_family(io):
    """an AS's coat family letter: its first shader-param / texture-pattern clip's FileName Horse_Link_Collar_<x>; None: not one"""
    for e in io.lists["Elements"].lists.values():
        par = e.objects["Parameters"]
        if int(par.params["TypeIndex"].v) in COAT_CLIPS and "FileName" in par.params:
            fn = str(par.params["FileName"].v)
            for k, v in COAT_FAMILY.items():
                if fn.endswith("_Collar_" + k):
                    return v, fn
    return None, None


def coat_as(data, frame):
    """the Collar AS `data` with each of its anim clips held at `frame` (FrameCtrl0: Rate 0, StartFrame frame) -> bytes"""
    io = pio(data)
    els = io.lists["Elements"]
    for key in list(els.lists.keys()):
        e = els.lists[key]
        if int(e.objects["Parameters"].params["TypeIndex"].v) not in COAT_CLIPS:
            continue
        ext = e.lists["Extend"] if "Extend" in e.lists else oead.aamp.ParameterList()
        fcl = ext.lists["FrameCtrl"] if "FrameCtrl" in ext.lists else oead.aamp.ParameterList()
        fc = fcl.objects["FrameCtrl0"] if "FrameCtrl0" in fcl.objects else oead.aamp.ParameterObject()
        fc.params["Rate"] = oead.aamp.Parameter(oead.F32(0.0))
        fc.params["StartFrame"] = oead.aamp.Parameter(oead.F32(float(frame)))
        fcl.objects["FrameCtrl0"] = fc
        ext.lists["FrameCtrl"] = fcl
        e.lists["Extend"] = ext
        els.lists[key] = e
    io.lists["Elements"] = els
    return io.to_binary()


class Baker:
    def __init__(self, romfs):
        self.r = romfs
        self.npc = romfs.actor_pack(BASE_NPC)
        self.idle = romfs.actor_pack(BASE_AI)
        self.inputs = {INFO: romfs.read(INFO), RSTB: romfs.read(RSTB)}
        self.rstb = Rstb(unyaz(self.inputs[RSTB]))
        self.sizes = {}
        self.coat_note = {}  # (s291) puppet -> its coat AS, for horse_list.txt
        self._frames = {}    # (s291) model folder -> fmaa_frames of Model/<folder>.sbfres

    def model_frames(self, modellist):
        """(s291) the material anims' frame counts of the model a model list draws (ModelData_0 Base Folder); {} none"""
        try:
            folder = str(pio(modellist).lists["ModelData"].lists["ModelData_0"].objects["Base"].params["Folder"].v)
        except (KeyError, oead.InvalidDataError):
            return {}
        if folder not in self._frames:
            try:
                self._frames[folder] = fmaa_frames(self.r.read("Model/%s.sbfres" % folder))
            except OSError:
                self._frames[folder] = {}
        return self._frames[folder]

    def coat_files(self, pack, asl2, frames):
        """(s291 horsefix) the coat AS of a horse: for each Collar_Shader* AS in its list (the first per family), one AS per
        frame 0..its anims' frame count (at most COAT_FRAMES - 1): [(name, file, bytes, rstb size)] and a note"""
        out, note, seen = [], [], set()
        for o in asl2.lists["ASDefines"].objects.values():
            if not str(o.params["Name"].v).startswith("Collar_Shader"):
                continue
            fn = str(o.params["Filename"].v)
            f = pack.get_file("Actor/AS/%s.bas" % fn)
            if f is None:
                continue
            fam, clip = coat_family(pio(f.data))
            if fam is None or fam in seen:
                continue
            seen.add(fam)
            fc = max(frames.get(clip + "_fsp", -1), frames.get(clip + "_ftp", -1))
            if fc < 0:
                note.append("%s: no %s_fsp / _ftp in the model" % (fam, clip))
                continue
            last = min(fc, COAT_FRAMES - 1)
            size = max(self.rstb.get("Actor/AS/%s.bas" % fn) or 0, self.rstb.get("Actor/AS/Player_Wait.bas") or 0)
            for fr in range(last + 1):
                nm = "Coat_%s_%02d" % (fam, fr)
                out.append((nm, "OE_" + nm, coat_as(f.data, fr), size))
            note.append("%s 0-%d (%s%s)" % (fam, last, clip, ", %d frames: cut to %d" % (fc, last) if fc > last else ""))
        out.sort(key=lambda x: (COAT_ORDER.index(x[0][5]), x[0]))
        return out, ", ".join(sorted(note, key=lambda n: COAT_ORDER.index(n[0]) if n[0] in COAT_ORDER else 9))

    def puppet_as(self, pack, asl_file, online, frames=None):
        """(PR puppet_as) the puppet's AS list (named as the puppet; the horse's animations kept) and AS: each of the
        horse's AS as one clip, OE_<its file>; (s291) with `frames` (its model's anims) also its coat AS, Coat_<f>_<nn>.
        None: no AS with a clip"""
        asl2 = pio(asl_file.data)
        files = {}
        rows = []  # (s291 coat) (Name parameter, Filename): written sorted, see as_defines
        for o in asl2.lists["ASDefines"].objects.values():
            fn = str(o.params["Filename"].v)
            f = pack.get_file("Actor/AS/%s.bas" % fn)
            flat = flat_as(f.data) if f else None
            if flat is None:
                continue
            rows.append((o.params["Name"], "OE_" + fn))
            files["Actor/AS/OE_%s.bas" % fn] = flat
            self.sizes["Actor/AS/OE_%s.bas" % fn] = self.rstb.get("Actor/AS/Player_Wait.bas")
        if not files:
            return None
        if frames is not None:  # (s291 horsefix: the coat AS)
            coat, note = self.coat_files(pack, asl2, frames)
            for nm, fn, data, size in coat:
                rows.append((oead.aamp.Parameter(oead.FixedSafeString32(nm)), fn))
                files["Actor/AS/%s.bas" % fn] = data
                self.sizes["Actor/AS/%s.bas" % fn] = size
            self.coat_note[online] = "%d coat AS: %s" % (len(coat), note) if coat else "no coat AS (%s)" % (note or "no Collar_Shader AS")
        asl2.lists["ASDefines"] = as_defines(rows)
        order = [str(o.params["Name"].v) for o in asl2.lists["ASDefines"].objects.values()]
        lost = [n for n in order if bot_find(order, n) < 0]
        if lost:  # (never once sorted: a guard against a later change)
            raise ValueError("AS list: BotW would not find %s" % ", ".join(lost[:5]))
        files["Actor/ASList/%s.baslist" % online] = asl2.to_binary()
        self.sizes["Actor/ASList/%s.baslist" % online] = max(self.rstb.get("Actor/ASList/%s" % asl_file.name.split("/")[-1]) or 0, 0x10000)
        return files

    def build(self, name, modellist, as_files=None, users=None):
        """(PR build) an actor pack: the DemoNPC's actor link and an AI that only stands, the model list given, the AS
        list and AS when given (an actor finds them in its own pack: without them its load never ends); -> yaz0 bytes"""
        files = {}
        link = pio(self.npc.get_file("Actor/ActorLink/%s.bxml" % BASE_NPC).data)
        lt = link.objects["LinkTarget"]
        for key, value in (("ModelUser", name), ("ASUser", "Dummy"), ("ElinkUser", "Dummy"), ("SlinkUser", "Dummy")):
            lt.params[key] = oead.aamp.Parameter(value)
        for key, value in (users or {}).items():
            lt.params[key] = oead.aamp.Parameter(value)
        for f in self.idle.get_files():  # (a DemoNPC that only stands: root DummyAction, a demo heron's; PR)
            if f.name.startswith("Actor/AIProgram/"):
                files[f.name] = bytes(f.data)
                lt.params["AIProgramUser"] = oead.aamp.Parameter(f.name.split("/")[-1].rsplit(".", 1)[0])
                self.sizes[f.name] = self.rstb.get(f.name)
        files["Actor/ActorLink/%s.bxml" % name] = link.to_binary()
        self.sizes["Actor/ActorLink/%s.bxml" % name] = self.rstb.get("Actor/ActorLink/%s.bxml" % BASE_NPC)
        files["Actor/ModelList/%s.bmodellist" % name] = modellist
        self.sizes["Actor/ModelList/%s.bmodellist" % name] = max(self.rstb.get("Actor/ModelList/%s.bmodellist" % BASE_LOOK) or 0, 0x4000)
        files.update(as_files or {})
        w = oead.SarcWriter(oead.Endianness.Little)
        for p2 in sorted(files):
            w.files[p2] = files[p2]
        pack = bytes(w.write()[1])
        self.sizes["Actor/Pack/%s.bactorpack" % name] = int(len(pack) * 1.5) + 0x1000
        return bytes(oead.yaz0.compress(pack))

    def link_target(self, pack):
        al = [f for f in pack.get_files() if f.name.startswith("Actor/ActorLink/")]
        if not al:
            raise ValueError("no actor link")
        return pio(al[0].data).objects["LinkTarget"]

    def horse(self, piece):
        a = self.r.actor_pack(piece)
        lt = self.link_target(a)
        user, as_user = str(lt.params["ModelUser"].v), str(lt.params["ASUser"].v)
        ml = a.get_file("Actor/ModelList/%s.bmodellist" % user)
        asl = a.get_file("Actor/ASList/%s.baslist" % as_user)
        if not ml or not asl:
            raise ValueError("no model list (%s) / AS list (%s)" % (user, as_user))
        online = PREFIX + piece
        has = self.puppet_as(a, asl, online, frames=self.model_frames(ml.data))
        if not has:
            raise ValueError("no AS with a clip")
        n_as = sum(1 for p2 in has if p2.startswith("Actor/AS/OE_") and not p2.startswith("Actor/AS/OE_Coat_"))
        return online, self.build(online, bytes(ml.data), as_files=has, users={"ASUser": online}), n_as

    def tack(self, piece):
        a = self.r.actor_pack(piece)
        user = str(self.link_target(a).params["ModelUser"].v)
        ml = a.get_file("Actor/ModelList/%s.bmodellist" % user)
        if not ml:
            raise ValueError("no model list (%s)" % user)
        online = PREFIX + piece
        return online, self.build(online, bytes(ml.data)), 0

    def info(self, made):
        """(PR add_info) ActorInfo: PauseMenuPlayer's entry for each puppet, a DemoNPC, with its horse's model and bounds"""
        info = oead.byml.from_binary(unyaz(self.inputs[INFO]))
        actors, hashes = info["Actors"], info["Hashes"]
        by_name = {str(a["name"]): a for a in actors}
        npc_info = by_name[BASE_NPC]
        added, kept = [], []
        for online, piece in made:
            src = by_name.get(piece)
            if src is None:
                kept.append((online, "no ActorInfo entry for %s" % piece))
                continue
            entry = oead.byml.Hash()
            for k2, v in by_name[BASE_LOOK].items():
                entry[k2] = v
            for k2 in ("bfres", "mainModel", "aabbMin", "aabbMax", "boundingForTraverse"):
                if k2 in src:
                    entry[k2] = src[k2]
                elif k2 in ("bfres", "mainModel") and k2 in entry:
                    del entry[k2]
            entry["name"] = online
            entry["profile"] = "DemoNPC"
            entry["elink"] = "Dummy"
            entry["slink"] = "Dummy"
            entry["instSize"] = npc_info["instSize"]
            entry["traverseDist"] = oead.F32(TRAVERSE)
            if "tags" in npc_info:
                entry["tags"] = npc_info["tags"]
            h = crc(online)
            keys = [int(x) & 0xffffffff for x in hashes]
            if h in keys:  # (already there: the romfs' own entry stands)
                kept.append((online, "ActorInfo already has %#x" % h))
                continue
            i = 0
            while i < len(keys) and keys[i] < h:
                i += 1
            hashes.insert(i, oead.S32(h) if h < 0x80000000 else oead.U32(h))
            actors.insert(i, entry)
            added.append(online)
        info_bin = bytes(oead.byml.to_binary(info, big_endian=False, version=2))
        self.sizes["Actor/ActorInfo.product.byml"] = max(self.rstb.get("Actor/ActorInfo.product.byml") or 0, len(info_bin) * 2)
        return bytes(oead.yaz0.compress(info_bin)), added, kept

    def rstb_out(self):
        for p in sorted(self.sizes):
            s = self.sizes[p]
            self.rstb.set(p, int(s) if s else 0x10000)
        return bytes(oead.yaz0.compress(self.rstb.binary()))


def name_ok(n):
    return 0 < len(n) <= NAME_MAX and all(c.isascii() and (c.isalnum() or c == "_") for c in n)


def write_atomic(path, data):
    """a bake killed half way leaves the old file or none, never a short one (gear_bake.py's)"""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "wb") as f:
        f.write(data)
    os.replace(tmp, path)


def clear_out(out):
    """<out> empty / missing, or an earlier bake's (its manifest's files only go); else refused"""
    if not os.path.isdir(out):
        return
    left = os.listdir(out)
    if not left:
        return
    mp = os.path.join(out, MANIFEST)
    if not os.path.isfile(mp):
        raise SystemExit("horse_bake: %s is not empty and holds no %s: not a bake folder of ours, refused" % (out, MANIFEST))
    with open(mp, "r", encoding="utf-8") as f:
        old = json.load(f)
    for rel in list(old.get("files", {})) + [LIST, MANIFEST]:
        if rel.startswith(("/", "\\")) or ".." in rel.replace("\\", "/").split("/") or ":" in rel:
            raise SystemExit("horse_bake: %s lists %r: refused" % (mp, rel))
        p = os.path.join(out, *rel.split("/"))
        if os.path.isfile(p):
            os.remove(p)


def main(argv):
    args = [a for a in argv if not a.startswith("--")]
    if len(args) != 2:
        raise SystemExit(__doc__)
    root, out = (os.path.abspath(a) for a in args)
    for rel in (INFO, RSTB, "Actor/Pack/%s.sbactorpack" % BASE_AI, "Actor/Pack/%s.sbactorpack" % BASE_LOOK):
        if not os.path.isfile(os.path.join(root, *rel.split("/"))):
            raise SystemExit("horse_bake: %s: no %s (not a BotW romfs)" % (root, rel))
    try:
        inside = os.path.normcase(os.path.commonpath([root, out])) == os.path.normcase(root)
    except ValueError:  # (s289 review3 #8: on another drive: not inside)
        inside = False
    if inside:
        raise SystemExit("horse_bake: the out folder is inside the romfs: refused (the romfs is read only)")
    clear_out(out)
    r = Romfs(root)
    b = Baker(r)
    names = sorted(os.listdir(os.path.join(root, "Actor", "Pack")))
    horses = [n[:-len(".sbactorpack")] for n in names if HORSE_RE.match(n)]
    tack = [n[:-len(".sbactorpack")] for n in names if TACK_RE.match(n)]
    lines, packs, made = [], {}, []
    for piece, fn in [(p, b.horse) for p in horses] + [(p, b.tack) for p in tack]:
        if not name_ok(PREFIX + piece):
            lines.append("%s skipped: its puppet's name is longer than %d / not letters, digits, _" % (piece, NAME_MAX))
            continue
        try:
            online, data, n_as = fn(piece)
        except Exception as e:  # (oead's errors are no OSError / ValueError: one odd actor is skipped and listed)
            lines.append("%s skipped: %s: %s" % (piece, type(e).__name__, e))
            continue
        packs["Actor/Pack/%s.sbactorpack" % online] = data
        made.append((online, piece))
        lines.append("%s baked: %s, %d bytes%s%s" % (piece, online, len(data), ", %d AS" % n_as if n_as else " (model only)",
                                                   "; " + b.coat_note[online] if online in b.coat_note else ""))
    if not made:
        raise SystemExit("horse_bake: no horse baked:\n" + "\n".join(lines))
    info, added, kept = b.info(made)
    for online, why in kept:
        lines.append("%s: no ActorInfo entry added (%s)" % (online, why))
    if not added:
        raise SystemExit("horse_bake: no ActorInfo entry added:\n" + "\n".join(lines))
    ok = set(added)
    for online, piece in made:  # (a puppet with no entry of ours: not in horse.txt, its pack not written)
        if online not in ok:
            packs.pop("Actor/Pack/%s.sbactorpack" % online, None)
    rstb = b.rstb_out()
    for path, was, now in b.rstb.raised:
        lines.append("RSTB: %s's hash was in the table (size %d): kept the bigger of it and %d" % (path, was, now))
    files = dict(packs)
    files[INFO] = info
    files[RSTB] = rstb
    files[MAP] = "".join("%s=%s\n" % (piece, online) for online, piece in sorted(made, key=lambda x: x[1]) if online in ok).encode("ascii")
    for rel in sorted(files):
        write_atomic(os.path.join(out, *rel.split("/")), files[rel])
    write_atomic(os.path.join(out, LIST), ("\n".join(lines) + "\n").encode("utf-8"))
    man = {"tool": "tools/sorabuild/nethorse/horse_bake.py (s289 objs part 4, s291 coat AS; after Savage Wind PR #294 by byluki, MIT)",
           "romfs_inputs": {rel: sha256(data) for rel, data in sorted(b.inputs.items())},
           "files": {rel: sha256(files[rel]) for rel in sorted(files)}}
    write_atomic(os.path.join(out, MANIFEST), (json.dumps(man, indent=1, sort_keys=True) + "\n").encode("utf-8"))
    print("horse_bake: %d puppets (%d horses, %d saddles / reins) into %s; %d skipped (see %s)" % (
        len(ok), sum(1 for o, p in made if o in ok and not TACK_RE.match(p + ".sbactorpack")),
        sum(1 for o, p in made if o in ok and TACK_RE.match(p + ".sbactorpack")), out,
        sum(1 for ln in lines if " skipped: " in ln), LIST))


if __name__ == "__main__":
    main(sys.argv[1:])
