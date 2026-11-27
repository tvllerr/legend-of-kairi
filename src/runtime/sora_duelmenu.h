// (s289 duelmenu) The command menu's "Duel" (the user's route (a), recon/s289/S289_HELPER_DUELPLAN.md 2.5). Page 2 (d-pad Left:
// Attack / Summon / Party / Limit) shows "Duel" in KH2's Party slot (slot 2) while a duel-ready friend is in the room (g_duel,
// net_duel_slot_ok; the pretend friend of live duelfake 1 counts), or while our own duel is past idle. It opens a text list (M.sub
// DU_SUB, drawn in the Drive page's look): idle = "Ask <name>" per friend, asked = Accept / Decline, asking = Cancel, countdown /
// fight = Forfeit; the rows and their greyed reasons are runtime/sora_duelmenu_core.h (host test runtime/test/duelmenu_test.c).
// A pick closes the list (the menu goes back to page 1, Attack) and its call runs on the next menu frame, because du_gate refuses
// while a menu is up (du_menu_up): du_ask / du_accept / du_decline / the live "duel end" line's END cancel / du_end lose-forfeit,
// the calls the Triangle prompts and the live "duel" lines make. "Ask" uses the Triangle prompt's own distance (duelnear, 6 m flat,
// the 3 m height band, no facing), checked again on that next frame. Page 2 then opens with no summon owned too (Summon greyed);
// Genie keeps slot 2 while he is out; the Slate whose slot 2 Duel takes is on d-pad Right on Attack (page 1 as before, page 2 too
// while Duel has the slot). The Triangle prompts are unchanged.
// Slot 2's label = cmd.bin label 56 (the "-" placeholder: a space glyph, 5.2 px at menu_font 21) with the DLL's text over it
// (sora_hud_menu_text, as sub_text_rows does for the meals / Slate / Summon rows); label 2 "Items" goes back on page 1.
// Live: duelmenu 0|1 (default 1 since s290; 0 = page 2 exactly as before), duelmenux / duelmenuy = the "Duel" text's offset (PS2 px of the menu
// frame, default 0 0). Hooks: sora_summon_menu.h (smn_menu_ok / smn_slate_row / smn_toggle / smn_top_tick / smn_hud /
// smn_live_set) and sora_meals.h sub_text_rows (the "Duel" text while another one-row page leaves slot 2 in view).
// Included after sora_duel.h. GUESS: the text's place (sub_text_rows' x + 10, y - 12 on slot 2's row), the texts, page 1 after a pick.
#pragma once
#include "sora_duelmenu_core.h"

#define DUM_LABEL 56      // cmd.bin label 56 "-" (code {1}: a space, 5.2 px drawn): slot 2's strip under the DLL's "Duel"
#define DUM_PEND_MS 1000  // a pick's call waits at most this long for the next menu frame

static int g_dum = 1, g_dum_dx = 0, g_dum_dy = 0;  // live duelmenu / duelmenux / duelmenuy. (s290: passed the two-copy lab, sb/s290/LAB_RESULTS.md; the user: "Flip the passes ON" -> default ON)
static struct {
    struct { int act, k, phase; uint32_t id; uint64_t at; } pend;  // the pick waiting for the next menu frame (act DUM_NONE = none)
    int ph;          // the phase the open list showed last frame
    uint64_t ph_at;  // when that phase changed under the open list (dum_settled; 0 = not since it opened)
    bool lab;        // label 56 went on slot 2 (2 "Items" goes back on page 1)
    bool txt;        // this file sent the menu text last (the "Duel" text / the list's rows): cleared at once when it goes
} DUM;

static const char *dum_ph(int ph) { return ph >= 0 && ph < DU_NPH ? DU_PH_NAME[ph] : "?"; }
static bool dum_any_friend(void) {
    if (DU.fake_mode == 1 && DU.fk.on) return true;
    for (int k = 0; k < NET_MAX_PEERS; k++)
        if (net_duel_slot_ok(k)) return true;
    return false;
}
// page 2 shows "Duel" (dum_show: the key, duels on, a friend in the room or our duel past idle)
static bool dum_row(void) { return g_dum && dum_show(true, g_duel && g_on, DU.sm.phase, dum_any_friend() ? 1 : 0); }
// Duel has page 2's slot 2 now (Genie keeps it while he is out)
static bool dum_slot2(void) { return dum_row() && smn_page2() && !gl_menu(); }

