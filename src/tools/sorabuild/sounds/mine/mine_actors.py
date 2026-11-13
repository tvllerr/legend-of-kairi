#!/usr/bin/env python3
"""Mine the "actors" + "motions" sections of the sound recipe (work/sounds_recipe.json, step sounds_mine) from the player's own KH2 files (no PHH data).

  python -E -P mine_actors.py --kh2 <KH2 extract> --out <fragment.json>

Everything numeric comes from vanilla KH2 files (see the table at the end of this docstring); what is OUR choice (which
KH2 object is which BotW actor, the PHH-style labels the sfx tools key on, ...) is in the CONSTANTS block below, marked.

Where each field comes from (paths relative to the extract; kh2_sixth wins over kh2_first, like trigjson):
  slots[s]            obj/<mset>.mset entry s (a BAR of anb BARs: type 9 motion + type 16 trigger list, kh2bin.triggers)
    .name             the mset entry's 4-char BAR name
    .motion           the entry's motion id (s // 4 in 4-variant player msets (> 300 entries), else s) as its label
    .frames           motion header @0xa0+4 (i32 total frame count)
    .triggers         frame triggers 2 footstep_sound (p0), 6 use_item, 8 play_sound (params [bank, cue]: se =
                      bank * 1000 + cue, bank 500 = the hand's weapon SE bank), 14 vsb_voice2 (vtbl row p0),
                      13 vsb_voice (voice cue p0), plus the weapon msets' play_sound triggers (pass 1, creates the slot
                      record); then 1 effect_caster ("<fx prefix>.<p0>", pass 2, a name-less stub if the slot is new);
                      list sorted by frame (stable)
    .trails           weapon mset range triggers type 12 (pass 3, stubs too), .trail_hands = + weapon index
    .body_hits        own range triggers 10 [atkp, part] / 33 [atkp, part, n] whose atkp row is active (+0x24 = 1):
                      [start, end, atkp, atkp power, part]  (only on existing slots, main actors only)
  vtbl / barks        00battle.bin "vtbl": 14-byte rows (u8 char, u8 row, u8 priority, u8 -, 5 x (i8 vsb, u8 weight));
                      vsb -1 = empty, other negatives kept without a sound, vsb >= 0 kept when the voice SCD cue has
                      audio (kh2bin.scd_cues); a row without choices is left out
  hits[atkp]          00battle.bin "atkp" 48-byte rows (u16 sub, u16 id, +0x06 u16 power, +0x0b u8 hit effect,
                      +0x1c i32 hit SE, +0x24 u8 active, +0x2e u8 kind) of every attack window (own + weapon + nested
                      motions of summons) with a hit SE; SE bank 500 = the hitting weapon's bank
  kb_power / kb_hits  (gen2 actors) the weapon attack windows per slot: [start, end, atkp, power, hand, kind], sorted by
                      start; kb_power = the strongest one
  weapon SE bank      obj/<weapon>.a.us BAR type 31 (Seb) +8 u16 bank
  footsteps           12soundinfo.bar <world> row 0: Sora / Donald / Goofy / other footstep SE banks (u16 +14 .. +22);
                      footstep param n -> cue 16 + n (n = 0 / 4: the 4 walk / run variants 16+n .. 19+n, cycled)
  loops[s]            motion frame data (start, end, return): from = (return - start) / (end - start) when 0 < from < 1
  hurtbox_off[s]      own range triggers type 27 (no hurt box)
  special actors      nested (type 20) mset entries: sub-anb "00<c>0" = party member c's body, "00<c>1" his weapon
  motions             the raw trigger lists of every entry of the two msets chickfx / stitchfx read ("self" = an anb
                      entry, else the nested entry's sub-anbs by name); end = total frames, loop_time = the return
                      point in total-frame units, type = motion header @0x90
"""
import argparse
import json
import os
import struct
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import kh2bin  # noqa: E402

