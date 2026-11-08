"""KH2 FM Sora progression tables -> D:/SoraBotW/data/levels/ (offline, read-only on the KH2 files).

Reads 00battle.bin (lvup, bons, plrp, fmlv, lvpm, enmp, btlv) + 03system.bin (item table: ability / item names via
msg/us/sys.bar) from a KH2 FM PC extract (D:/KH2Extract/kh2_first/original by default) and writes:
  kh2_sora_levels.json  every table below, each row tagged with its file offset ("src")
  kh2_sora_levels.h     C arrays for sora.c (level rows, the three Dream Weapon ability columns, bonus rows,
                        lvpm EXP / HP curve)
Usage: python -I tools/kh2_levels.py [--kh2 <dir>] [--out <dir>]
Formats (all little-endian; checked against the data, see recon/S54_HELPER_LV_LEVELS.md section 1):
  BAR    'BAR\\x01', u32 count, 8 pad; entries {u16 type, u16 dup, char name[4], u32 off, u32 size}
  lvup   u32 ver 2, u32 count 14, u32 off[14] (x4 bytes, from the entry start; 0 = none); per character u32 nlevels,
         nlevels x {u32 exp, u8 str, u8 mag, u8 def, u8 ap, u16 sword, u16 shield, u16 staff, u16 pad}
         exp of row L = the total EXP that reaches level L+1; stats / ability = what level L has / teaches.
  bons   u32 ver, u32 count; {u8 reward, u8 chara, u8 hp, u8 mp, u8 drive, u8 item_slot, u8 acc_slot, u8 armor_slot,
         u16 item1, u16 item2, u32 pad}
  plrp   u32 ver, u32 count; 128-byte rows {u16 id, u8 chara, u8 hp, u8 mp, u8 ap, u8 str, u8 mag, u8 def,
         u8 armor_slots, u8 acc_slots, u8 item_slots, u16 objects[58]}
  fmlv   u32 ver, u32 count; {u8 form_level_id (form<<4 | lv), u8 ability_lo, u8 ability_hi?, ...} (PHH's names kept)
  lvpm   no header, 99 rows {u16 hp_mul, u16 str, u16 def, u16 max_str, u16 min_str, u16 exp}  (enemy level params)
  enmp   u32 ver, u32 count; 92-byte rows {u16 id, u16 level, u16 hp[32], u16 max_dmg, u16 min_dmg, u16 weak[7],
         u16 exp, u16 prize, u16 bonus_level}
  item   (03system.bin) u32 ver, u32 count; 24-byte rows {u16 id, u8 type, u8 f0, u8 f1, u8 rank, u16 stat, u16 name,
         u16 desc, u16 buy, u16 sell, u16 cmd, u16 slot, u16 pic, u8 icon1, u8 icon2}
"""
import json, os, struct, sys

KH2 = OUT = None  # (sorabuild) --kh2 <extract>/kh2_first/original --out <dir>, both required
CHARS = ["(none)", "Sora", "Donald", "Goofy", "Mickey", "Auron", "Mulan", "Aladdin", "Jack Sparrow", "Beast",
         "Jack Skellington", "Simba", "Tron", "Riku"]  # lvup character order (OpenKH Lvup)


def bar(b):
    assert b[:4] == b"BAR\x01", "not a BAR"
    n = struct.unpack_from("<I", b, 4)[0]
    out = {}
    for i in range(n):
        t, dup, name, off, size = struct.unpack_from("<HH4sII", b, 16 + 16 * i)
        out.setdefault(name.rstrip(b"\0").decode(errors="replace"), (off, size))
    return out


# KH2 international text: 0x01 space, 0x02 newline, 0x2e.. 'A'-'Z', 0x9a.. 'a'-'z'; digits / punctuation below
# 0x90.. '0'-'9' (all inferred from item names: "High Jump LV1" = ...{91}); unknown bytes come out as {xx}
def kh2_text(b, o):
    s = []
    while b[o] != 0:
        c = b[o]
        if c == 0x01: s.append(" ")
        elif c == 0x02: s.append("\n")
        elif 0x2e <= c <= 0x47: s.append(chr(ord("A") + c - 0x2e))
        elif 0x9a <= c <= 0xb3: s.append(chr(ord("a") + c - 0x9a))
        elif 0x90 <= c <= 0x99: s.append(chr(ord("0") + c - 0x90))
        elif c in PUNCT: s.append(PUNCT[c])
        else: s.append("{%02x}" % c)  # (control codes such as 0x0b <icon> stay raw)
        o += 1
    return "".join(s)