// the friends in the room as the Triangle prompt sees them (du_input): net slot order, the pretend friend last
static int dum_friends(DuMenuFriend *f, int cap, uint64_t now) {
    int n = 0;
    float me[3] = {g_body[0], g_body[1], g_body[2]};
    for (int k = 0; k < NET_MAX_PEERS && n < cap; k++) {
        if (!net_duel_slot_ok(k)) continue;
        float ft[3];
        bool pose = du_peer_feet(k, ft, now);
        uint16_t fl = du_peer_flags(k);
        f[n++] = (DuMenuFriend){.k = k, .name = NET.slot[k].p.name, .pose = pose, .in_duel = (fl & NF_DUEL) != 0,
                                .away = (fl & (NF_DEAD | NF_LOADING | NF_DOWNED)) != 0, .dist = pose ? du_hdist(ft, me) : -1.0f, .dy = pose ? ft[1] - me[1] : 0.0f};
    }
    if (DU.fake_mode == 1 && DU.fk.on && n < cap)  // (the prompt's pretend friend has no height band)
        f[n++] = (DuMenuFriend){.k = DU_FAKE_K, .name = "Pretend", .pose = true, .dist = du_hdist(DU.fk.feet, me), .dy = 0.0f};
    return n;
}
// the rows now. Our gate = du_ask's / du_accept's strict one, taken with the list itself closed (it closes before the call); the
// other phases' rows need none (no scan then)
static int dum_rows_now(DuMenuRow *r, uint64_t now) {
    DuMenuFriend f[NET_MAX_PEERS + 1];
    int nf = dum_friends(f, NET_MAX_PEERS + 1, now), ph = DU.sm.phase, gate = DUG_OK;
    if (ph == DU_IDLE || ph == DU_OVER || ph == DU_ASKED) {
        int sub = M.sub;
        M.sub = -1;
        gate = du_gate_now(now, true, ph == DU_OVER);
        M.sub = sub;
    }
    return du_menu_rows(ph, gate, f, nf, g_du_near_m, r, DUM_MAX);
}
static void dum_rows_log(const char *what, const DuMenuRow *r, int n) {
    char b[320];
    int o = 0;
    b[0] = 0;
    for (int i = 0; i < n && o < (int)sizeof b - 1; i++) {
        o += snprintf(b + o, sizeof b - (size_t)o, "%s%s%s", i ? " | " : "", r[i].text, r[i].grey ? " (greyed)" : "");
        if (o > (int)sizeof b - 1) o = (int)sizeof b - 1;
    }
    LOG("[duel] menu: %s (%s): %s\n", what, dum_ph(DU.sm.phase), b);
}

