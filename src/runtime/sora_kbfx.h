// sora_kbfx.h (s89 kbfx): each Keyblade's own KH2 swing ribbon + hit sparks. Notes: recon/S89_HELPER_KBFX.md.
//
// KH2 gives every Keyblade its own effect set, obj/W_EX010_<xx>.a.us (the model's name: 03system.bin "went" row ->
// 00objentry; PHH mined/keyblades.kh.json): element 1 = the combo hit spark, 2 = the finisher spark, 3 = the guard /
// wall spark (as w_ex010.1 / .2 / .3 for the Kingdom Key, recon/S35_HELPER_X_FX.md), element 10 = the swing ribbon
// (texture + blend + colour; KH2 shapes it from the blade's motion). The fx bake (tools_s89/kbfx/fxbake.py, folded into
// tools/fxbake.py at merge) packs them as "kb<xx>_hit" / "kb<xx>_hit_finish" / "kb<xx>_hit_guard" / "kb<xx>_trail";
// the Kingdom Key keeps "hit" / "hit_finish" / "hit_guard" and adds "kb00_trail".
// Who draws what: blade 0 = KB_MAIN (base Sora, Wisdom, Valor's right hand), blade 1 = KB_VALOR (Valor's left hand).
// An id not in KBFX_TAB (a Keyblade added later) or a set missing from fx.bin = the Kingdom Key's set.
// The ribbon's look needs the s89 exe (sora_fx_trail_ex, GetProcAddress); an older exe = the s35 white ribbon, main
// blade only (the hit sparks work with the old exe: they are plain fx.bin elements).
// Live: "set kbfx 0" = the old single set (Kingdom Key sparks + white ribbon, no left-blade ribbon); "set kbfxid <id>"
// = force that Keyblade's set on both blades (e.g. 0x2b Oblivion; 0 = the equipped ones); "set kbfxflip 1" = the
// ribbon texture's v 0 at the grip (GUESS default: at the tip).
// Needs (sora.c before the include): KB (sora_levels.h), sora_fx.h, LOG, windows.h.
#pragma once

typedef struct {
    uint16_t id;       // KH2 item id (03system.bin)
    char set[3];       // W_EX010_<set>.a.us; "" = W_EX010 (the Kingdom Key)
    const char *name;  // log name (sora_kb.h KB_TAB may not have the row yet)
} KbfxRow;
static const KbfxRow KBFX_TAB[] = {
    {0x029, "", "Kingdom Key"},     {0x051, "", "Kingdom Key (item 81)"}, {0x02a, "10", "Oathkeeper"},
    {0x02b, "20", "Oblivion"},      {0x1e0, "30", "Star Seeker"},       {0x1e1, "40", "Hidden Dragon"},
    {0x1e4, "50", "Hero's Crest"},  {0x1e5, "60", "Monochrome"},        {0x1e6, "70", "Follow the Wind"},
    {0x1e7, "80", "Circle of Life"}, {0x1e8, "90", "Photon Debugger"},  {0x1e9, "a0", "Gull Wing"},
    {0x1ea, "b0", "Rumbling Rose"}, {0x1eb, "c0", "Guardian Soul"},     {0x1ec, "d0", "Wishing Lamp"},
    {0x1ed, "e0", "Decisive Pumpkin"}, {0x1ee, "f0", "Sleeping Lion"},  {0x1ef, "g0", "Sweet Memories"},
    {0x1f0, "h0", "Mysterious Abyss"}, {0x1f1, "j0", "Fatal Crest"},    {0x1f2, "k0", "Bond of Flame"},
    {0x1f3, "m0", "Fenrir"},        {0x1f4, "n0", "Ultima Weapon"},     {0x21f, "p0", "Two Become One"},
    {0x220, "r0", "Winner's Proof"}, {0x180, "w0", "Struggle Sword"},   {0x1f5, "v0", "Struggle Wand"},
    {0x1f6, "u0", "Struggle Hammer"}, {0x02c, "y0", "Detection Saber"}, {0x02d, "z0", "Frontier of Ultima"},
};
#define KBFX_N ((int)(sizeof KBFX_TAB / sizeof KBFX_TAB[0]))

