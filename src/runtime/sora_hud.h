// Sora-in-BotW: KH2's HP / MP gauge and command menu on screen (runtime/sora_hud.c).
// Recon: D:/SoraBotW/recon/SORA_HUD_RECON.md (gauge), S32_HELPER_L_CMDMENU.md + S34_HELPER_M_MENU_DRAW.md (menu)
#pragma once
#include <stdbool.h>
#include <stdint.h>

// Game thread (sora.c, any time): the values to show. hp_max <= 0 hides the gauge, mp_max <= 0 hides the MP bar.
// KH2 units (Sora: HP 20..120, MP 100..140); the lengths of the bars follow hp_max / mp_max like KH2's do.
// mp < 0 = MP Charge (KH2: MP ran out, the pink flashing gauge refills): -mp = the charge so far (0..mp_max).
void sora_hud_set(float hp, float hp_max, float mp, float mp_max);
void sora_hud_off(void);  // hide everything: the gauge and the menu (paused, no Sora, ...)

// (s57 party) KH2's party gauges (dk::GAUGE_FRIEND; recon/S57_HELPER_HUDPARTY.md): one small plate per member above
// Sora's ring (KH2's own art: es0field 'gaug' anim groups 64-74, the members' 'face' sequences, data/hud/party.bin =
// hudbake `party`): HP = the left half of the ring (green), MP = the right half (blue), each filling up from 6 o'clock;
// the dark red track of lost HP, the low-HP pulse (HP <= max / 4), the hit flash, the KO face. member 0 = Donald (the
// plate right above Sora's ring), 1 = Goofy (the one above); when member 0 is not shown, member 1 slides down into its
// place, as in KH2. Any units (only hp / hp_max and mp / mp_max are drawn). mp < 0 = MP Charge, -mp = the charge so
// far (drawn as the fill: GUESS). Game thread, every frame with sora_hud_set; drawn only while Sora's gauge is up; a
// member not updated for 30 presents counts as not shown. The mod DLL looks it up by name (GetProcAddress): exes
// linked before s57 lack it.
void sora_hud_party(int member, float hp, float hp_max, float mp, float mp_max, bool shown);
// (s57 party) where Sora's ring centre sits, HUD units (game image height / 448) from the image's bottom-right; the party
// plates and the MP bar move with it. <= 0 = that axis back to SAVAGEWIND_SORA_HUD_POS / the default (60, 177: the ring
// just above BotW's minimap; 60, 66 = the pre-s57 place, on the minimap). Any thread (live "set hudx / hudy").
void sora_hud_place(float x, float y);
void sora_hud_face(int rox);  // (s111 roxhud) 1 = KH2's Roxas portrait on the HUD (roxface.bin), 0 = Sora's
void sora_hud_face_id(int who);  // (s116 mkface) 0 Sora, 1 Roxas (roxface.bin), 2 Mickey (mkface.bin)
// (s139 sumplate) the summon's orb plate (KH2 anim groups 154 / 155 / 156 in drive.bin) + its face portrait (data/hud/sumface.bin): 0 none (fades out),
// 1 Chicken Little, 2 Stitch, 3 Genie, 4 Peter Pan. Game thread, on change; the mod DLL looks it up by name (exes before s139 lack it).
void sora_hud_summon_face(int id);
// (s67 drive) KH2's Drive gauge beside Sora's orb (data/hud/drive.bin = hudbake `drive`): state 0 hidden / 1 shown,
// bars 0..max_bars full bars, fill 0..100 the next one (100 with bars = max_bars = full). Game thread, every frame with
// sora_hud_set (sora_drive.h dg_hud, by GetProcAddress); 30 presents without a call = hidden; drawn only while Sora's
// gauge is up.
void sora_hud_drive(int state, int bars, int fill, int max_bars);

// KH2's enemy HP gauge (top right, 'gaug' sprite groups 63-71): the locked target's life and max life (BotW units),
// life_max <= 0 hides it. The bar's length follows life_max up to one full bar; more life = extra bars (the pips under
// it). Game thread, every frame; sora_hud_off hides it too.
void sora_hud_enemy(float life, float life_max);
// (s92 hud) the enemy gauge at the image's top-left (where BotW's hearts were): x = the bar's left end in HUD units from the left
// edge, y = its centre line from the top; on = false = the old place (top-right). Any thread. The DLL looks it up by name.
void sora_hud_enemy_place(float x, float y, bool on);
// (s71 drive) KH2's Drive screen dim: a black full-screen quad under the HUD at alpha a (0..1). Game thread, every frame
// while dimmed; 3 presents without a call = off. The DLL looks it up by name (exes before s71 lack it).
void sora_hud_dim(float a);

