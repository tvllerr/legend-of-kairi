#!/usr/bin/env python3
"""s220 riku2: append the party STAND-INS' KH2 sounds (a world member in Donald's / Goofy's slot, runtime/sora_pchar.h) to an EXISTING
sfx.bin (SKSF v1) without rebuilding it: every older sound index / trigger / vtbl / choice / name / hit row and the old PCM stay
byte-identical (checked at the end), so Donald's, Goofy's and everyone else's sounds come out exactly as before.

Source = the sound recipe's actor of each stand-in (sounds/mine/mine_actors.py STANDINS, id "6" + the KH2 character id: Riku 13 =
"613"), the same json fields fx/sfxpack.py reads for Donald / Goofy (party_names). Names packed (sora_party.h plays them by name,
the same scheme as Donald's):
  <key>_<set>.<k>     the KH2 vtbl row's choices that have a sound, in order; <key>_<set> = choice 0 (VOICE below = which rows)
  <key>_name_<c>      his voice file's name call-outs (cue c - 1 = party member c: sora / donald / goofy)
  <key>_step<f>.<k>   footstep kind f (P_xxx.mset trigger 2; 12soundinfo "other" bank): kinds 0 run / 4 walk cycle 4 variants
  <key>_se<id>        every play_sound trigger of his msets (body + weapon), by KH2 sound id bank * 1000 + cue (e.g. riku_se593034
                      = Way to the Dawn's swing, W_EH000.mset A300 frame 8)
  <key>_sfx_v1        marker (= his first voice line): the DLL turns his sounds on only when the pack has it
Hit rows: his atkp rows (00battle.bin +0x1c hit SE, bank 500 = his weapon's Seb bank) appended to the Hit table when the row is not
there yet, so sora_sfx_hit(atkp) plays his own hit sound (Riku 1494-1498 -> se593 #0 #1 #2 #3 #2) instead of the base Keyblade hit.
Nothing goes into the slot trigger / vtbl tables (those are Sora's).

(s223 voices) + Auron / Mulan / Aladdin / Jack Sparrow (recipe actors 605 / 606 / 607 / 608). Packed ONE STAND-IN AT A TIME (his
names' sounds, then his hit rows' sounds), in STANDINS order, and a stand-in whose marker is in the input already is skipped
whole: so a pack that has Riku (the s220 install) + this tool = the same bytes as the pre-s220 pack + this tool (Riku first, as
before). A stand-in half in the input (some names, no marker) stops the run.

Usage: python -E -P sfxparty2.py <in sfx.bin> <out sfx.bin>
"""
import json, os, struct, sys, wave
import sys as _sys, os as _os; _sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__))); import sndsrc  # sound inputs: recipe tree

# OUR table: stand-in key (= sora_pchar.h key, the name prefix) -> recipe actor id; which vtbl rows become which named set
STANDINS = (("riku", "613"), ("auron", "605"), ("mulan", "606"), ("aladdin", "607"), ("sparrow", "608"))  # (s223: + the four)
_SETS = {"attack": 4, "attack2": 6, "item": 3, "hurt": 29, "hurt_low": 30, "blown": 1, "ko": 2, "stun_end": 10}
VOICE = {  # set name -> 00battle vtbl row (re:0x3CDB60). Riku: VERIFIED from his P_EH000.mset vsb_voice2 triggers (s220)
    "riku": dict(_SETS),
    # (s223) the same 8 rows: their msets' vsb_voice2 triggers name vtbl 4 (A300 / A301 / A330 / A331 frame 8), 29 / 30 (damage),
    # 1 (blow), 10 (stun end), 3 (A110 item), 6 (Auron / Mulan / Aladdin's own act entries 764 / 768); 2 = KO as Riku's (no
    # trigger): VERIFIED by the mined msets (sb/s223/voices py/dump.py). Auron's row 3 has only the -3 partner-name pick: no line.
    "auron": dict(_SETS), "mulan": dict(_SETS), "aladdin": dict(_SETS), "sparrow": dict(_SETS),
}
CHAR = {"1": "sora", "2": "donald", "3": "goofy"}
# (s226 review F1) with the recipe sounds (SB_SNDROOT: every player's build, and the miner's trace) a stand-in that is not in
# the recipe, has no first attack line (= no marker), or lacks a sound STOPS the run: a short pack must never pass as built
# (s225: 5 se banks missing from kh2_files.json gave 58 silent sounds and exit 0). The old PHH cache (dev only) stays lenient.
STRICT = bool(os.environ.get("SB_SNDROOT"))