static int g_kbfx = 1;     // live "set kbfx": 0 = the old single set
static int g_kbfx_id;      // live "set kbfxid": force this item id's set on both blades (0 = the equipped Keyblades)
static int g_kbfx_vflip;   // live "set kbfxflip": the ribbon texture's v 0 at the grip
static int g_kbfx_blade;   // the blade whose hit is being drawn (blade_hits: 0 main, 1 Valor's left)

static int kbfx_row(int id) {
    for (int i = 0; i < KBFX_N; i++)
        if (KBFX_TAB[i].id == id) return i;
    return -1;
}
static int kbfx_item(int blade) { return g_kbfx_id ? g_kbfx_id : blade ? kb_sub_id(KB.kb, g_fmots_on ? g_fform : 0) : KB.kb[KB_MAIN]; }  // (s91 mfkb: blade 1 = the form's own slot)

// the fx.bin element ids of one row (resolved once; -1 = not in fx.bin -> the Kingdom Key's)
typedef struct { int tried, hit, fin, guard, trail; } KbfxIds;
static KbfxIds g_kbfx_ids[KBFX_N];
static int g_kbfx_pack = -1;  // fx.bin has the per-Keyblade sets (its "kb00_trail"): 1 yes, 0 no, -1 not asked
static const KbfxIds *kbfx_ids(int row) {
    if (row < 0) row = 0;
    if (g_kbfx_pack < 0) {
        if (!sora_fx_ready()) return NULL;  // (fx.bin not loaded yet: ask again later)
        g_kbfx_pack = sora_fx_id("kb00_trail") >= 0;
        if (!g_kbfx_pack) LOG("[sora] kbfx: fx.bin has no per-Keyblade sets (bake tools_s89/kbfx/fxbake.py -> fx_s89_kbfx.bin, "
                              "or the combined rebake): every Keyblade draws the Kingdom Key's sparks + the white ribbon\n");
    }
    KbfxIds *d = &g_kbfx_ids[row];
    if (!d->tried) {
        d->tried = 1, d->hit = d->fin = d->guard = d->trail = -1;
        if (g_kbfx_pack) {
            const char *s = KBFX_TAB[row].set;
            char b[32];
            if (s[0]) {
                snprintf(b, sizeof b, "kb%s_hit", s), d->hit = sora_fx_id(b);
                snprintf(b, sizeof b, "kb%s_hit_finish", s), d->fin = sora_fx_id(b);
                snprintf(b, sizeof b, "kb%s_hit_guard", s), d->guard = sora_fx_id(b);
            }
            snprintf(b, sizeof b, "kb%s_trail", s[0] ? s : "00"), d->trail = sora_fx_id(b);
        }
    }
    return d;
}

// The element name of a hit spark for the blade being drawn (g_kbfx_blade): kind = "hit" / "hit_finish" / "hit_guard"
// -> "kb<xx>_<kind>" when fx.bin has it, else kind itself (the Kingdom Key's).
static const char *kbfx_name(const char *kind) {
    { const char *mk = mk_kbfx_name(kind); if (mk) return mk; }  // (s110m mickeyfx: W_EX200's own sparks)
    if (!g_kbfx || IN_ANTI) return kind;  // (s89 trial: Anti has no Keyblade - its claws keep the plain sparks)
    int row = kbfx_row(kbfx_item(g_kbfx_blade));
    if (row <= 0 || !KBFX_TAB[row].set[0]) return kind;
    const KbfxIds *d = kbfx_ids(row);
    int id = !d ? -1 : !strcmp(kind, "hit") ? d->hit : !strcmp(kind, "hit_finish") ? d->fin : !strcmp(kind, "hit_guard") ? d->guard : -1;
    if (id < 0) return kind;
    static char b[4][32];
    static int k;
    k = (k + 1) & 3;
    snprintf(b[k], sizeof b[k], "kb%s_%s", KBFX_TAB[row].set, kind);
    return b[k];
}