// ---- input (smn_top_tick)
// page 2, slot 2, A or Right: the list
static bool dum_open(void) {
    DuMenuRow r[DUM_MAX];
    int n = dum_rows_now(r, GetTickCount64());
    if (n < 1) return false;
    M.sub = DU_SUB, M.sub_cursor = 0, DUM.ph = DU.sm.phase, DUM.ph_at = 0;
    sora_sfx_event("submenu", 1);
    dum_rows_log("Duel list open", r, n);
    return true;
}
// a row taken: the list closes (back to page 1, Attack), the call waits for the next menu frame (dum_pending_tick)
static void dum_pick(const DuMenuRow *p, uint64_t now) {
    DUM.pend.act = p->act, DUM.pend.k = p->k, DUM.pend.phase = DU.sm.phase, DUM.pend.id = DU.sm.id, DUM.pend.at = now ? now : 1;
    M.sub = -1, M.cursor = 0, M.sub_cursor = 0, M.dead = 8, SMN.page2 = false;
    if (p->act == DUM_DECLINE || p->act == DUM_CANCEL) sora_sfx_event("decide", 1);  // (du_ask / du_go_live / du_end play their own)
    LOG("[duel] menu: \"%s\" picked: the list closes, the call is next frame\n", p->text);
}
// M.sub DU_SUB (smn_top_tick, before the page 2 check): -> 1 = handled
static int dum_sub_tick(uint64_t *press) {
    uint64_t now = GetTickCount64();
    DuMenuRow r[DUM_MAX];
    int n = dum_slot2() ? dum_rows_now(r, now) : 0;
    if (n < 1) {  // (the row went: duelmenu 0, the friend left, page 2 gone, Genie out)
        M.sub = -1, M.cursor = 0;
        LOG("[duel] menu: Duel list closed (%s)\n", !g_dum ? "duelmenu 0" : !dum_slot2() ? "no Duel in slot 2 now" : "no rows");
        return 1;
    }
    if (DU.sm.phase != DUM.ph) {  // (his ASK / ANSWER / END, a time-out: new rows under the cursor)
        DUM.ph = DU.sm.phase, DUM.ph_at = now ? now : 1, M.sub_cursor = 0;
        dum_rows_log("Duel list changed", r, n);
    }
    if (M.sub_cursor < 0 || M.sub_cursor >= n) M.sub_cursor = 0;
    if (*press & (BTN_JUMP | BTN_LEFT)) {
        *press &= ~(BTN_JUMP | BTN_LEFT);
        M.sub = -1, M.cursor = 2;
        sora_sfx_event("cancel", 1);
        LOG("[duel] menu: Duel list closed\n");
        return 1;
    }
    if (*press & BTN_UP) M.sub_cursor = (M.sub_cursor + n - 1) % n, sora_sfx_event("cursor", 1);
    if (*press & BTN_DOWN) M.sub_cursor = (M.sub_cursor + 1) % n, sora_sfx_event("cursor", 1);
    if (*press & BTN_ATTACK) {
        *press &= ~BTN_ATTACK;
        const DuMenuRow *p = &r[M.sub_cursor];
        if (!dum_settled(DUM.ph_at, now)) {
            sora_sfx_event("error", 1);
            LOG("[duel] menu: \"%s\" not taken: the list just changed (press again)\n", p->text);
        } else if (M.dead > 0 || p->grey || p->act == DUM_NONE) {
            sora_sfx_event("error", 1);
            LOG("[duel] menu: \"%s\" greyed (%s)\n", p->text, M.dead > 0 ? "the menu's pause after a command" : p->act == DUM_NONE ? "the reason row" : dum_why_long(p->why));
        } else dum_pick(p, now);
    }
    return 1;
}
// the start of every smn_top_tick: last frame's pick makes its call now, with the list closed (the same calls as the Triangle prompts /
// the live "duel" lines)
static void dum_pending_tick(void) {
    if (DUM.pend.act == DUM_NONE) return;
    int act = DUM.pend.act, k = DUM.pend.k, ph = DUM.pend.phase;
    uint32_t id = DUM.pend.id;
    uint64_t at = DUM.pend.at, now = GetTickCount64();
    DUM.pend.act = DUM_NONE;
    const char *an = act > 0 && act < DUM_NACT ? DUM_ACT_NAME[act] : "?";
    if (now > at + DUM_PEND_MS) { LOG("[duel] menu: %s dropped: no menu frame for %llu ms after the pick\n", an, (unsigned long long)(now - at)); return; }
    if (!g_dum || !g_duel || !g_on) { LOG("[duel] menu: %s dropped: %s\n", an, !g_dum ? "duelmenu 0" : "duel 0 / Sora mode off"); return; }
    if (DU.sm.phase != ph || DU.sm.id != id) {
        sora_sfx_event("error", 1);
        LOG("[duel] menu: %s dropped: the duel moved on (%s -> %s)\n", an, dum_ph(ph), dum_ph(DU.sm.phase));
        return;
    }
    switch (act) {
    case DUM_ASK: {  // (the Triangle prompt's distance once more: he may have walked off in that frame)
        DuMenuRow r[DUM_MAX];
        int n = dum_rows_now(r, now), i = dum_find_ask(r, n, k);
        if (i < 0 || r[i].grey) {
            sora_sfx_event("error", 1);
            LOG("[duel] menu: Ask refused here: %s\n", i < 0 ? "that friend is not in the room now" : dum_why_long(r[i].why));
            return;
        }
        LOG("[duel] menu: %s (%s)\n", r[i].text, k == DU_FAKE_K ? "the pretend friend" : k == 0 ? "slot 1" : k == 1 ? "slot 2" : "slot 3");
        du_ask(k, now);
        return;
    }
    case DUM_ACCEPT:
        LOG("[duel] menu: Accept\n");
        du_accept(now);
        return;
    case DUM_DECLINE: du_decline(DUA_DECLINE, 0, "Decline in the command menu", now); return;
    case DUM_CANCEL: {  // (the live "duel end" line's asking branch, as it is)
        LOG("[duel] menu: Cancel: your ask to %s is called off\n", du_who());
        NetDuelMsg m = {.op = NDO_END, .id = DU.sm.id, .reason = DUE_CANCEL};
        du_send(&m, now);
        du_drop("Duel cancelled", now);
        return;
    }
    case DUM_FORFEIT:
        if (du_sm_live(&DU.sm)) {
            LOG("[duel] menu: Forfeit\n");
            du_end(DUR_LOSE, DUE_GAVEUP, true, now);  // (s289 lab: was DUE_FORFEIT = "you left the ring" on both banners)
        }
        return;
    }
}