def load(path):
    d = open(path, "rb").read()
    magic, ver, ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes, _ = struct.unpack_from("<4s11I", d, 0)
    assert magic == b"SKSF" and ver == 1, "not an SKSF v1 sfx.bin"
    return d, (ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes)


def standin_names(js):
    """[(who, [(name, sound key)], [(atkp, key)] hits)] per stand-in in the recipe, in STANDINS order (skips one whose actor
    is not mined)"""
    per = []
    for who, aid in STANDINS:
        act = js["actors"].get(aid)
        if not act:
            if STRICT:
                sys.exit(f"no actor {aid} for {who} in the sound recipe: nothing written")
            print(f"  (no actor {aid} for {who} in the recipe: skipped)", file=sys.stderr)
            continue
        out, hits = [], []
        per.append((who, out, hits))
        for set_name, vt in VOICE[who].items():
            row = act["vtbl"].get(str(vt))
            ks = [c["sound"] for c in (row or {}).get("choices", []) if c.get("sound")]
            for k, key in enumerate(ks):
                out.append((f"{who}_{set_name}.{k}", key))
            if ks:
                out.append((f"{who}_{set_name}", ks[0]))
        for c, v in sorted(act["names"].items(), key=lambda x: int(x[0])):
            out.append((f"{who}_name_{CHAR.get(c, c)}", v["sound"]))
        steps, ses = {}, {}
        for sl in act["slots"].values():
            for t in sl.get("triggers", []):
                if t["type"] == "footstep_sound" and t.get("sounds"):
                    steps.setdefault(int(t["footstep"]), t["sounds"])
                elif t["type"] == "play_sound" and t.get("sounds"):
                    ses.setdefault(int(t["se"]), t["sounds"][0])
        for f in sorted(steps):
            for k, key in enumerate(steps[f]):
                out.append((f"{who}_step{f}.{k}", key))
        for se in sorted(ses):
            out.append((f"{who}_se{se}", ses[se]))
        first = next((k for n, k in out if n == f"{who}_attack.0"), None)
        if first:
            out.append((f"{who}_sfx_v1", first))
        elif STRICT:
            sys.exit(f"{who} has no first attack line ({who}_attack.0) in the sound recipe, so no marker: nothing written")
        for row, h in sorted(((int(r), v) for r, v in act["hits"].items())):
            if h.get("sounds"):
                hits.append((row, h["sounds"][0]))
    return per