# ------------------------------------------------------------------------------------------------------------------
# OUR CONSTANTS (not KH2 data: which KH2 objects we use, and the names the sfx tools / runtime key on)
# ------------------------------------------------------------------------------------------------------------------
# the KH2 motion id labels (OpenKH MotionSet.MotionName convention; ids 74..128, 252..282 have no label -> "")
MOTION_NAMES = dict(enumerate(
    "IDLE WALK RUN JUMP FALL LAND LINK_IDLE HANG HANG_UP ITEM DAMAGE_S_FRONT_HIGH DAMAGE_S_FRONT_LOW DAMAGE_S_BACK_HIGH "
    "DAMAGE_S_BACK_LOW DAMAGE_AIR_FRONT DAMAGE_AIR_BACK DAMAGE_BLOW_FRONT DAMAGE_BLOW_FRONT_LAND DAMAGE_BLOW_BACK "
    "DAMAGE_BLOW_BACK_LAND DAMAGE_TORNADO_FRONT DAMAGE_TORNADO_BACK DAMAGE_TORNADO DAMAGE_LARGE_FRONT DAMAGE_LARGE_BACK "
    "DAMAGE_RESERVE_3 DAMAGE_RESERVE_4 DAMAGE_RESERVE_5 DAMAGE_RESERVE_6 DAMAGE_RESERVE_7 DAMAGE_01 DAMAGE_02 DAMAGE_03 "
    "DAMAGE_04 DAMAGE_05 DAMAGE_06 DAMAGE_07 DAMAGE_08 DAMAGE_09 DAMAGE_10 DAMAGE_BLOW_RECOV REFLECT AUTOGUARD REFLECT_AIR "
    "APPEAR LEAVE LEAVE_AIR FOOTWORK TURN_L TURN_R TALK TALK_END CHANGEFORM CHANGEEND STUN_LOOP STUN_END MAGIC_FIRE "
    "MAGIC_FIRE_FINISH MAGIC_FIRE_AIR MAGIC_BLIZZARD MAGIC_BLIZZARD_FINISH MAGIC_BLIZZARD_AIR MAGIC_THUNDER "
    "MAGIC_THUNDER_FINISH MAGIC_THUNDER_AIR MAGIC_CURE MAGIC_CURE_FINISH MAGIC_CURE_AIR MAGIC_MAGNET MAGIC_MAGNET_FINISH "
    "MAGIC_MAGNET_AIR MAGIC_REFLECT MAGIC_REFLECT_FINISH MAGIC_REFLECT_AIR".split()))
MOTION_NAMES.update({129: "RTN_TURN", 130: "CALL_SUMMON_END", 131: "DEAD_LAND", 132: "DEAD_AIR", 133: "MEMO_IDLE",
                     134: "SUBMENU_IDLE", 135: "SUBMENU_ACTION1", 136: "SUBMENU_ACTION2", 137: "CALL_FRIEND",
                     138: "CALL_SUMMON", 139: "MENU_IDLE"})
MOTION_NAMES.update({140 + i: "REFLECT%02d" % i for i in range(11)})
MOTION_NAMES.update({151 + i: "EX%03d" % i for i in range(101)})
MOTION_NAMES.update({283 + i: "RTN_%02d" % i for i in range(10)})

FOOTSTEP_WORLD = "hb"     # which 12soundinfo world's footstep banks we use (Hollow Bastion = the BotW look)
FOOTSTEP_APPROX = "A32"   # label kept in every footstep trigger (the old recipe's "approx" tag; nothing reads it)
VOICE_DIR = "voice/us/battle"

# barks = named voice events: BARKS_MOTION (name, motion id) = the vtbl row of that motion's first vsb_voice2 trigger
# (skipped when the actor has none), then the actor's own (name, vtbl row) list (events without an own motion: KO, casts)
BARKS_MOTION = [("damage", 10), ("damage_low", 11), ("blown", 16), ("recover", 40), ("item", 9), ("form", 52),
                ("summon", 138), ("stun_end", 55), ("attack", 151), ("attack_heavy", 154), ("finisher", 165)]
BARKS_SORA_ROWS = [("ko", 2), ("fire_cast", 7), ("magic", 7), ("fire", 11), ("blizzard_cast", 7), ("blizzard", 12),
                   ("thunder_cast", 7), ("thunder", 13), ("cure_cast", 22), ("cure_self", 23), ("magnet_cast", 24),
                   ("reflect_cast", 7), ("reflect", 25)]

# party name call-outs: voice cue <char - 1> of the speaker's battle voice file = the name of party member <char>
PARTY_NAMES = (1, 2, 3)


def W(mset, aus):
    """a weapon: its mset (trails, swing SEs, hits) + the a.us whose Seb names its SE bank"""
    return {"mset": mset, "aus": aus}


