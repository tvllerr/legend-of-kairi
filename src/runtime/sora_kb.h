// sora_kb.h: Sora's Keyblades, the KH2 pause menu Equipment page model (slice 5, plan
// docs/plans/2026-10-30-kh2-pause-menu-slice5-equipment.md). Pure, host-tested (runtime/test/kb_test.c).
// Two slots, as vanilla KH2 FM (recon/S85_EQUIP_PAGE.md): KB_MAIN = the main Keyblade (base Sora, Wisdom, Valor's right
// hand) and KB_VALOR = Valor's sub-weapon (left hand). One copy per Keyblade: the two slots never hold the same one
// (vanilla takes an equipped Keyblade out of stock; GUESS from the stock calls, not traced in game).
// Ids = KH2 item ids (03system.bin). alt = the sorabake alt id (group attr bits 24..31) = KB_TAB index + 1.
// Sidecar pack: main u16 | valor u16 << 16 (the default packs to 0x01e00029).
// Adding a Keyblade later = a KB_TAB row + camp_text keys + a sorabake SORABAKE_KB_ALT entry (no exe link).
// (s89 kb) every KH2 FM Sora Keyblade (24) + (s214) our own two (rows 25, 26). VERIFIED bytes, read with tools_s89/kb/kb_items.py from
// kh2_first/original/03system.bin: 'item' table @0x1087c (24-byte entries: id u16 +0, type u8 +2 (2 = Sora weapon),
// went index u8 +4, stat row u16 +6, name / desc sys.bar ids +8 / +10) and the stat table right after it @0x13aac
// (16-byte rows: id u16, ability u16, Attack u8, Magic u8). Model = 'went' @0x8f74 Sora set (+0x118) [went index] ->
// 00objentry.bin -> W_EX010 / _10 / _20 / _30 .. _N0 / _P0 / _R0 (comment per row). Rows are append-only: the index
// is the bake's alt id - 1 and a bit of the owned mask (sidecar "kb_owned").
#pragma once
#include <stdint.h>

// (s91 mfkb) + KB_MASTER / KB_FINAL: Master and Final Form's own 2nd Keyblade (vanilla KH2: one Equipment row per Drive Form
// with a sub-weapon slot, recon/S85_EQUIP_PAGE.md kind 6). 0 in those two = "auto" (no own Keyblade yet: the form uses
// Valor's slot, the pre-s91 look). g_mfkb 0 (live "set mfkb 0") = Master / Final always use Valor's slot.
enum { KB_MAIN, KB_VALOR, KB_MASTER, KB_FINAL, KB_SLOTS };
static int g_mfkb = 1;
static int g_eqlook = 1;  // (s120) 1 = the vanilla KH2 Equipment look (sora_camp_equip.h), live "set eqlook 0" = the old page
static int g_eq2x = 97, g_eq2y = 58, g_eqtx = 51, g_eqvx = 22, g_eqky = 0, g_eqpx = 75, g_eqpy = 55, g_eqax = 0, g_eqhy = 334, g_eqhx = 141, g_eqpt = 86, g_eqpc = -53;  // (s120b) Equipment panel text tuning (sora_camp_equip.h)
// the slot of the 2nd blade in a Drive Form (sora.c form ids: 3 Master, 4 Final; anything else = Valor's), -1 form = none
static int kb_sub_slot(int form) { return g_mfkb && form == 3 ? KB_MASTER : g_mfkb && form == 4 ? KB_FINAL : KB_VALOR; }
// the form slot's Keyblade id (auto 0 -> Valor's)
static int kb_sub_id(const int kb[KB_SLOTS], int form) { int s = kb_sub_slot(form); return kb[s] ? kb[s] : kb[KB_VALOR]; }
// the Keyblade a slot SHOWS (the Equipment page row): auto (0) Master / Final = Valor's
static int kb_slot_id(const int kb[KB_SLOTS], int slot) { return slot >= 0 && slot < KB_SLOTS && kb[slot] ? kb[slot] : kb[KB_VALOR]; }
#define KB_KINGDOM_KEY 0x29
#define KB_STAR_SEEKER 0x1e0

typedef struct {
    uint16_t id;
    const char *name;   // log name
    int str, mag;       // 03system.bin stat row Attack / Magic
    int ability;        // the Keyblade's ability (on while equipped, no AP)
} KbRow;

