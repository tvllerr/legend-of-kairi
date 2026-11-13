#!/usr/bin/env python3
"""party_sfx_check.py: (s220 riku2) host check of the party sound rows keyed by CHARACTER (runtime/sora_party.h PSFX_SETS /
PSFX_WEAPON) against the data they name. No game, read only. Checks:

  1. Donald's and Goofy's rows (member 0 / 1) are the same as on the base commit (git show <base>:runtime/sora_party.h): their
     sounds come out exactly as before.
  2. Every name a stand-in's row can build is in the NEW sfx.bin (fx/sfxparty2.py output): voice <key>_<set>.<k> for k < n,
     name call-outs when name_w > 0, footsteps <key>_step<f>.<k> for every footstep kind his motion bake has, the weapon /
     body play_sound rows, the marker <key>_sfx_v1. A name missing = a silent moment + one "unknown sound" log line, so: FAIL.
  3. Every trigger of his motion bake (<party dir>/<key>_motion.bin, SKRM) that psfx_fire plays: id 14 (vsb_voice2 vtbl) has a
     row of his (else that line stays silent: listed), id 2 (footstep kind) has its names.
  4. Every PSFX_WEAPON row of his = a play_sound trigger (slot, frame, sound id) of the recipe actor json (the mined KH2 msets),
     and every play_sound trigger of the json is in the table (else: listed).
  5. Every atkp row of his hit bake (<key>_hits.bin, SKRH) has a hit row in the new sfx.bin (his own hit SE, not the base hit).
  6. The OLD sfx.bin (today's data) has no marker: the DLL keeps him silent there (both-way compatibility).

(s223 voices) keys = one or more stand-ins, comma separated (riku,auron,mulan,aladdin,sparrow); the actor json may hold several
actors (found by name). Check 1 then covers every character the BASE table has rows for (Donald, Goofy, and Riku on a base that has
him) unless he is one of the keys. A key whose marker is already in the old pack (Riku in the s220 pack) is checked as KEPT: his
names and hit rows resolve the same in the new pack (instead of 5's "not in the old pack" and 6's "old pack lacks the marker").

usage: python -I party_sfx_check.py <worktree> <base commit> <old sfx.bin> <new sfx.bin> <party dir> <actor json> [keys=riku]
exit 0 = all PASS
"""
import json, re, struct, subprocess, sys

wt, base, old_sfx, new_sfx, pdir, actor_js = sys.argv[1:7]
KEYS = (sys.argv[7] if len(sys.argv) > 7 else "riku").split(",")
PCHS = {"riku": "PCH_RIKU", "auron": "PCH_AURON", "mulan": "PCH_MULAN", "aladdin": "PCH_ALADDIN", "sparrow": "PCH_SPARROW"}
runs = fails = 0


def check(ok, what):
    global runs, fails
    runs += 1
    if not ok:
        fails += 1
        print("  FAIL", what)
    return ok


def tables(src):
    """(PSFX_SETS rows, PSFX_WEAPON rows) as tuples of strings / ints"""
    def body(name):
        m = re.search(r"static const \w+ " + name + r"\[\] = \{(.*?)\n\};", src, re.S)
        return re.sub(r"//[^\n]*", "", m.group(1))
    sets = [(a, int(b), int(c), d, int(e), int(f)) for a, b, c, d, e, f in
            re.findall(r'\{(\w+), (\d+), (\d+), "(\w+)", (\d+), (\d+)\}', body("PSFX_SETS"))]
    weap = [(a, int(b), int(c), d) for a, b, c, d in re.findall(r'\{(\w+), (\d+), (\d+), "(\w+)"\}', body("PSFX_WEAPON"))]
    return sets, weap