KK = "W_EX010"  # Kingdom Key (the keyblade every Sora form swings in the mod)
# The main actors.  id/name = the BotW-side actor table; char = the 00battle vtbl character; mset = the KH2 motion set;
# weapons = (right, left) weapon msets; fx = the effect-name prefix our effects table uses for the object's pax;
# voice = the battle voice file (Hollow Bastion party files, like the footsteps); barks = which bark lists;
# names = party name call-outs; gen2 = the newer hit fields (spark + hand, kb_power / kb_hits)
ACTORS = [
    dict(id="0", name="sora", char=1, mset="P_EX100", weapons=[W(KK, KK)], fx="sora", voice="hb0_sora",
         barks=BARKS_SORA_ROWS, names=True),
    dict(id="1", name="donald", char=2, mset="P_EX020", weapons=[W("W_EX020", "W_EX020")], fx="donald",
         voice="hb0_donald", barks=[("ko", 2), ("spell", 6), ("cure_self", 21), ("cure", 3)], names=True),
    dict(id="2", name="goofy", char=3, mset="P_EX030", weapons=[W("W_EX030", "W_EX030")], fx="goofy",
         voice="hb0_goofy", barks=[("ko", 2)], names=True),
    dict(id="10", name="valor", char=1, mset="P_EX100_BTLF", weapons=[W("W_EX010_BTLF_R", KK), W("W_EX010_BTLF_L", KK)],
         fx="valor", voice="hb0_sora", barks=BARKS_SORA_ROWS, names=True, vtbl=False),
    dict(id="11", name="wisdom", char=1, mset="P_EX100_MAGF", weapons=[W("W_EX010_MAGF", KK)], fx="wisdom",
         voice="hb0_sora", barks=BARKS_SORA_ROWS, names=True, vtbl=False),
    dict(id="12", name="limit", char=1, mset="P_EX100_KH1F", weapons=[W("W_EX010_KH1F", KK)], fx="limit",
         voice="hb0_sora", barks=BARKS_SORA_ROWS, names=True, vtbl=False),
    dict(id="13", name="master", char=1, mset="P_EX100_TRIF", weapons=[W("W_EX010_TRIF_R", KK), W("W_EX010_TRIF_L", KK)],
         fx="master", voice="hb0_sora", barks=BARKS_SORA_ROWS, names=True, vtbl=False),
    dict(id="14", name="final", char=1, mset="P_EX100_ULTF", weapons=[W("W_EX010_ULTF_R", KK), W("W_EX010_ULTF_L", KK)],
         fx="final", voice="hb0_sora", barks=BARKS_SORA_ROWS, names=True, vtbl=False),
    dict(id="15", name="anti", char=1, mset="P_EX100_HTLF", weapons=[], fx="anti", voice=None, barks=None,
         names=False, vtbl=False),
    dict(id="20", name="roxas", char=14, mset="P_EX110_BTLF",
         weapons=[W("W_EX010_RX_BTLF_R", "W_EX010_10"), W("W_EX010_RX_BTLF_L", "W_EX010_20")], fx="roxas",
         voice="tt0_roxas", barks=[("ko", 2)], names=False, gen2=True),
    dict(id="22", name="mickey_coat", char=4, mset="P_EX200", weapons=[W("W_EX200", "W_EX200")], fx="ex200",
         voice="micky", barks=[], names=False, gen2=True),
    dict(id="23", name="mickey_classic", char=4, mset="P_EX220", weapons=[W("W_EX200", "W_EX200")], fx="ex220",
         voice="micky", barks=[], names=False, gen2=True),
    dict(id="50", name="summon_ex330", char=1, mset="P_EX330", weapons=[], fx="ex330", voice="peterpan", barks=None,
         names=False, vtbl=False, nested_hits=True),
    # (51: the old recipe carries no Tinker Bell motion triggers at all -> fx None keeps her slots empty)
    dict(id="51", name="summon_ex340", char=1, mset="P_EX340", weapons=[], fx=None, voice=None, barks=None,
         names=False),
    dict(id="52", name="summon_ex350", char=1, mset="P_EX350", weapons=[], fx="ex350", voice="chicken_little",
         barks=None, names=False, vtbl=False, nested_hits=True),
    dict(id="53", name="summon_al010", char=1, mset="P_AL010", weapons=[], fx="al010", voice="genie", barks=None,
         names=False, vtbl=False, nested_hits=True),
    dict(id="54", name="summon_hb040", char=1, mset="N_HB040_BTL", weapons=[], fx="hb040", voice="stitch", barks=None,
         names=False, vtbl=False, nested_hits=True),
]
# (s220 riku2) the party STAND-INS (sora_pchar.h: a world member in Donald's / Goofy's slot). id = "6" + the KH2 character id
# (2 digits, = 00battle vtbl / lvup character) so more members never collide: Riku 13 -> "613" (Auron 5 -> "605" ...).
# voice = his own battle voice file (the world he fights in: Riku only in The World That Never Was = eh0). Packed by
# fx/sfxparty2.py (names <key>_<set>.<k>, <key>_step<f>.<k>, <key>_swing<n>, his atkp hit rows).
STANDINS = [
    dict(id="613", name="riku", char=13, mset="P_EH000", weapons=[W("W_EH000", "W_EH000")], fx="riku", voice="eh0_riku",
         barks=[("ko", 2)], names=True),
    # (s223 voices) the four world members of s220 members (objentry P_HE000 / P_MU000 / P_AL000 / P_CA000_HUMAN, msets as
    # partybake's: Sparrow's body + weapon msets are P_CA000 / W_CA000, his drawn blade W_CA000_HUMAN); voice = his world's battle file
    dict(id="605", name="auron", char=5, mset="P_HE000", weapons=[W("W_HE000", "W_HE000")], fx="auron", voice="he0_auron",
         barks=[("ko", 2)], names=True),
    dict(id="606", name="mulan", char=6, mset="P_MU000", weapons=[W("W_MU000", "W_MU000")], fx="mulan", voice="mu0_mulan",
         barks=[("ko", 2)], names=True),
    dict(id="607", name="aladdin", char=7, mset="P_AL000", weapons=[W("W_AL000", "W_AL000")], fx="aladdin", voice="al0_aladdin",
         barks=[("ko", 2)], names=True),
    dict(id="608", name="sparrow", char=8, mset="P_CA000", weapons=[W("W_CA000", "W_CA000_HUMAN")], fx="sparrow",
         voice="ca0_sparrow", barks=[("ko", 2)], names=True),
]
ACTORS += STANDINS