// The command menu (KH2's, drawn from data/hud/cmd.bin = tools/hudbake `cmd`). Game thread, every frame after
// sora_hud_set (sora_hud_off clears it too); copied under the same lock as the gauge values. NULL or on = false hides it.
// Labels: 0 Attack 1 Magic 2 Items 3 Drive 4 Fire 5 Blizzard 6 Thunder 7 Cure (the bake's label atlas rows).
// States (KH2's entry states -> text colour): 0 normal (grey 0xB4), 2 can't afford (orange), 5 disabled (dark grey);
// the selected row uses the brighter variants. Other values draw as 0.
typedef struct {
    bool on, shortcut;                 // shortcut: the L-held shortcut page replaces the top page
    int cursor, sub, sub_cursor;       // top page cursor 0..3; sub: -1 none, 1 = the Magic page open (its cursor)
    uint8_t top_state[4];              // Attack / Magic / Items / Drive
    int n_sub;                         // the Magic page's rows (0..8)
    uint8_t sub_label[8], sub_state[8];
    int16_t sub_cost[8];               // MP costs (not drawn in this slice: KH2 shows no cost text, only state 2)
    int sc_label[4];                   // the shortcut rows' labels (-1 = empty), rows = anim groups 67 / 69 / 68 / 66
    uint8_t sc_state[4];
} SoraHudMenu;
void sora_hud_menu(const SoraHudMenu *m);
// (s68 drive) the top page's label for <slot> 0..3 (cmd.bin label id; default = the slot): 9 Revert in a Drive Form;
// sub = 2 draws the Drive page like the Magic page (no title). Looked up by name by the DLL.
void sora_hud_menu_top(int slot, int label);
// (s111 roxcmd) which top rows are drawn: bit i = slot i (Attack Magic Items Drive), packed; Roxas = 0b0101. Default 0xF. Looked up by name by the DLL.
void sora_hud_menu_rows(int mask);
// (s119 mkmenu) KH2's Mickey command pane instead of the top / Magic / shortcut pages (recon/S119_HELPER_MKMENU.md): n rows (0 = off, max 4),
// top to bottom, <lab> = cmd.bin label ids (28 Jump, 29 Pearl, 0 Attack), <btn> = the button icon's label ids (30 / 31 / 32 PlayStation Circle /
// Square / Cross, 33 / 34 / 35 Xbox B / X / A), <st> = entry states (0 / 2 / 5). Resend every frame while it shows (it hides itself 3 presents
// after the last call). A cmd.bin without labels 28..35 keeps the normal menu. Looked up by name by the DLL (exes before s119 lack it).
// Returns 1 when the pane draws, 0 when the normal menu stays (n 0, cmd.bin not loaded yet or without labels 28..35): the DLL then
// keeps Sora's menu input.
int sora_hud_menu_pad(int n, const int8_t *lab, const int8_t *btn, const uint8_t *st);

// (s54 dream) KH2's Dream Weapon picker (recon/S54_HELPER_DREAM.md): a message box (KH2's own Station of Awakening
// lines) over a command-menu-style list of choices, drawn instead of the command menu while on. Labels = ids into
// data/hud/dream.bin (tools/hudbake `dream`, from data/dream/dream_labels.txt = tools/kh2_dream_text.py; the ids are
// SORA_DREAM_L_* in sora_dream.h). Game thread, every frame it shows (it hides by itself 3 presents after the last
// call, and with sora_hud_off). The mod DLL looks this up by name (GetProcAddress): exes linked before s54 lack it.
typedef struct {
    bool on;
    int n_text;
    int16_t text[4];  // the message lines, top to bottom (centred, on a dark box)
    int n_rows;
    int16_t row[4];   // the choices, top to bottom (KH2 command-menu slots; 0 rows = the message alone)
    int cursor;       // the selected row
} SoraHudDream;
void sora_hud_dream(const SoraHudDream *d);

// (s55 HUD) KH2's level-up panel (dk::LEVEL_UP; recon/S55_HELPER_HUD.md): the red "LEVEL UP! LV. n  Sora" panel at the
// top right with up to two of KH2's lines, KH2's own art (es0field 'gaug' anim group 104 one line / 105 two lines) and
// text (sys.bar), from data/hud/lvup.bin (hudbake `lvup`). One call = one panel (KH2: one per level gained); queued
// like KH2's LEVELUP_INFO (16 waiting at most, up to 4 shown at once stacked, each 200 frames at 60 Hz = 3.3 s). Game
// thread; NULL = drop the queue and the panels on screen. The mod DLL looks it up by name (GetProcAddress): exes
// linked before s55 lack it. Lines: KH2's order (re:0x3ED7D0) = the stats that grew, then the level's ability.
#define SORA_LVUP_STR 0x101          // "Strength Increased!"      (sys.bar 0xade)
#define SORA_LVUP_MAG 0x102          // "Magic Increased!"         (0xadf)
#define SORA_LVUP_DEF 0x103          // "Defense Increased!"       (0xae0)
#define SORA_LVUP_AP 0x104           // "Maximum AP Increased!"    (0xae1)
#define SORA_LVUP_ABILITY(item) (0x10000u + (uint32_t)(item))  // "<icon><name>!" (0xae8 + the KH2 item's name / icon)
#define SORA_LVUP_NAME_SORA 0u                                    // (s76) the name slot: "Sora" (sys.bar 0x51f)
#define SORA_LVUP_NAME_FORM(form) (0x200u + (uint32_t)(form))     // "Valor" / "Wisdom" (sys.bar 485 / 486, first word)
typedef struct {
    int level;         // 1..99 (the "LV." number)
    int n_lines;       // 0..2 (0 = the panel with the level only: not a KH2 use)
    uint32_t line[2];  // SORA_LVUP_* keys, top to bottom (a key missing from lvup.bin = that line left out, logged)
    uint32_t name;     // (s76) SORA_LVUP_NAME_*: whose level (0 = Sora; a key missing from lvup.bin = Sora, logged)
} SoraHudLevelUp;
void sora_hud_levelup(const SoraHudLevelUp *u);