def sfx_names(p):
    d = open(p, "rb").read()
    _, _, ns, nt, nts, nv, nc, nn, nh, do, db, _ = struct.unpack_from("<4s11I", d, 0)
    o = 48 + 72 * ns + 28 * nt + 4 * nts + 16 * nv + 8 * nc
    names = {d[o + 36 * i:o + 36 * i + 32].split(b"\0")[0].decode(): struct.unpack_from("<I", d, o + 36 * i + 32)[0] for i in range(nn)}
    o += 36 * nn
    hits = {}
    for i in range(nh):
        r, s = struct.unpack_from("<II", d, o + 8 * i)
        hits.setdefault(r, s)
    keys = [d[48 + 72 * i:48 + 72 * i + 48].split(b"\0")[0].decode() for i in range(ns)]
    return names, hits, keys


cur = open(f"{wt}/runtime/sora_party.h", encoding="utf-8").read()
old = subprocess.run(["git", "-C", wt, "show", f"{base}:runtime/sora_party.h"], capture_output=True, text=True, encoding="utf-8", check=True).stdout
cs, cw = tables(cur)
os_, ow = tables(old)
nn, nh, nkeys = sfx_names(new_sfx)
on, oh, okeys = sfx_names(old_sfx)
FRESH = {k for k in KEYS if f"{k}_sfx_v1" not in on}  # (the keys this new pack brings; a key the old pack has is checked as kept)
keep = [m for m in dict.fromkeys(r[0] for r in os_ + ow) if m not in {PCHS[k] for k in FRESH}]
print(f"1. rows of {keep} vs {base}")
for m in keep:
    check([r for r in cs if r[0] == m] == [r for r in os_ if r[0] == m], f"PSFX_SETS member {m} rows changed")
    check([r for r in cw if r[0] == m] == [r for r in ow if r[0] == m], f"PSFX_WEAPON member {m} rows changed")