// The swing ribbon of blade `blade` (blade_hits_one, every frame of an attack): the Keyblade's own look on the s89 exe.
static void kbfx_trail(int blade, const float grip[3], const float tip[3], bool on) {
    typedef void (*TrailEx)(int, const float *, const float *, bool, int, int);
    static TrailEx ex;
    static int tried;
    if (!tried) {
        tried = 1, ex = (TrailEx)(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_fx_trail_ex");
        if (!ex) LOG("[sora] kbfx: exe has no sora_fx_trail_ex (before s89): the white ribbon on the main blade only\n");
    }
    if (!ex || !g_kbfx) {
        if (!blade) sora_fx_trail(grip, tip, on);
        return;
    }
    const KbfxIds *d = kbfx_ids(kbfx_row(kbfx_item(blade)));
    ex(blade, grip, tip, on, d ? d->trail : -1, g_kbfx_vflip ? 1 : 0);
}

static void kbfx_log_id(const char *who, int id) {
    int row = kbfx_row(id);
    const KbfxIds *d = kbfx_ids(row);
    const char *s = row < 0 ? "" : KBFX_TAB[row].set;
    bool own = row > 0 && s[0] && d && d->hit >= 0;
    LOG("[sora] kbfx: %s = %s (0x%x)%s -> W_EX010%s%s set: sparks %s, ribbon %s\n", who,
        row < 0 ? kb_name(id) : KBFX_TAB[row].name, id, row < 0 ? " (no kbfx row: the Kingdom Key's)" : "", s[0] ? "_" : "", s,
        own ? "its own" : "the Kingdom Key's", d && d->trail >= 0 ? "its own (element 10)" : "the white gradient");
}

// once per frame (sora_mod_frame_body, after kb_send): logs the active sets when the equipped Keyblades / live keys change
static void kbfx_tick(void) {
    static int last[7] = {-1, -1, -1, -1, -1, -1, -1};
    int now[7] = {KB.kb[KB_MAIN], KB.kb[KB_VALOR], g_kbfx, g_kbfx_id, g_kbfx_vflip, KB.kb[KB_MASTER] * 4 + g_mfkb, KB.kb[KB_FINAL]};  // (s91 mfkb: + Master / Final)
    if (!memcmp(now, last, sizeof now)) return;
    if (g_kbfx_pack < 0 && !sora_fx_ready()) return;
    memcpy(last, now, sizeof now);
    if (!g_kbfx) {
        LOG("[sora] kbfx: off (set kbfx 0): Kingdom Key sparks + the white ribbon for every Keyblade\n");
        return;
    }
    if (g_kbfx_id) LOG("[sora] kbfx: forced to item 0x%x on both blades (set kbfxid 0 = the equipped ones)\n", g_kbfx_id);
    kbfx_log_id("main", g_kbfx_id ? g_kbfx_id : KB.kb[KB_MAIN]);
    kbfx_log_id("valor left", g_kbfx_id ? g_kbfx_id : KB.kb[KB_VALOR]);
    kbfx_log_id("master 2nd", g_kbfx_id ? g_kbfx_id : kb_sub_id(KB.kb, 3));  // (s91 mfkb)
    kbfx_log_id("final 2nd", g_kbfx_id ? g_kbfx_id : kb_sub_id(KB.kb, 4));
    if (g_kbfx_vflip) LOG("[sora] kbfx: ribbon texture flipped (v 0 at the grip)\n");
}

// live "set kbfxid <item id>" (0 = back to the equipped Keyblades)
static void kbfx_live_id(int id) {
    if (id && kbfx_row(id) < 0) LOG("[sora] live: kbfxid 0x%x is not a Keyblade in KBFX_TAB: the Kingdom Key's set\n", id);
    g_kbfx_id = id;
}