# The multi-actor motions Sora plays (nested mset entries: sub-anb "00<c>0" = party member c's body, "00<c>1" = his
# weapon; c = 1 Sora, 2 Donald, 3 Goofy).  Slot numbers / names / which entries = OUR table.
# reaction commands: (slot, name, mset, entry); slot 10 and 13 only add an fx / a hurt-box range in the old recipe
# (their names never show).  13: GUESS - the old recipe has only "no hurt box over the whole motion" for it, which
# many reaction entries give; M_EX890 #254 is one with exactly that (any of them yields the same data).
REACTIONS = [(0, "reversal", "M_EX990", 241), (1, "reversal_b", "M_EX990", 242), (2, "warp_snipe", "M_EX920", 252),
             (3, "warp_snipe_b", "M_EX920", 253), (4, "kickback", "M_EX050", 254), (5, "full_swing", "M_EX050", 255),
             (6, "full_swing_bandit", "M_EX060", 252), (7, "fail_safe", "M_EX900", 254), (8, "snag", "M_EX770", 253),
             (9, "sparkle_ray", "M_EX770", 254), (10, "ex760_254", "M_EX760", 254), (11, "duel_stance", "M_EX910", 252),
             (12, "the_end", "M_EX910", 253), (13, "ex890_254", "M_EX890", 254)]
# party limits: every nested entry of these limit/fm bars that has the member's sub-anb, numbered in this order
LIMIT_BARS = ["donald", "donald2", "goofy", "goofy2", "trinity"]
# summon limits (Sora's side): every nested entry of these summon msets, numbered in this order; short name for the slot
SUMMON_LIMITS = [("P_EX330", "peterpan", "ex330"), ("P_EX340", "tink", "ex340"), ("P_EX350", "chickenlittle", "ex350"),
                 ("P_AL010", "genie", "al010"), ("N_HB040_BTL", "stitch", "hb040")]
# Mickey's rescue captures: the nested entries of P_EX200 (named by motion id)
RESCUE_MSET = "P_EX200"
SPECIALS = [
    dict(id="30", name="reaction_commands", char=1, kind="rc", voice="hb0_sora"),
    dict(id="40", name="limit_1", char=1, kind="limit", voice="hb0_sora"),
    dict(id="41", name="limit_2", char=2, kind="limit", voice="hb0_donald"),
    dict(id="42", name="limit_3", char=3, kind="limit", voice="hb0_goofy"),
    dict(id="55", name="summon_limits", char=1, kind="summon", voice="hb0_sora", kb_power=True),
    dict(id="24", name="rescue_captures", char=1, kind="rescue", voice="hb0_sora", fx="ex200"),
]
# the special actors play as Sora: his keyblade's SE bank (W_EX010 Seb) is their weapon_bank and bank 500

# ------------------------------------------------------------------------------------------------------------------