print(f"   sets {sum(r[0] in keep for r in cs)} + weapon {sum(r[0] in keep for r in cw)} rows of {keep}: same")
pch = open(f"{wt}/runtime/sora_pchar.h", encoding="utf-8").read()
check(all(n in nn and nn[n] == i for n, i in on.items()), "an old name changed in the new pack")
check(all(r in nh and nkeys[nh[r]] == okeys[s] for r, s in oh.items()), "an old hit row changed in the new pack")
print(f"   old pack {old_sfx}: its {len(on)} names + {len(oh)} hit rows resolve the same in the new pack")
JS = json.load(open(actor_js, encoding="utf-8"))
for KEY in KEYS:
  PCH = PCHS[KEY]
  fresh = KEY in FRESH
  print(f"== {KEY} ({PCH}){'' if fresh else ': already in the old pack (kept)'}")
  check(re.search(r'\{"' + KEY + r'", "\w+", -?\d+, [^,]+, true,', pch) is not None, f"sora_pchar.h {KEY} row: sfx = true")
  my_sets = [r for r in cs if r[0] == PCH]
  my_weap = [r for r in cw if r[0] == PCH]
  print(f"2. {KEY}: {len(my_sets)} voice rows, {len(my_weap)} play_sound rows -> names in {new_sfx}")
  built = set()
  for _, vt, pri, b, n, nw in my_sets:
      check(n > 0 or nw > 0, f"row vtbl {vt} {b}: nothing to play")
      for k in range(n):
          built.add(f"{KEY}_{b}.{k}")
      if nw:
          built.update(f"{KEY}_name_{c}" for c in ("sora", "donald", "goofy"))
  built.update(r[3] for r in my_weap)
  built.add(f"{KEY}_sfx_v1")

  # 3. the motion bake's triggers
  b = open(f"{pdir}/{KEY}_motion.bin", "rb").read()
  check(b[:4] == b"SKRM", "motion bake magic")
  cnt = struct.unpack_from("<I", b, 8)[0]
  vt_used, steps, silent = set(), set(), []
  for k in range(cnt):
      d = 16 + k * 24
      slot, frames = struct.unpack_from("<HH", b, d)
      ft, nft = struct.unpack_from("<IH", b, d + 16)
      for j in range(nft if ft else 0):
          fr, tid, _, p1 = struct.unpack_from("<HBBh", b, ft + j * 8)
          if tid == 14:
              vt_used.add(p1)
              if not any(r[1] == p1 for r in my_sets):
                  silent.append((slot, fr, p1))
          elif tid == 2 and 0 <= p1 <= 10:
              steps.add(p1)
  for f in sorted(steps):
      built.update(f"{KEY}_step{f}.{k}" for k in range(4 if f in (0, 4) else 1))
  print(f"3. motion bake: voice vtbl {sorted(vt_used)}, footstep kinds {sorted(steps)}")
  check(not silent, f"bake voice triggers without a row (slot, frame, vtbl): {silent}")
  missing = sorted(n for n in built if n not in nn)
  check(not missing, f"names missing in the new sfx.bin: {missing}")
  print(f"   {len(built)} names built, {len(built) - len(missing)} in the new sfx.bin")

  # 4. weapon / body play_sound rows = the mined KH2 msets
  a = next(v for v in JS.values() if v.get("name") == KEY)
  js_rows = set()
  for sl, v in a["slots"].items():
      for t in v.get("triggers", []):
          if t["type"] == "play_sound" and t.get("sounds"):
              js_rows.add((int(sl), int(t["frame"]), f"{KEY}_se{int(t['se'])}"))
  tab_rows = {(s, f, nm) for _, s, f, nm in my_weap}
  check(tab_rows == js_rows, f"PSFX_WEAPON vs the KH2 msets: only table {sorted(tab_rows - js_rows)}, only KH2 {sorted(js_rows - tab_rows)}")
  print(f"4. {len(tab_rows)} play_sound rows = the mined msets' {len(js_rows)}")

  # 5. hit rows
  h = open(f"{pdir}/{KEY}_hits.bin", "rb").read()
  check(h[:4] == b"SKRH", "hit bake magic")
  hc = struct.unpack_from("<I", h, 8)[0]
  atkp = set()
  for k in range(hc):
      d = 16 + k * 16
      slot, nwin, nseg = struct.unpack_from("<HHH", h, d)
      off = struct.unpack_from("<I", h, d + 8)[0]
      for w in range(nwin):
          atkp.add(struct.unpack_from("<HHH", h, off + w * 16)[2])  # (PHWin: start, end, atkp, ... 16 bytes)
  own = {r: nkeys[s] for r, s in nh.items() if r in atkp}
  nose = sorted(r for r in atkp if str(r) not in a["hits"])  # (s223: KH2's row has no hit SE (+0x1c < 0): the base hit, as KH2 plays none)
  if nose:
      print(f"   atkp rows without a KH2 hit SE (not packed): {nose}")
  check(atkp and all(r in nh for r in atkp if r not in nose), f"atkp rows without a hit row: {sorted(r for r in atkp if r not in nh and r not in nose)}")
  if fresh:
      check(all(r not in oh for r in atkp), "an atkp row of his was already in the old pack (would keep the old sound)")
  else:
      check(all(r in oh and okeys[oh[r]] == nkeys[nh[r]] for r in atkp if r not in nose), "his hit rows in the old pack changed")
  check(all(k.startswith(f"se/se{a['weapon_bank']}.") for k in own.values()), f"hit sounds not from bank {a['weapon_bank']}: {own}")
  print(f"5. hit bake atkp rows {sorted(atkp)} -> {sorted(set(own.values()))}")

  # 6. the old pack
  check(f"{KEY}_sfx_v1" in nn, "marker: the new pack must have it")
  if fresh:
      check(not any(n.startswith(KEY + "_") for n in on), "the old pack has names of his but no marker")
      print(f"6. old pack: no {KEY}_sfx_v1 and no {KEY}_ name (he stays silent there)")
  else:
      check(all(n in on for n in built), f"kept: names of his missing in the old pack: {sorted(n for n in built if n not in on)}")
      print(f"6. old pack: has {KEY}_sfx_v1 + his {len(built)} names (kept, same index: see 1)")

print(f"party_sfx_check: {runs - fails} / {runs} PASS" + (f", {fails} FAIL" if fails else ""))
sys.exit(1 if fails else 0)