// camp_text keys: name = KB_TEXT + 2 * index, desc = + 1 for the first KB_TEXT_N1 rows (166..173: key 174 is taken);
// rows from KB_TEXT_N1 on at KB_TEXT2 + 2 * (index - KB_TEXT_N1) (s89 kb: 400..439); (s214) rows from KB_TEXT_N2 on at
// KB_TEXT3 + 2 * (index - KB_TEXT_N2) (450.. : 440..443 are the Master / Final rows). Use kb_text_key().
#define KB_TEXT 166
#define KB_TEXT_N1 4
#define KB_TEXT2 400
#define KB_TEXT_N2 24
#define KB_TEXT3 450
static const KbRow KB_TAB[] = {
    {KB_KINGDOM_KEY, "Kingdom Key", 3, 1, 542},  // Damage Control (no effect yet)       W_EX010     stat 0x50
    {KB_STAR_SEEKER, "Star Seeker", 3, 1, 163},  // Air Combo Plus                      W_EX010_30  stat 0x7b
    {0x2a, "Oathkeeper", 3, 3, 398},             // Form Boost (no effect yet)          W_EX010_10  stat 0x51
    {0x2b, "Oblivion", 6, 2, 397},               // Drive Boost (no effect yet)         W_EX010_20  stat 0x52
    // (s89 kb) the rest, item table order (abilities menu-only: no effect yet, user s89 decision)
    {0x1e1, "Hidden Dragon", 2, 2, 412},         // MP Rage                             W_EX010_40  stat 0x7c
    {0x1e4, "Hero's Crest", 4, 0, 391},          // Air Combo Boost                     W_EX010_50  stat 0x7f
    {0x1e5, "Monochrome", 3, 2, 411},            // Item Boost                          W_EX010_60  stat 0x80
    {0x1e6, "Follow the Wind", 3, 1, 405},       // Draw                                W_EX010_70  stat 0x81
    {0x1e7, "Circle of Life", 4, 1, 413},        // MP Haste                            W_EX010_80  stat 0x82
    {0x1e8, "Photon Debugger", 3, 2, 410},       // Thunder Boost                       W_EX010_90  stat 0x83
    {0x1e9, "Gull Wing", 2, 3, 401},             // Experience Boost                    W_EX010_A0  stat 0x84
    {0x1ea, "Rumbling Rose", 5, 0, 393},         // Finishing Plus                      W_EX010_B0  stat 0x85
    {0x1eb, "Guardian Soul", 5, 1, 392},         // Reaction Boost                      W_EX010_C0  stat 0x86
    {0x1ec, "Wishing Lamp", 4, 3, 406},          // Jackpot                             W_EX010_D0  stat 0x87
    {0x1ed, "Decisive Pumpkin", 6, 1, 390},      // Combo Boost                         W_EX010_E0  stat 0x88
    {0x1ee, "Sleeping Lion", 5, 3, 162},         // Combo Plus                          W_EX010_F0  stat 0x89
    {0x1ef, "Sweet Memories", 0, 4, 540},        // Drive Converter                     W_EX010_G0  stat 0x8a
    {0x1f0, "Mysterious Abyss", 3, 3, 409},      // Blizzard Boost                      W_EX010_H0  stat 0x8b
    {0x1f1, "Fatal Crest", 3, 5, 395},           // Berserk Charge                      W_EX010_J0  stat 0x8c
    {0x1f2, "Bond of Flame", 4, 4, 408},         // Fire Boost                          W_EX010_K0  stat 0x8d
    {0x1f3, "Fenrir", 7, 1, 394},                // Negative Combo                      W_EX010_M0  stat 0x8e
    {0x1f4, "Ultima Weapon", 6, 4, 422},         // MP Hastega                          W_EX010_N0  stat 0x8f
    {0x21f, "Two Become One", 5, 4, 541},        // Light & Darkness (FM)               W_EX010_P0  stat 0x94
    {0x220, "Winner's Proof", 5, 7, 404},        // No Experience (FM)                  W_EX010_R0  stat 0x95
    // (s214 kb) OUR OWN Keyblades (BotW motifs, modelled by us; not KH2 items, no KH2 data): ids above every 03system.bin item
    // id (1..0x259, 535 rows) and away from the 0x777 / 0x999 test values; model = tools/sorabuild/ours/kb/<name>.skkb (+ .png).
    // Stats GUESS (no vanilla values exist: close to the mid-game Keyblades), ability none yet (0 = no Ability line).
    {0x7e01, "Sealing Blade", 5, 2, 0},          // (none yet) GUESS                    ours sealing_blade.skkb
    {0x7e02, "Guardian Core", 3, 3, 0},          // (none yet) GUESS                    ours guardian_core.skkb
};
#define KB_N ((int)(sizeof KB_TAB / sizeof KB_TAB[0]))