// ---- HUD (smn_hud, sub_text_rows)
typedef void (*DumTextFn)(const SoraHudQuad *, int);
static DumTextFn dum_text_fn(void) {
    static DumTextFn fn;
    static bool tried;
    if (!tried) {
        tried = true, fn = (DumTextFn)(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_menu_text");
        if (!CAMP.tried) camp_load();
        if (!fn || !CAMP.ok) LOG("[duel] menu: %s: the Duel slot and list have no text (blank rows)\n", !fn ? "sora_hud_menu_text not in this exe" : "camp_seq.bin not loaded");
    }
    return CAMP.ok ? fn : NULL;
}
static void dum_top_label(int label) {
    static void (*top)(int, int);
    static int tried;
    if (!tried) tried = 1, top = (void (*)(int, int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_menu_top");
    if (top) top(2, label);
}
// one text in the menu frame (PS2 px), sub_text_rows' font and scale
static int dum_quads(SoraHudQuad *q, int n, int cap, const char *txt, float x, float y, uint32_t col) {
    uint8_t msg[64];
    int len = 0;
    for (int c = 0; txt[c] && len < 63; c++) {
        uint8_t b = txt[c] == ' ' ? 0x01 : cs_ascii(&CAMP.seq, txt[c]);
        if (b) msg[len++] = b;
    }
    CsOut o = {q, n, cap};
    static const float W[4] = {1, 1, 1, 1};
    cs_text(&CAMP.seq, &o, msg, len, x, y, 0.8f, W, NULL, NULL);
    for (int i = n; i < o.n; i++) for (int c = 0; c < 4; c++) q[i].col[c] = col;
    return o.n;
}
// slot 2's "Duel" on the 4-row page (row y = 410 - 22 * 4 + 22 * 2 = 366; the label's place x + 10, y - 12 as sub_text_rows; the
// selected row sits 8 px right, KH2's SEL_ST / COL_ST state 0 colours)
static int dum_top_quads(SoraHudQuad *q, int n, int cap) {
    if (!CAMP.ok) return n;
    bool sel = M.cursor == 2;
    float x = 28.0f + (sel ? 8.0f : 0.0f) + 10.0f + (float)g_dum_dx, y = 366.0f - 12.0f + (float)g_dum_dy;
    return dum_quads(q, n, cap, "Duel", x, y, sel ? 0x80E6E6E6 : 0x80B4B4B4);
}
// sub_text_rows' hook (sora_meals.h): a page of ns rows (bottom-aligned) leaves slot 2 in view when it has one row -> + "Duel"
static int dum_top_text(SoraHudQuad *q, int n, int cap, int ns) {
    if (ns >= 2 || M.shortcut || !dum_slot2()) return n;
    return dum_top_quads(q, n, cap);
}
// smn_hud on page 2, in the slot 2 chain (after Genie): Duel's slot label + state
static void dum_hud_slot(SoraHudMenu *m) {
    dum_top_label(DUM_LABEL), DUM.lab = true;
    m->top_state[2] = 0;
}
// the exe keeps a menu text set 3 presents: when ours goes (page 1, a pick, the friend left), an empty set at once, so "Duel" / the
// list's rows are not drawn over slot 2's next label. Only with no other page open (M.sub < 0) or ours: then no other caller sends
// menu text this frame (meal_hud M.sub 4, sk_hud SK_SUB, pl_hud PL_SUB, smn_hud SMN_SUB). Key off: DUM.txt is never set, no call.
static void dum_text_gone(void) {
    if (!DUM.txt) return;
    DUM.txt = false;
    DumTextFn fn = M.sub < 0 || M.sub == DU_SUB ? dum_text_fn() : NULL;
    if (fn) fn(NULL, 0);
}
// smn_hud on page 2: the Duel list (M.sub DU_SUB) / the "Duel" text over slot 2 when no page is open
static void dum_hud(SoraHudMenu *m) {
    if (!dum_slot2()) { dum_text_gone(); return; }
    if (M.sub == DU_SUB) {
        DuMenuRow r[DUM_MAX];
        int n = dum_rows_now(r, GetTickCount64());
        if (n < 1) { dum_text_gone(); return; }  // (dum_sub_tick closes it)
        const char *rows[DUM_MAX];
        for (int i = 0; i < n; i++) rows[i] = r[i].text, m->sub_label[i] = DUM_LABEL, m->sub_state[i] = r[i].grey ? 5 : 0, m->sub_cost[i] = 0;
        int cur = M.sub_cursor >= 0 && M.sub_cursor < n ? M.sub_cursor : 0;
        m->sub = 2, m->n_sub = n, m->sub_cursor = cur;
        if (sub_text_rows(m, rows, n, cur)) DUM.txt = true;  // (+ the "Duel" text through dum_top_text)
        else dum_text_fn();  // (no export / font: logged once, blank rows)
        return;
    }
    if (M.sub >= 0) return;  // (another page: its sub_text_rows adds the "Duel" text while slot 2 shows)
    DumTextFn fn = dum_text_fn();
    static SoraHudQuad q[8];
    if (fn) fn(q, dum_top_quads(q, 0, 8)), DUM.txt = true;
}
// smn_hud off page 2: the list closed, slot 2's label back to 2 "Items" (page 1's, sk_hud's SK_PLACEHOLDER_TOP), our text gone
static void dum_hud_off(void) {
    if (M.sub == DU_SUB) M.sub = -1, M.cursor = 0;
    if (DUM.lab) dum_top_label(SK_PLACEHOLDER_TOP), DUM.lab = false;
    dum_text_gone();
}

// ---- live (smn_live_set's chain)
static bool dum_live_set(const char *key, int v) {
    if (!strcmp(key, "duelmenu")) {
        if (g_dum == (v != 0)) return true;  // (s290: a live rewrite re-applies every line: one line only when it changes)
        g_dum = v != 0;
        if (!g_dum) DUM.pend.act = DUM_NONE;
        LOG("[duel] duelmenu %d (%s)\n", g_dum, g_dum ? "page 2 (d-pad Left) shows \"Duel\" in slot 2 while a friend is in the room: Ask / Accept / Decline / Cancel / Forfeit"
                                                     : "off: page 2 as before");
    } else if (!strcmp(key, "duelmenux")) g_dum_dx = v < -200 ? -200 : v > 200 ? 200 : v, LOG("[duel] menu: the \"Duel\" text %+d px across\n", g_dum_dx);
    else if (!strcmp(key, "duelmenuy")) g_dum_dy = v < -200 ? -200 : v > 200 ? 200 : v, LOG("[duel] menu: the \"Duel\" text %+d px down\n", g_dum_dy);
    else return false;
    return true;
}