def main():
    src, dst = sys.argv[1], sys.argv[2]
    if os.path.abspath(src) == os.path.abspath(dst):
        sys.exit("refusing to overwrite the input")
    d, (ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes) = load(src)
    sj = json.load(open(sndsrc.sounds_json(), encoding="utf-8"))
    sounds_js = sj["sounds"]
    o = 48
    sec_sounds = d[o:o + 72 * ns]; o += 72 * ns
    sec_rest = d[o:o + 28 * nt + 4 * nts + 16 * nv + 8 * nc]; o += 28 * nt + 4 * nts + 16 * nv + 8 * nc
    sec_names = d[o:o + 36 * nn]; o += 36 * nn
    sec_hits = d[o:o + 8 * nh]
    pcm = bytearray(d[data_off:data_off + data_bytes])
    keys = [sec_sounds[i * 72:i * 72 + 48].split(b"\0")[0].decode() for i in range(ns)]
    kidx = {k: i for i, k in enumerate(keys)}
    old_names = {sec_names[i * 36:i * 36 + 32].split(b"\0")[0].decode() for i in range(nn)}
    old_rows = {struct.unpack_from("<I", sec_hits, i * 8)[0] for i in range(nh)}

    per = standin_names(sj)
    todo = []
    for who, names, hits in per:  # (s223: a stand-in already in the input is skipped whole; a half-packed one stops the run)
        if f"{who}_sfx_v1" in old_names:
            have = [n for n, _ in names if n not in old_names]
            if have:
                sys.exit(f"{src} has {who}_sfx_v1 but not {have}: an older pack of his, rebuild from a pack without him")
            print(f"  {who}: already in {src} (marker {who}_sfx_v1): kept as it is")
            continue
        for n, _ in names:
            if n in old_names:
                sys.exit(f"{src} already has {n} but no {who}_sfx_v1: half packed, nothing written")
        todo.append((who, names, hits))
    new_s, new_keys, miss = bytearray(), [], []

    def want(key):
        if key in kidx:
            return kidx[key]
        s = sounds_js.get(key)
        if not s:
            return -1
        path = sndsrc.wavfile(s)
        if not os.path.exists(path):
            return -1
        with wave.open(path, "rb") as w:
            assert w.getsampwidth() == 2, path
            rate, ch, n, fr = w.getframerate(), w.getnchannels(), w.getnframes(), w.readframes(w.getnframes())
        while len(pcm) % 4:
            pcm.append(0)
        new_s.extend(struct.pack("<48sIIfIII", key.encode()[:47], rate, ch, float(s.get("volume", 1.0)), 1 if s.get("loop") else 0, len(pcm), n))
        pcm.extend(fr)
        kidx[key] = ns + len(new_keys)
        new_keys.append(key)
        return kidx[key]

    nrows, hrows = [], []
    for who, names, hits in todo:  # (s223: one stand-in at a time: his names' sounds, then his hits' sounds)
        mine = []
        for n, key in names:
            i = want(key)
            if i < 0:
                miss.append(f"{n}: {key}")
                continue
            mine.append((n, i))
        nrows += mine
        for row, key in hits:
            if row in old_rows or any(r == row for r, _ in hrows):
                print(f"  (hit row {row} is already in the pack: kept the old sound)", file=sys.stderr)
                continue
            i = want(key)
            if i < 0:
                miss.append(f"hit {row}: {key}")
                continue
            hrows.append((row, i))
        if any(n == f"{who}_sfx_v1" for n, _ in names) and not any(n == f"{who}_sfx_v1" for n, _ in mine):
            sys.exit(f"{who}'s marker sound is missing (its wav is not in the sounds tree): nothing written")
    if miss and STRICT:
        sys.exit(f"{len(miss)} stand-in sounds are missing (a KH2 sound bank that is not in the sounds tree?): {miss[:12]}: nothing written")
    ns2 = ns + len(new_keys)
    body = bytearray(sec_sounds + bytes(new_s) + sec_rest + sec_names + b"".join(struct.pack("<32sI", n.encode()[:31], i) for n, i in nrows) +
                     sec_hits + b"".join(struct.pack("<II", r, i) for r, i in hrows))
    while (48 + len(body)) % 16:
        body.append(0)
    hdr = struct.pack("<4s11I", b"SKSF", 1, ns2, nt, nts, nv, nc, nn + len(nrows), nh + len(hrows), 48 + len(body), len(pcm), 0)
    with open(dst, "wb") as f:
        f.write(hdr + body + pcm)
    print(f"{dst}: +{len(new_keys)} sounds, +{len(nrows)} names, +{len(hrows)} hit rows ({[r for r, _ in hrows]}); {len(pcm) / 1e6:.1f} MB PCM16")
    print("missing:", miss or "none")

    # the old rows byte-identical?
    n2, (ns_, nt_, nts_, nv_, nc_, nn_, nh_, do2, db2) = load(dst)
    o2 = 48
    assert n2[o2:o2 + 72 * ns] == sec_sounds
    o2 += 72 * ns_
    assert (nt_, nts_, nv_, nc_) == (nt, nts, nv, nc) and n2[o2:o2 + len(sec_rest)] == sec_rest
    o2 += len(sec_rest)
    assert n2[o2:o2 + 36 * nn] == sec_names
    o2 += 36 * nn_
    assert n2[o2:o2 + 8 * nh] == sec_hits
    assert n2[do2:do2 + data_bytes] == d[data_off:data_off + data_bytes]
    print("VERIFIED: old sounds / triggers / trig sounds / vtbl / choices / names / hit rows and the old PCM are byte-identical")


if __name__ == "__main__":
    main()