static int kb_index(int id) {
    for (int i = 0; i < KB_N; i++)
        if (KB_TAB[i].id == id) return i;
    return -1;
}
static int kb_alt(int id) { return kb_index(id) + 1; }
static int kb_text_key(int i) {  // + 1 = desc
    return i < KB_TEXT_N1 ? KB_TEXT + 2 * i : i < KB_TEXT_N2 ? KB_TEXT2 + 2 * (i - KB_TEXT_N1) : KB_TEXT3 + 2 * (i - KB_TEXT_N2);
}

// (s89 kb) the owned set: bit i = KB_TAB[i] (the sidecar "kb_owned"). Kingdom Key + Star Seeker are always owned (the
// start blade and the default Valor sub-weapon; KH2 gives Star Seeker at Yen Sid's tower, before the first world).
_Static_assert(sizeof KB_TAB / sizeof KB_TAB[0] <= 32, "the owned mask is a u32");
#define KB_OWN_ALWAYS 0x3u
static uint32_t kb_own_bit(int id) { int i = kb_index(id); return i < 0 ? 0 : 1u << i; }
// the owned mask as the page / picker sees it: the always ones + the equipped ones (never lose an equipped blade)
static uint32_t kb_own_fix(uint32_t own, const int kb[KB_SLOTS]) {
    uint32_t all = KB_N >= 32 ? 0xffffffffu : (1u << KB_N) - 1;
    for (int s = 0; s < KB_SLOTS; s++) own |= kb_own_bit(kb[s]);
    return (own | KB_OWN_ALWAYS) & all;
}
static int kb_owned(uint32_t own, int id) { return (own & kb_own_bit(id)) != 0; }
// the picker list: KB_TAB indices of the owned ones, table order -> count
static int kb_own_list(uint32_t own, int out[]) {
    int n = 0;
    for (int i = 0; i < KB_N; i++)
        if (own >> i & 1) out[n++] = i;
    return n;
}  // 0 = not a Keyblade (the bake's default draws)
static const char *kb_name(int id) { int i = kb_index(id); return i < 0 ? "?" : KB_TAB[i].name; }
static int kb_str(int id) { int i = kb_index(id); return i < 0 ? KB_TAB[0].str : KB_TAB[i].str; }
static int kb_mag(int id) { int i = kb_index(id); return i < 0 ? KB_TAB[0].mag : KB_TAB[i].mag; }
static int kb_ability(int id) { int i = kb_index(id); return i < 0 ? 0 : KB_TAB[i].ability; }

static void kb_default(int kb[KB_SLOTS]) { kb[KB_MAIN] = KB_KINGDOM_KEY, kb[KB_VALOR] = KB_STAR_SEEKER, kb[KB_MASTER] = kb[KB_FINAL] = 0; }

static uint32_t kb_pack(const int kb[KB_SLOTS]) { return (uint32_t)(kb[KB_MAIN] & 0xFFFF) | (uint32_t)(kb[KB_VALOR] & 0xFFFF) << 16; }
// (s91 mfkb) Master | Final << 16: the sidecar "keyblade2" key (missing in old saves = kb_form_defaults)
static uint32_t kb_pack2(const int kb[KB_SLOTS]) { return (uint32_t)(kb[KB_MASTER] & 0xFFFF) | (uint32_t)(kb[KB_FINAL] & 0xFFFF) << 16; }

// returns the slots that were set back to the default (an id not in KB_TAB, or the one-copy rule broken by a hand edit:
// the Valor slot gives way; if its default is the main one, the first other Keyblade)
static int kb_unpack(uint32_t v, int kb[KB_SLOTS]) {
    int d[KB_SLOTS], bad = 0;
    kb_default(d);
    kb[KB_MAIN] = (int)(v & 0xFFFF), kb[KB_VALOR] = (int)(v >> 16), kb[KB_MASTER] = kb[KB_FINAL] = 0;
    for (int s = 0; s < KB_VALOR + 1; s++)
        if (kb_index(kb[s]) < 0) kb[s] = d[s], bad++;
    if (kb[KB_VALOR] == kb[KB_MAIN]) {
        bad++;
        kb[KB_VALOR] = d[KB_VALOR] != kb[KB_MAIN] ? d[KB_VALOR] : d[KB_MAIN];
    }
    return bad;
}

// the levels sidecar "keyblade" key: missing (old saves / a new slot) -> the default
static int kb_load(int found, uint32_t v, int kb[KB_SLOTS]) {
    if (!found) { kb_default(kb); return 0; }
    return kb_unpack(v, kb);
}