class Ctx:
    def __init__(self, kh2):
        self.k = kh2bin.Kh2(kh2)
        bt = {n: e for t, n, e in kh2bin.bar(self.k.read("00battle.bin"))}
        a = bt["atkp"]
        self.atkp = {}
        for i in range(struct.unpack_from("<I", a, 4)[0]):
            r = a[8 + 48 * i:56 + 48 * i]
            sub, aid = struct.unpack_from("<HH", r, 0)
            self.atkp.setdefault(aid, {})[sub] = r
        v = bt["vtbl"]
        self.vtbl = {}
        for i in range(struct.unpack_from("<I", v, 4)[0]):
            r = struct.unpack_from("<BBBB10b", v, 8 + 14 * i)
            self.vtbl.setdefault(r[0], {})[r[1]] = (r[2], [(r[4 + 2 * j], r[5 + 2 * j] & 0xff) for j in range(5)])
        self._cues = {}
        self._msets = {}
        si = {n: e for t, n, e in kh2bin.bar(self.k.read("12soundinfo.bar"))}[FOOTSTEP_WORLD]
        r = struct.unpack_from("<12H", si, 4)
        self.foot_bank = {"sora": r[7], "donald": r[8], "goofy": r[9], "other": r[11]}

    def mset(self, name):
        if name not in self._msets:
            ents = kh2bin.mset(self.k.read("obj/%s.mset" % name))
            per4 = len(kh2bin.bar(self.k.read("obj/%s.mset" % name))) > 300  # player msets: 4 entries per motion
            self._msets[name] = ([(i, nm, mo, kh2bin.triggers(tr) if tr else ([], [])) for i, nm, mo, tr in ents], per4)
        return self._msets[name]

    def cues(self, voice):
        if voice not in self._cues:
            self._cues[voice] = kh2bin.scd_cues(self.k.read("%s/%s.win32.scd" % (VOICE_DIR, voice)))
        return self._cues[voice]

    def seb_bank(self, aus):
        for t, n, e in kh2bin.bar(self.k.read("obj/%s.a.us" % aus)):
            if t == 31:
                return struct.unpack_from("<H", e, 8)[0]
        raise SystemExit("no Seb in %s.a.us" % aus)

    def atk(self, aid, sub=0):
        return self.atkp[aid][sub]