// The KH2 lock-on reticle (data/hud/lock.bin = tools/hudbake `lock`) on the locked target. Game thread, every frame:
// on + the target's centre (world, BotW units, the space of Link's matrix). Projected with sora_draw's camera of the
// frame; it flies in on lock, loops while held, plays KH2's release animation once when on goes false.
void sora_hud_lock(bool on, const float world[3]);  // (= sora_hud_target(on ? 2 : 0, world))
// The two KH2 lock-on icons (lock.bin v2): kind 1 = the yellow AUTO target mark (where a swing will go), 2 = the
// blue-white R1 LOCK (inner arcs turning), 0 = none. Switching kinds hides the old icon while the new one flies in.
void sora_hud_target(int kind, const float world[3]);

// (s78 camp) The KH2 pause menu's quads (the DLL's sora_camp_*.h build them from camp_seq.bin; the art = camp.bin, see
// sora_hud.c's knobs). Same struct as sora_camp_seq.h's (whichever is included first defines it).
#ifndef SORA_HUD_QUAD_DEFINED
#define SORA_HUD_QUAD_DEFINED
typedef struct {
    float x[4], y[4];   // virtual px (KH2's 512 x 416 frame, 16:9 = x -85..597), corners TL TR BL BR (OpenKH Vec0..3)
    float u[4], v[4];   // texels of the logical sheet (camp 256 x 256, font 512 x 256)
    uint32_t col[4];    // 0xAABBGGRR, 0x80 = 1.0 (multiply, then clamp)
    float wrap[4];      // UV repeat region, texels: l r t b (l == r: none in u, t == b: none in v)
    uint8_t tex;        // 0 camp sheet, 1 sys font, 2 solid colour (no texture)
    uint8_t blend;      // 0 normal, 1 add, 2 subtract (VK_BLEND_OP_REVERSE_SUBTRACT: dst - src)
} SoraHudQuad;

#endif
// (s134 rcslot) KH2's Reaction Command slot over the command menu (comw group 2 offered / 4 flash at menu-local (36, 377 - 22 rows)):
// state 0 off, 1 offered, 2 flash; icon_label = cmd.bin label 42 (Triangle) / 43 (Y); q / n = the DLL's name text (sys font quads, menu-local
// PS2 px). Resend every frame (gone 3 presents after the last call). Returns 1 when cmd.bin has group 2 + the label, else 0 (the DLL keeps its box).
int sora_hud_menu_rc(int state, int icon_label, const SoraHudQuad *q, int n);
// Game thread, every frame it shows: n quads in draw order (copied). n <= 0 / NULL = hide now; > 2048 = clipped (logged
// once); 3 presents without a call = hidden. While shown nothing else of the HUD draws. No camp.bin = ignored (logged once).
void sora_hud_quads(const SoraHudQuad *q, int n);
void sora_hud_quads2(const SoraHudQuad *q, int n, int split);  // (s94) [0, split) under the screen-mode actors
void sora_hud_present_under(float vx, float vy, float vw, float vh);  // (s94) device.c, before sora_draw_present
bool sora_hud_quads_shown(void);  // drawn at the last present
// (mp Task 7) one label over a world point, e.g. the remote player's name plate: n quads (sys font tex 1 / solid tex 2, KH2
// virtual px around the label's anchor 0, 0) put on the projection of world (BotW units) with this frame's camera at
// present. Game thread, every frame it shows (copied, <= 64; gone 3 presents after the last call); n <= 0 / NULL = hide
// now. Behind the camera = not drawn; drawn over the KH2 HUD, never while sora_hud_quads' pause menu is up (that page
// owns the screen); needs camp.bin (the font).
void sora_hud_label(const float world[3], const SoraHudQuad *q, int n);
// (mp4) slots 0..3 (4 players), each stale after 3 presents; sora_hud_label = slot 0
void sora_hud_label_slot(int slot, const float world[3], const SoraHudQuad *q, int n);
bool sora_hud_itempic_ready(void);  // (s120) itempic.bin loaded: SoraHudQuad.tex 3 = the Keyblade picture atlas (u/v in its texels)

// Render thread: device.c present_locked, inside the window image's rendering, after sora_draw_present and before
// menu_render. (vx, vy, vw, vh) = the game image's place in the window: the HUD is anchored to its bottom-right corner,
// the menu to KH2's 512 x 416 frame centred in the image (see sora_hud.c's knobs).
void sora_hud_present(float vx, float vy, float vw, float vh);