// 1 changed, 0 already so, -1 refused (bad slot / not a Keyblade / held by the other slot: one copy)
static int kb_set(int kb[KB_SLOTS], int slot, int id) {
    if (slot < 0 || slot >= KB_SLOTS || kb_index(id) < 0) return -1;
    if (kb[slot] == id) return 0;
    for (int s = 0; s < KB_SLOTS; s++)
        if (s != slot && kb[s] == id) return -1;
    kb[slot] = id;
    return 1;
}

// a picker entry the slot can take (the other slot's Keyblade is out of stock, greyed as KH2's disabled entries)
static int kb_offer(const int kb[KB_SLOTS], int slot, int id) {
    if (kb_index(id) < 0) return 0;
    for (int s = 0; s < KB_SLOTS; s++)
        if (s != slot && kb[s] == id) return 0;
    return 1;
}

// (s91 mfkb) the Master / Final slots from the sidecar: found = the "keyblade2" key was there. A bad id or one held by an
// earlier slot (hand edit) = 0 (auto). Returns the slots set back to auto. Then kb_form_defaults (owned set known).
static int kb_unpack2(uint32_t v, int kb[KB_SLOTS]) {
    int bad = 0;
    kb[KB_MASTER] = (int)(v & 0xFFFF), kb[KB_FINAL] = (int)(v >> 16);
    for (int s = KB_MASTER; s < KB_SLOTS; s++) {
        if (!kb[s]) continue;
        int dup = kb_index(kb[s]) < 0;
        for (int o = 0; o < s; o++) dup |= kb[o] == kb[s];
        if (dup) kb[s] = 0, bad++;
    }
    return bad;
}
// KH2-faithful default GUESS: Master = Oathkeeper, Final = Oblivion (the form Keyblades KH2 hands out with the Divine Beast
// milestones, sora_unlock.h 2 / 3 beasts) when OWNED and not already equipped; else the first owned Keyblade nobody holds
// (table order); else stays 0 (auto: Valor's slot, the old look). Only fills 0s, so a pick is never replaced. -> slots filled
// (s118) forms = the unlocked mask (bit 0 Master, bit 1 Final): a locked form's slot is never filled (its row is hidden, so
// a Keyblade parked there could not be picked anywhere: user s118 "4 keyblades but it wont let me select any of them").
static int kb_form_defaults_m(int kb[KB_SLOTS], uint32_t own, int forms) {
    static const int pref[2] = {0x2a, 0x2b};
    int n = 0;
    for (int s = KB_MASTER; s < KB_SLOTS; s++) {
        if (kb[s] || !(forms >> (s - KB_MASTER) & 1)) continue;
        int pick = 0;
        if (kb_offer(kb, s, pref[s - KB_MASTER]) && kb_owned(own, pref[s - KB_MASTER])) pick = pref[s - KB_MASTER];
        for (int i = 0; !pick && i < KB_N; i++)
            if (kb_owned(own, KB_TAB[i].id) && kb_offer(kb, s, KB_TAB[i].id)) pick = KB_TAB[i].id;
        if (pick) kb[s] = pick, n++;
    }
    return n;
}
static int kb_form_defaults(int kb[KB_SLOTS], uint32_t own) { return kb_form_defaults_m(kb, own, 3); }
// (s118) a locked form's slot gives its Keyblade back (0 = auto) -> slots cleared
static int kb_lock_forms(int kb[KB_SLOTS], int forms) {
    int n = 0;
    for (int s = KB_MASTER; s < KB_SLOTS; s++)
        if (kb[s] && !(forms >> (s - KB_MASTER) & 1)) kb[s] = 0, n++;
    return n;
}
// (s118) the other slot that holds id (-1 none). Picking it swaps: this slot takes id, that slot takes this slot's Keyblade
// (GUESS, not vanilla: KH2 greys a Keyblade another slot holds). Live "set kbswap 0" = the old error SE.
static int g_kbswap = 1;
static int kb_holder(const int kb[KB_SLOTS], int slot, int id) {
    for (int s = 0; s < KB_SLOTS; s++)
        if (s != slot && kb[s] == id) return s;
    return -1;
}
// 1 swapped, -1 refused (no holder / this slot is auto (0) / kbswap off)
static int kb_swap(int kb[KB_SLOTS], int slot, int id) {
    int o = kb_holder(kb, slot, id);
    if (!g_kbswap || slot < 0 || slot >= KB_SLOTS || o < 0 || !kb[slot]) return -1;
    kb[o] = kb[slot], kb[slot] = id;
    return 1;
}