def se_key(se):
    return "se/se%03d.win32.scd#%d" % (se // 1000, se % 1000)


def vtbl_row(cx, char, voice, row):
    """{"vtbl", "priority", "choices"} of a 00battle vtbl row, or None if no choice has a sound / negative pick"""
    if row not in cx.vtbl.get(char, {}):
        return None
    pr, ch = cx.vtbl[char][row]
    cues = cx.cues(voice)
    out = []
    for vsb, w in ch:
        if vsb == -1:
            continue
        if vsb < 0:
            out.append({"vsb": vsb, "weight": w})
        elif cues.get(vsb) is not None:
            out.append({"vsb": vsb, "weight": w, "sound": "%s/%s.win32.scd#%d" % (VOICE_DIR, voice, vsb)})
    if not out:
        return None
    return {"vtbl": row, "priority": pr, "choices": out}


def footstep(cx, bank, n):
    if n in (0, 4):
        ids = [16 + n + i for i in range(4)]
    else:
        ids = [16 + n]
    return {"sounds": ["se/se%03d.win32.scd#%d" % (bank, i) for i in ids], "cycle": len(ids) > 1}


def mine_actor(cx, A):
    """a main actor: its own mset + weapon msets"""
    ents, per4 = cx.mset(A["mset"])
    weapons = [(cx.mset(w["mset"])[0], cx.seb_bank(w["aus"])) for w in A["weapons"]]

    def meta(i, nm, mo):
        return nm, MOTION_NAMES.get(i // 4 if per4 else i, ""), kh2bin.motion_info(mo)[0] if mo else 0
    nested = []
    if A.get("nested_hits"):  # the attack windows of the mset's nested (multi-actor) motions count as hits too
        for t, nm, e in kh2bin.bar(cx.k.read("obj/%s.mset" % A["mset"])):
            if t == 20 and e:
                for st, snm, se_ in kh2bin.bar(e):
                    if se_:
                        nested.append(sub_anb(se_)[2][0])
    return build(cx, A, ents, weapons, meta, per4, nested)


def sub_anb(e):
    """one anb BAR -> (motion bytes, trigger bytes, (ranges, frames))"""
    inner = kh2bin.bar(e)
    mo = next((x[2] for x in inner if x[0] == kh2bin.T_MOTION and x[2]), None)
    tr = next((x[2] for x in inner if x[0] == kh2bin.T_TRIGGERS and x[2]), None)
    return mo, tr, (kh2bin.triggers(tr) if tr else ([], []))


def build(cx, A, ents, weapons, meta, per4=False, nested=()):
    """ents = [(slot, name, motion, (ranges, frames))] of the actor's own motions, weapons = [(ents, SE bank)] per hand"""
    char = A["char"]
    foot = cx.foot_bank[{1: "sora", 2: "donald", 3: "goofy"}.get(char, "sora" if char == 14 else "other")]
    wbank = weapons[0][1] if weapons else cx.seb_bank(KK)
    if A.get("bank") is not None:
        wbank = A["bank"]
    hbank = [b for _, b in weapons] or [wbank]
    if A.get("bank") is not None:
        hbank = [A["bank"]] * max(1, len(weapons))

    def se_of(p, hand=0):
        """play_sound params [bank, cue]: bank 500 = the hand's weapon SE bank"""
        bank = hbank[hand] if p[0] == 500 else p[0]
        return bank * 1000 + p[1]

    slots = {}
    own = {i: (i, nm, mo) for i, nm, mo, _ in ents}

    def full(i, nm, mo):
        n, m, f = meta(i, nm, mo)
        return {"name": n, "motion": m, "frames": f, "triggers": []}

    def stub(i):
        if str(i) not in slots:
            slots[str(i)] = {"name": str(i), "motion": "", "frames": 0, "triggers": []}
        return slots[str(i)]

    # pass 1: per slot (ascending), the object's own sound triggers (footsteps, SEs, voices, item use) + the weapons'
    # swing SEs -> a full slot record (a stub when only a weapon has the slot)
    wslot = {}
    for h, (wents, bank) in enumerate(weapons):
        for wi, wnm, wmo, (wr, wf) in wents:
            wslot.setdefault(wi, []).append((h, wf))
    ownf = {i: frm for i, nm, mo, (rng, frm) in ents}
    for i in sorted(set(ownf) | set(wslot)):
        trig = []
        for ty, f, p in ownf.get(i, []):
            if ty == 2:
                fs = footstep(cx, foot, p[0])
                trig.append({"frame": f, "kind": "sfx", "type": "footstep_sound", "footstep": p[0],
                             "sounds": fs["sounds"], "cycle": fs["cycle"], "approx": FOOTSTEP_APPROX})
            elif ty == 6:
                trig.append({"frame": f, "kind": "use_item", "type": "use_item"})
            elif ty == 8:
                se = se_of(p)
                trig.append({"frame": f, "kind": "sfx", "type": "play_sound", "se": se, "sounds": [se_key(se)]})
            elif ty == 14 and A["voice"]:
                r = vtbl_row(cx, char, A["voice"], p[0])
                if r is not None:
                    d = dict(r)
                    d.update(frame=f, kind="voice", type="vsb_voice2")
                    trig.append(d)
            elif ty == 13 and A["voice"]:
                cues = cx.cues(A["voice"])
                if cues.get(p[0]) is not None:
                    trig.append({"vtbl": None, "priority": 0, "choices": [
                        {"vsb": p[0], "weight": 0, "sound": "%s/%s.win32.scd#%d" % (VOICE_DIR, A["voice"], p[0])}],
                        "frame": f, "kind": "voice", "type": "vsb_voice"})
        for h, wf in wslot.get(i, []):
            for ty, f, p in wf:
                if ty == 8:
                    se = se_of(p, h)
                    trig.append({"frame": f, "kind": "sfx", "type": "play_sound", "se": se, "sounds": [se_key(se)]})
        if trig:
            sl = slots[str(i)] = full(*own[i]) if i in own else {"name": str(i), "motion": "", "frames": 0}
            sl["triggers"] = trig
    # pass 3: effect casters
    if A["fx"]:
        for i, nm, mo, (rng, frm) in ents:
            pre = A["fx"][i] if isinstance(A["fx"], dict) else A["fx"]
            trig = [{"frame": f, "fx": "%s.%d" % (pre, p[0]), "type": "effect_caster", "kind": "fx"}
                    for ty, f, p in frm if ty == 1]
            for h, wf in wslot.get(i, []):
                trig += [{"frame": f, "fx": "%s.%d" % (pre, p[0]), "type": "effect_caster", "kind": "fx"}
                         for ty, f, p in wf if ty == 1]
            if trig:
                stub(i)["triggers"].extend(trig)
    # pass 4: weapon trails
    for h, (wents, bank) in enumerate(weapons):
        for wi, wnm, wmo, (wr, wf) in wents:
            tr = [[s, e] for ty, s, e, p in wr if ty == 12]
            if tr:
                sl = stub(wi)
                sl.setdefault("trails", []).extend(tr)
                sl.setdefault("trail_hands", []).extend([s, e, h] for s, e in tr)
    # pass 5: own attack windows (only on slots that exist)
    for i, nm, mo, (rng, frm) in ents:
        body = [[s, e, p[0], struct.unpack_from("<H", cx.atk(p[0]), 6)[0], p[1]]
                for ty, s, e, p in rng if ty in (10, 33) and cx.atk(p[0])[0x24] == 1]
        if body and str(i) in slots and not A.get("special"):
            slots[str(i)]["body_hits"] = body
    for sl in slots.values():
        sl["triggers"].sort(key=lambda t: t["frame"])
    out = {"name": A["name"], "char": char, "weapon_bank": wbank, "slots": slots}
    # barks
    barks = {}
    if A["barks"] is not None:
        for bn, mid in BARKS_MOTION:
            for i, nm, mo, (rng, frm) in ents:
                if (i // 4 if per4 else i) != mid:
                    continue
                v = next((p[0] for ty, f, p in frm if ty == 14), None)
                if v is not None:
                    r = vtbl_row(cx, char, A["voice"], v)
                    if r is not None:
                        barks[bn] = r
                        break
        for bn, row in A["barks"]:
            r = vtbl_row(cx, char, A["voice"], row)
            if r is not None:
                barks[bn] = r
    out["barks"] = barks
    names = {}
    if A["names"]:
        cues = cx.cues(A["voice"])
        for c in PARTY_NAMES:
            if c != char and cues.get(c - 1) is not None:
                names[str(c)] = {"sound": "%s/%s.win32.scd#%d" % (VOICE_DIR, A["voice"], c - 1)}
    out["names"] = names
    vt = {}
    if A.get("special"):
        pass
    elif A.get("vtbl", True) and A["voice"]:
        for row in sorted(cx.vtbl.get(char, {})):
            r = vtbl_row(cx, char, A["voice"], row)
            if r is not None:
                vt[str(row)] = r
    if not A.get("special"):
        out["vtbl"] = vt
    # hits: every atkp row an own / weapon attack window names that has a hit SE (+0x1c)
    hits, hgrp = {}, {}
    src = [(rng, None) for i, nm, mo, (rng, frm) in ents] +           [(wr, h) for h, (wents, bank) in enumerate(weapons) for wi, wnm, wmo, (wr, wf) in wents]
    ids = [(p[0], h) for rng, h in src for ty, s_, e, p in rng if ty in (10, 33)]
    ids += [(p[0], None) for rng in nested for ty, s_, e, p in rng if ty in (10, 33)]
    if A.get("special"):
        ids = []
    for aid, h in ids:
        r = cx.atk(aid)
        se = struct.unpack_from("<i", r, 0x1c)[0]
        if se < 0 or aid in hits:
            continue
        if se // 1000 == 500:
            se = hbank[h or 0] * 1000 + se % 1000
        e = {"se": se, "sub_id": 0, "sounds": [se_key(se)]}
        if A.get("gen2"):
            e["spark"] = ["%s.%d" % (A["weapons"][h or 0]["aus"].lower(), r[0x0b])]
            e["hand"] = h or 0
        hits[aid] = e
        hgrp[aid] = h
    if A.get("gen2"):  # newer layout: grouped by hand (own body, right weapon, left weapon), each ascending
        out["hits"] = {}
        for g in [None] + list(range(len(weapons))):
            for k in sorted(k for k in hits if hgrp[k] == g):
                out["hits"][str(k)] = hits[k]
    else:
        out["hits"] = {str(k): hits[k] for k in sorted(hits)}
    if A.get("gen2") or A.get("kb_power"):
        kbp, kbh = {}, {}
        for h, (wents, bank) in enumerate(weapons):
            for wi, wnm, wmo, (wr, wf) in wents:
                for ty, s_, e, p in wr:
                    if ty in (10, 33) and struct.unpack_from("<i", cx.atk(p[0]), 0x1c)[0] >= 0:
                        kbh.setdefault(wi, []).append([s_, e, p[0], struct.unpack_from("<H", cx.atk(p[0]), 6)[0], h,
                                                       cx.atk(p[0])[0x2e]])
        for wi in sorted(kbh):
            hl = sorted(kbh[wi], key=lambda x: x[0])
            best = max(hl, key=lambda x: x[3])
            kbp[str(wi)] = {"atkp": best[2], "power": best[3]}
            kbh[wi] = {"hits": hl}
        out["kb_power"] = kbp
        if A.get("gen2"):
            out["kb_hits"] = {str(k): kbh[k] for k in sorted(kbh)}
    # hurt box off + loops
    hb, lp = {}, {}
    for i, nm, mo, (rng, frm) in ents:
        r27 = [[s, e] for ty, s, e, p in rng if ty == 27]
        if r27:
            hb[str(i)] = {"ranges": r27}
        if mo:
            tot, fs, fe, fr = kh2bin.motion_info(mo)
            if fe > fs:
                x = (fr - fs) / (fe - fs)
                if 0 < x < 1:
                    lp[str(i)] = {"from": round(x, 6)}
    if hb:
        out["hurtbox_off"] = hb
    if lp:
        out["loops"] = lp
    return out


def nested(cx, rel, mset_in_bar=False):
    """[(entry index, {sub name: anb bytes})] of the nested (type 20) entries of an mset (or of the mset inside a bar)"""
    b = kh2bin.bar(cx.k.read(rel))
    if mset_in_bar:
        b = kh2bin.bar(next(e for t, n, e in b if t == 20))
    out = []
    for i, (t, nm, e) in enumerate(b):
        if t == 20 and e:
            subs = {}
            for st, snm, se_ in kh2bin.bar(e):
                if se_ and snm not in subs:
                    subs[snm] = se_
            out.append((i, subs))
    return out


def mine_special(cx, S):
    c = S["char"]
    own_n, wpn_n = "00%d0" % c, "00%d1" % c
    items = []  # (slot, name, label, frames?, prefix, subs)
    if S["kind"] == "rc":
        for slot, name, ms, ent in REACTIONS:
            subs = dict(nested(cx, "obj/%s.mset" % ms)).get(ent, {})
            items.append((slot, name, "obj/%s.mset#%d" % (ms, ent), False, ms.lower(), subs))
    elif S["kind"] == "limit":
        slot = 0
        for bar_ in LIMIT_BARS:
            for ent, subs in nested(cx, "limit/fm/%s.bar" % bar_, True):
                if own_n in subs:
                    items.append((slot, "%s.%d" % (bar_, ent), "limit/fm/%s.bar#%d" % (bar_, ent), True, "lim_" + bar_,
                                  subs))
                    slot += 1
    elif S["kind"] == "summon":
        slot = 0
        for ms, short, pre in SUMMON_LIMITS:
            for ent, subs in nested(cx, "obj/%s.mset" % ms):
                items.append((slot, "%s.%d" % (short, ent), "obj/%s.mset" % ms, True, pre, subs))
                slot += 1
    elif S["kind"] == "rescue":
        for slot, (ent, subs) in enumerate(nested(cx, "obj/%s.mset" % RESCUE_MSET)):
            items.append((slot, "mickey.%d" % (ent // 4), "obj/%s.mset" % RESCUE_MSET, True, S["fx"], subs))
    ents, wents, meta_of, fx_of = [], [], {}, {}
    for slot, name, label, frames, pre, subs in items:
        if own_n in subs:
            mo, tr, tg = sub_anb(subs[own_n])
            ents.append((slot, name, mo, tg))
            meta_of[slot] = (name, label, kh2bin.motion_info(mo)[0] if (frames and mo) else 0)
        if wpn_n in subs:
            mo, tr, tg = sub_anb(subs[wpn_n])
            wents.append((slot, name, mo, tg))
        fx_of[slot] = pre
    bank = cx.seb_bank(KK)
    A = dict(S, special=True, bank=bank, fx=fx_of, barks=None, names=False)
    return build(cx, A, ents, [(wents, bank)], lambda i, nm, mo: meta_of[i])


# the two mset trigger tables chickfx / stitchfx read (raw triggers of every entry, keyed obj_<name>_mset)
MOTION_TABLES = ["P_EX350", "N_HB040_BTL"]


def anb_entry(e):
    """one anb (BAR of motion + triggers) -> {"ranges", "frames", "end", "loop_time", "type"}"""
    inner = kh2bin.bar(e)
    mo = next((x[2] for x in inner if x[0] == kh2bin.T_MOTION), None)
    tr = next((x[2] for x in inner if x[0] == kh2bin.T_TRIGGERS), None)
    rng, frm = kh2bin.triggers(tr) if tr else ([], [])
    tot, fs, fe, fr = kh2bin.motion_info(mo)
    return {"ranges": [[ty, s_, e_, p] for ty, s_, e_, p in rng], "frames": [[ty, f, p] for ty, f, p in frm],
            "end": tot, "loop_time": (fr - fs) * tot / (fe - fs) if fe > fs else 0.0, "type": struct.unpack_from("<i", mo, 0x90)[0]}


def mine_motions(cx, name):
    slots = {}
    for i, (t, nm, e) in enumerate(kh2bin.bar(cx.k.read("obj/%s.mset" % name))):
        if not e:
            continue
        if t == kh2bin.T_ANB:
            slots[str(i)] = {"self": anb_entry(e)}
        elif t == 20:  # a nested motion set: its non-empty anbs by name
            sub = {}
            for st, snm, se_ in kh2bin.bar(e):
                if se_ and snm not in sub:
                    sub[snm] = anb_entry(se_)
            if sub:
                slots[str(i)] = sub
    return {"slots": slots}


def mine(kh2):
    """-> {"actors": ..., "motions": ...} (the two recipe sections)"""
    cx = Ctx(kh2)
    actors = {}
    for A in ACTORS:
        actors[A["id"]] = mine_actor(cx, A)
    for S in SPECIALS:
        actors[S["id"]] = mine_special(cx, S)
    return {"actors": actors, "motions": {"obj_%s_mset" % m: mine_motions(cx, m) for m in MOTION_TABLES}}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--kh2", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    t0 = time.time()
    frag = mine(a.kh2)
    actors = frag["actors"]
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, "w", encoding="utf-8", newline="\n") as f:
        json.dump(frag, f, indent=0, separators=(",", ":"))
        f.write("\n")
    print("actors: %d, motions: %d -> %s (%.1f s)" % (len(actors), len(frag["motions"]), a.out, time.time() - t0))


if __name__ == "__main__":
    main()