# seen in item names (Hi-Potion, Seifer's Trophy, "The Struggle" Trophy, Orichalcum+, Crime & Punishment); the rest
# stay {xx}
PUNCT = {0x66: "-", 0xec: "'", 0xee: "'", 0xed: '"', 0x2b: "+", 0x86: "&"}


def msgs(sysbar):
    ent = bar(sysbar)
    off, size = ent["sys"]
    m = sysbar[off:off + size]
    ver, n = struct.unpack_from("<II", m, 0)
    table = {}
    for i in range(n):
        mid, moff = struct.unpack_from("<II", m, 8 + 8 * i)
        table[mid] = (moff, off + moff)
    return m, table


def main():
    args = sys.argv[1:]
    kh2, out = KH2, OUT
    while args:
        a = args.pop(0)
        if a == "--kh2": kh2 = args.pop(0)
        elif a == "--out": out = args.pop(0)
        else: sys.exit(__doc__)
    if not (kh2 and out): sys.exit(__doc__)
    battle = open(f"{kh2}/00battle.bin", "rb").read()
    system = open(f"{kh2}/03system.bin", "rb").read()
    sysbar = open(f"{kh2}/msg/us/sys.bar", "rb").read()
    B, S = bar(battle), bar(system)

    # item names (abilities are items: lvup ability fields hold item ids)
    m, mt = msgs(sysbar)
    ioff, isize = S["item"]
    ver, n = struct.unpack_from("<II", system, ioff)
    items = {}
    for i in range(n):
        o = ioff + 8 + 24 * i
        iid, typ, f0, f1, rank, stat, name, desc = struct.unpack_from("<HBBBBHHH", system, o)
        txt = None
        for key in (name, name & 0x7fff, name | 0x8000):  # (the msg ids of sys.bar: try the raw id first)
            if key in mt:
                txt = kh2_text(m, mt[key][0]); break
        items[iid] = {"id": iid, "type": typ, "name": txt, "name_msg": name, "src": f"03system.bin@{o:#x}"}

    def iname(i):
        return items[i]["name"] if i in items else None

    # lvup
    loff, _ = B["lvup"]
    ver, nch = struct.unpack_from("<II", battle, loff)
    offs = struct.unpack_from(f"<{nch}I", battle, loff + 8)
    lvup = {}
    for c in range(nch):
        if not offs[c]: continue
        p = loff + offs[c] * 4
        nl = struct.unpack_from("<I", battle, p)[0]
        rows = []
        for L in range(nl):
            o = p + 4 + 16 * L
            exp, st, mg, df, ap, sw, sh, sf = struct.unpack_from("<IBBBBHHH", battle, o)
            rows.append({"level": L + 1, "exp_next": exp, "str": st, "mag": mg, "def": df, "ap": ap,
                         "sword": sw, "shield": sh, "staff": sf, "src": f"00battle.bin@{o:#x}"})
        lvup[CHARS[c] if c < len(CHARS) else str(c)] = rows
    sora = lvup["Sora"]
    for r in sora:  # names + the EXP that REACHES this level (row L-1's exp_next; level 1 = 0)
        for k in ("sword", "shield", "staff"):
            r[k + "_name"] = iname(r[k]) if r[k] else None
    for i, r in enumerate(sora):
        r["exp_total"] = 0 if i == 0 else sora[i - 1]["exp_next"]

    # bons (Get Bonus rewards)
    boff, _ = B["bons"]
    ver, n = struct.unpack_from("<II", battle, boff)
    bons = []
    for i in range(n):
        o = boff + 8 + 16 * i
        rw, ch, hp, mp, dr, isl, asl, arm, i1, i2 = struct.unpack_from("<8BHH", battle, o)
        if ch != 1: continue
        bons.append({"reward": rw, "hp": hp, "mp": mp, "drive": dr, "item_slot": isl, "acc_slot": asl,
                     "armor_slot": arm, "item1": i1, "item1_name": iname(i1) if i1 else None, "item2": i2,
                     "item2_name": iname(i2) if i2 else None, "src": f"00battle.bin@{o:#x}"})

    # plrp (base stats rows of Sora)
    poff, _ = B["plrp"]
    ver, n = struct.unpack_from("<II", battle, poff)
    plrp = []
    for i in range(n):
        o = poff + 8 + 128 * i
        pid, ch, hp, mp, ap, st, mg, df, arm, acc, isl = struct.unpack_from("<H10B", battle, o)
        if ch != 1: continue
        objs = [x for x in struct.unpack_from("<58H", battle, o + 12) if x]
        plrp.append({"id": pid, "hp": hp, "mp": mp, "ap": ap, "str": st, "mag": mg, "def": df, "armor_slots": arm,
                     "acc_slots": acc, "item_slots": isl, "start_items": objs,
                     "start_item_names": [iname(x) for x in objs], "src": f"00battle.bin@{o:#x}"})

    # lvpm (per enemy level 1..99)
    voff, vsize = B["lvpm"]
    lvpm = []
    for i in range(vsize // 12):
        o = voff + 12 * i
        hp, st, df, mx, mn, ex = struct.unpack_from("<6H", battle, o)
        lvpm.append({"level": i + 1, "hp_mul": hp, "str": st, "def": df, "max_str": mx, "min_str": mn, "exp": ex,
                     "src": f"00battle.bin@{o:#x}"})

    # enmp (enemy params; exp = the EXP column, a percentage of lvpm.exp: see the doc)
    eoff, _ = B["enmp"]
    ver, n = struct.unpack_from("<II", battle, eoff)
    enmp = []
    for i in range(n):
        o = eoff + 8 + 92 * i
        r = struct.unpack_from("<46H", battle, o)
        enmp.append({"id": r[0], "level": r[1], "hp": r[2], "max_dmg": r[34], "min_dmg": r[35],
                     "weak": {"phys": r[36], "fire": r[37], "ice": r[38], "thunder": r[39], "dark": r[40],
                              "light": r[41], "general": r[42]},
                     "exp": r[43], "prize": r[44], "bonus_level": r[45], "src": f"00battle.bin@{o:#x}"})

    # btlv (battle level per world per story progress)
    toff, tsize = B["btlv"]
    ver, n = struct.unpack_from("<II", battle, toff)
    rsz = (tsize - 8) // n if n else 0
    btlv = []
    for i in range(n):
        o = toff + 8 + rsz * i
        idx, prog = struct.unpack_from("<II", battle, o)
        btlv.append({"index": idx, "progress_flag": prog, "world_levels": list(battle[o + 8:o + rsz]),
                     "src": f"00battle.bin@{o:#x}"})

    # fmlv (form levels; Final Form = form 5 levels Glide 1..5? kept raw + PHH-style fields)
    foff, _ = B["fmlv"]
    ver, n = struct.unpack_from("<II", battle, foff)
    fmlv = []
    for i in range(n):
        o = foff + 8 + 8 * i
        fl, unk, ab, exp = struct.unpack_from("<BBHI", battle, o)
        fmlv.append({"form": fl >> 4, "form_level": fl & 0xf, "ability": ab & 0xfff, "ability_level": ab >> 12,
                     "ability_name": iname(ab & 0xfff) if ab & 0xfff else None, "exp": exp,
                     "src": f"00battle.bin@{o:#x}"})

    doc = {
        "_meta": {"tool": "D:/SoraBotW/tools/kh2_levels.py", "kh2_dir": kh2,
                  "files": {"00battle.bin": len(battle), "03system.bin": len(system)},
                  "notes": ["lvup exp_next of level L = total EXP to reach L+1; exp_total = EXP at which L is reached",
                            "sword/shield/staff = the ability learned at that level for the Dream Weapon kept "
                            "(PHH: row +8 + 2 x choice, 0 sword / 1 shield / 2 staff; recon/S54_HELPER_LV_LEVELS.md 1.1)",
                            "HP / MP do not grow with level in KH2: they come from bons (Get Bonus) rows",
                            "enemy EXP (GUESS, see doc): lvpm[level].exp x enmp.exp / 100"]},
        "sora_levels": sora, "sora_bonus": bons, "sora_base": plrp, "lvpm": lvpm, "enmp": enmp, "btlv": btlv,
        "fmlv": fmlv,
        "items_used": {str(k): items[k]["name"] for k in sorted({r[c] for r in sora for c in ("sword", "shield", "staff")
                                                                   if r[c]} | {b["item1"] for b in bons if b["item1"]}
                                                                  | {b["item2"] for b in bons if b["item2"]})
                       if k in items},
    }
    os.makedirs(out, exist_ok=True)
    with open(f"{out}/kh2_sora_levels.json", "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=1, ensure_ascii=False)
    write_h(f"{out}/kh2_sora_levels.h", sora, bons, plrp, lvpm)
    print(f"wrote {out}/kh2_sora_levels.json + .h: {len(sora)} levels, {len(bons)} bonus rows, {len(plrp)} base rows, "
          f"{len(lvpm)} lvpm, {len(enmp)} enmp, {len(btlv)} btlv, {len(fmlv)} fmlv, {len(items)} items")


def cstr(s):
    return "NULL" if s is None else '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def write_h(path, sora, bons, plrp, lvpm):
    L = ["// Generated by D:/SoraBotW/tools/kh2_levels.py from KH2 FM 00battle.bin / 03system.bin. Do not edit.",
         "// KH_LV[L-1]: level L. exp_total = EXP at which L is reached; exp_next = EXP for L+1. Stats are absolute",
         "// (KH2 lvup rows hold Sora's Strength / Magic / Defense / AP at that level). ability[0/1/2] = item id learned",
         "// at L for the Dream Weapon kept: 0 sword, 1 shield, 2 staff (0 = none).",
         "#pragma once", "#include <stdint.h>",
         "typedef struct { uint32_t exp_total, exp_next; uint8_t str, mag, def, ap; uint16_t ability[3]; } KhLevel;",
         "typedef struct { uint8_t reward, hp, mp, drive, item_slot, acc_slot, armor_slot; uint16_t item[2]; } KhBonus;",
         "typedef struct { uint16_t hp_mul, str, def, max_str, min_str, exp; } KhLvpm;",
         f"#define KH_LV_MAX {len(sora)}", f"static const KhLevel KH_LV[{len(sora)}] = {{"]
    for r in sora:
        L.append(f"    {{{r['exp_total']}, {r['exp_next']}, {r['str']}, {r['mag']}, {r['def']}, {r['ap']}, "
                 f"{{{r['sword']}, {r['shield']}, {r['staff']}}}}},  // LV{r['level']}"
                 + (f" {r['sword_name']} / {r['shield_name']} / {r['staff_name']}" if r['sword'] else ""))
    L.append("};")
    L.append(f"#define KH_BONUS_N {len(bons)}")
    L.append(f"static const KhBonus KH_BONUS[{len(bons)}] = {{  // Sora's Get Bonus rows in file order")
    for b in bons:
        nm = " ".join(x for x in (b["item1_name"], b["item2_name"]) if x)
        L.append(f"    {{{b['reward']}, {b['hp']}, {b['mp']}, {b['drive']}, {b['item_slot']}, {b['acc_slot']}, "
                 f"{b['armor_slot']}, {{{b['item1']}, {b['item2']}}}}},{'  // ' + nm if nm else ''}")
    L.append("};")
    base = plrp[0] if plrp else None
    if base:
        L.append(f"#define KH_BASE_HP {base['hp']}  // plrp row id {base['id']} ({base['src']})")
        L.append(f"#define KH_BASE_MP {base['mp']}")
        L.append(f"#define KH_BASE_AP {base['ap']}")
    L.append(f"static const KhLvpm KH_LVPM[{len(lvpm)}] = {{  // enemy level 1..{len(lvpm)}")
    for r in lvpm:
        L.append(f"    {{{r['hp_mul']}, {r['str']}, {r['def']}, {r['max_str']}, {r['min_str']}, {r['exp']}}},"
                 f"  // {r['level']}")
    L.append("};")
    item_ids = sorted({r[c] for r in sora for c in ("sword", "shield", "staff") if r[c]})
    names = {}
    for r in sora:
        for c in ("sword", "shield", "staff"):
            if r[c]: names[r[c]] = r[c + "_name"]
    L.append("static const struct { uint16_t id; const char *name; } KH_LV_ABILITY_NAMES[] = {")
    for i in item_ids:
        L.append(f"    {{{i}, {cstr(names[i])}}},")
    L.append("};")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(L) + "\n")


if __name__ == "__main__":
    main()
