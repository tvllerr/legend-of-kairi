// (s131 orbs) KH2 prize orbs: an enemy Sora kills drops HP (green), MP (blue) and Drive (yellow) orbs and munny, which pop out,
// land, and fly to Sora when he is close. Replaces the instant Drive points of a kill (sora_drive.h dg_kill) while it is on.
// Report: D:/SoraBotW/recon/S131_HELPER_ORBS.md. Live keys (all "set <key> <v>", default ON, 0 = the old behaviour):
//   orb 0/1        the whole feature (0: a kill gives its Drive points at once, as before s131)
//   orbfx 0/1      the orb visuals (KH2 fx.bin stand-ins, see ORB_FX below); 0 = invisible orbs (pickups still work)
//   orbmunny 0/1/2 0 no munny (default, s131 user: no use for munny, the menu shows rupees) | 1 KH2 munny counter in the Munny row | 2 counter kept, the menu keeps BotW's rupees
//   orbmag N       magnet radius, x10 u (default 40 = 4 m, GUESS)
//   orblife N      seconds an orb stays before it fades (default 15, GUESS)
//   orbmodel 0/1   (s131 prize) 1 (default) = KH2's real prize orb (obj/PRIZE.mdlx's effect, fx.bin elements prize_hp / _mp / _drive / _munny, tools_s131p/prizefx.py;
//                  an fx.bin without them falls back to the stand-ins by itself) | 0 = the stand-ins below. orbrscale N = real orb size x100 (default 100)
//   orbscale N     fx size x100 (default 35, GUESS: the recorded elements are sized for Sora's body)
//   orbhpdrop 0/1  (s131 user) HP orbs drop at all (default 0: healing = BotW's hearts, potions, Cure)
//   orbhp N / orbmp N   HP / MP orb value scale, percent (default 100)
//   orbpt N        debug: drop N orbs of each kind at Sora's feet (like a kill), orbmunnyset N sets the munny counter
//   orblog 0/1     log every drop / pickup
//   orbsora 0/1    (s132 user) 1 (default) = orbs only once Sora is the body (as Roxas a kill's Drive points are instant, as before s131) | 0 = Roxas too
// KH2 data: 00battle.bin "przt" (184 rows x 24 B, header 8 B: version 2, count 0xB8) holds the real per-enemy prize rows (HP / MP / Drive
// orb counts, munny, item drops). Its column meaning is only half decoded (ids at +0, orb counts at +2..+10, 3 item id/percent pairs at +12),
// and BotW enemies have no przt id, so the COUNTS below are GUESS tiers by the levels table's KH HP percent (the same tiers dg_kill used); the
// Drive values (small 1, big 3 = status_kh2 PRIZE kinds 7 / 8) are the earlier-verified ones. HP / MP / munny AMOUNTS are GUESS.
// The real orb model is obj/PRIZE.mdlx (+ PRIZE_BOX_*): drawing it needs a new draw actor in the exe (see the report), so the orbs are fx stand-ins.
#pragma once

extern int g_hud_life_max;
enum { ORB_HP, ORB_MP, ORB_DRIVE, ORB_MUNNY, ORB_KINDS };
// fx.bin stand-ins, INVENTED: by colour only (cure sparkle green, blizzard hit blue, thunder hit yellow, keyblade hit spark = munny gold/white)
static const char *const ORB_FX[ORB_KINDS] = {"cure_sparkle", "blizzard_hit", "thunder_hit", "hit"};
static const char *const ORB_NAME[ORB_KINDS] = {"HP", "MP", "Drive", "munny"};
#define ORB_MAX 28
static const char *const ORB_PFX[ORB_KINDS] = {"prize_hp", "prize_mp", "prize_drive", "prize_munny"};  // (s131 prize) KH2's own orb, fx.bin from tools_s131p/prizefx.py
#define ORB_FXMAX 10  // orbs with a live fx at once (the exe has 32 fx slots shared with spells; s132 review: 12 -> 10, the prize orbs LOOP)
#define ORB_FXBUSY 24  // (s132 review) no NEW looping prize orb while this many fx slots are live (a Drive change = 12+ loops): spells keep room

typedef struct {
    bool used, landed;
    int kind, big;       // big: 0 small, 1 big (munny: the amount is in .amt)
    int amt;
    float pos[3], vel[3], t, y0, sp;  // t = age s, y0 = spawn height (landing fallback), sp = magnet speed m/s
    float fxp[3];
    int fx, fxk;         // fx handle; frames since respawn
    int fxm;             // (s132 review) 1 = fx is the looping prize orb, 0 = a stand-in (orbmodel switched live -> respawn)
    uint32_t fxg;        // g_fx_gen[fx] when this orb spawned it (fx_mine: the slot is still ours)
} Orb;

static struct {
    int on, fx, munny_mode, mag, life, scale, hpv, mpv, log, model, rscale;
    Orb o[ORB_MAX];
    struct { uint64_t a; float p[3]; } ec[64];  // the last position of every enemy linkpos listed (a kill's drop point)
    int nec;
    uint64_t last_ms;
    float dt;
    int munny;
    bool init, was_block;
    uint32_t rng;
    int picked[ORB_KINDS];
    int sora_only;  // (s132 user: orbs only once we are Sora) key orbsora
    int hp_drop;  // (s131 user: no HP orbs - healing stays BotW's + potions + Cure) 1 = drop them again
} ORB = {.on = 1, .fx = 1, .munny_mode = 0, .mag = 40, .life = 15, .scale = 35, .hpv = 100, .mpv = 100, .log = 1, .sora_only = 1, .model = 1, .rscale = 100, .rng = 0x9E3779B1u};

static float orb_rnd(void) {  // 0..1
    ORB.rng ^= ORB.rng << 13, ORB.rng ^= ORB.rng >> 17, ORB.rng ^= ORB.rng << 5;
    return (float)(ORB.rng & 0xFFFFFF) / 16777216.0f;
}
static int orb_munny(void) { return ORB.on && ORB.munny_mode == 1 ? ORB.munny : -1; }  // sora_camp.h: -1 = keep BotW's rupee read

// the munny follows BotW's saves like every other sidecar: in memory, read at a slot LOAD, written when BotW SAVES a slot (tmp + MoveFileEx);
// the process env keeps it over a DLL hot reload
#define ORB_ENV "SORA_KH2_ORB_STATE"
#define ORB_MUNNY_CAP 999999  // KH2's munny cap
static void orb_clear(void);
static void orb_munny_path(int slot, char *p, size_t n) { snprintf(p, n, "%s/%d/orbs.txt", sl_root(), slot); }
static void orb_persist(void) {
    char b[48];
    snprintf(b, sizeof b, "v1 munny=%d", ORB.munny);
    SetEnvironmentVariableA(ORB_ENV, b);
}
static void orb_slot_load(int slot) {
    char path[600];
    orb_clear();  // a load: no orb of the old world is collectable (their Drive is not paid, no duplicates)
    orb_munny_path(slot, path, sizeof path);
    FILE *f = fopen(path, "rb");
    int m = 0;
    if (f) {
        char b[96] = {0};
        size_t n = fread(b, 1, sizeof b - 1, f);
        (void)n, fclose(f);
        if (sscanf(b, "v1 munny=%d", &m) != 1) m = 0;
    }
    ORB.munny = m < 0 ? 0 : m > ORB_MUNNY_CAP ? ORB_MUNNY_CAP : m;
    orb_persist();
    LOG("[orb] slot %d loaded: munny %d (%s)\n", slot, ORB.munny, f ? "sidecar" : "no sidecar: 0");
}
static void orb_slot_saved(int slot) {
    char path[600], tmp[610];
    orb_munny_path(slot, path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) { LOG("[orb] munny sidecar not written (%s)\n", tmp); return; }
    fprintf(f, "v1 munny=%d\n", ORB.munny);
    fclose(f);
    if (!MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING)) LOG("[orb] munny sidecar rename failed (%s)\n", path);
    else LOG("[orb] slot %d saved: munny %d\n", slot, ORB.munny);
}

static void orb_fx_off(Orb *o) {
    if (fx_mine(o->fx, o->fxg)) sora_fx_kill(o->fx);  // never a slot a spell / Mickey's Pearl has taken since
    o->fx = -1;
}
static void orb_clear(void) {
    for (int i = 0; i < ORB_MAX; i++) if (ORB.o[i].used) orb_fx_off(&ORB.o[i]), ORB.o[i].used = false;
}
static void orb_pay_drive(const Orb *o) {  // an orb dropped before pickup (evicted / out of reach) still pays the Drive it carries
    if (o->used && o->kind == ORB_DRIVE) dg_gain(o->big ? 3.0f : 1.0f, "Drive orb paid (dropped)");
}
static Orb *orb_slot(void) {
    for (int i = 0; i < ORB_MAX; i++) if (!ORB.o[i].used) return &ORB.o[i];
    Orb *old = NULL;  // full: the oldest non-Drive orb gives way, else the oldest (its Drive is paid, not lost)
    for (int i = 0; i < ORB_MAX; i++) if (ORB.o[i].kind != ORB_DRIVE && (!old || ORB.o[i].t > old->t)) old = &ORB.o[i];
    if (!old) {
        old = &ORB.o[0];
        for (int i = 1; i < ORB_MAX; i++) if (ORB.o[i].t > old->t) old = &ORB.o[i];
    }
    orb_pay_drive(old);
    orb_fx_off(old);
    return old;
}
static void orb_spawn(int kind, int big, int amt, const float at[3]) {
    Orb *o = orb_slot();
    memset(o, 0, sizeof *o);
    o->used = true, o->kind = kind, o->big = big, o->amt = amt, o->fx = -1;
    float a = orb_rnd() * 2.0f * PI, sp = 1.2f + orb_rnd() * 2.2f;  // GUESS: a small pop
    o->pos[0] = at[0], o->pos[1] = at[1] + 0.7f, o->pos[2] = at[2];
    o->vel[0] = sinf(a) * sp, o->vel[1] = 4.0f + orb_rnd() * 2.0f, o->vel[2] = cosf(a) * sp;
    o->y0 = at[1];
}

// the kill hook (sora_levels.h sl_kill): true = the orbs carry this kill's Drive points (the caller skips the instant gain)
static bool orb_off_now(void) { return !ORB.on || (ORB.sora_only && rox_on_now()); }  // (s132 user) Roxas: no orbs
static bool orb_kill(const SlCredit *c) {
    if (orb_off_now()) return false;
    if (c->flags & 2) return true;  // an animal that does not fight: no prize (dg_kill gave none either)
    float at[3];
    bool have = false;
    for (int i = 0; i < ORB.nec; i++) if (ORB.ec[i].a == c->actor) memcpy(at, ORB.ec[i].p, sizeof at), have = true;
    if (!have) {
        float p[3];
        if (!linkpos_link_pos(p)) return false;
        at[0] = p[0] + sinf(S.face) * 1.5f, at[1] = p[1], at[2] = p[2] + cosf(S.face) * 1.5f;  // GUESS: in front of him
    }
    int tier = (c->flags & 1) || c->pct > 150 ? 2 : c->pct > 100 ? 1 : 0;
    // GUESS tiers (przt counts are not decoded per BotW enemy): Drive = the old dg_kill points (2 small = 2 / 2 big = 6 / 4 big = 12)
    static const int DRV_N[3] = {2, 2, 4}, DRV_BIG[3] = {0, 1, 1};
    static const int HP_PCT[3] = {60, 80, 100}, HP_N[3] = {1, 1, 2}, HP_BIG[3] = {0, 1, 1};
    static const int MP_PCT[3] = {50, 70, 100}, MP_N[3] = {1, 1, 2}, MP_BIG[3] = {0, 1, 1};
    static const int MUN_N[3] = {1, 2, 3}, MUN_LO[3] = {5, 10, 25}, MUN_SPAN[3] = {5, 10, 25};
    int n_hp = 0, n_mp = 0, n_dr = DRV_N[tier], n_mu = 0;
    if (ORB.hp_drop && orb_rnd() * 100.0f < (float)HP_PCT[tier]) n_hp = HP_N[tier];
    if (orb_rnd() * 100.0f < (float)MP_PCT[tier]) n_mp = MP_N[tier];
    if (ORB.munny_mode) n_mu = MUN_N[tier];
    for (int i = 0; i < n_hp; i++) orb_spawn(ORB_HP, HP_BIG[tier], 0, at);
    for (int i = 0; i < n_mp; i++) orb_spawn(ORB_MP, MP_BIG[tier], 0, at);
    for (int i = 0; i < n_dr; i++) orb_spawn(ORB_DRIVE, DRV_BIG[tier], 0, at);
    for (int i = 0; i < n_mu; i++) orb_spawn(ORB_MUNNY, tier, MUN_LO[tier] + (int)(orb_rnd() * (float)MUN_SPAN[tier]), at);
    if (ORB.log) LOG("[orb] %s (tier %d): %d HP %d MP %d Drive %d munny orbs at %.1f %.1f %.1f%s\n", c->name, tier, n_hp, n_mp, n_dr, n_mu, at[0], at[1], at[2], have ? "" : " (no enemy position: in front of Sora)");
    return true;
}

static void orb_pick(Orb *o) {
    ORB.picked[o->kind]++;
    switch (o->kind) {
    case ORB_HP: {  // GUESS: small 1 quarter heart, big 3 (the Drive orbs' 1 : 3 ratio); the hearts are BotW's
        int q = ((o->big ? 3 : 1) * ORB.hpv + 50) / 100;
        int life = linkpos_link_life();
        if (life > 0 && q > 0) link_heal(q);
        sora_sfx_event("cure_heal", 0.35f);  // GUESS: KH2's prize SEs are not in sfx.bin; the Cure heal chime stands in
        if (ORB.log) LOG("[orb] HP orb %s +%d quarter hearts (life %d -> %d, max %d)\n", o->big ? "big" : "small", q, life, linkpos_link_life(), g_hud_life_max);
        break;
    }
    case ORB_MP: {  // GUESS: small 6, big 18 of KH2's 100 MP; MP Charge takes them as the real mp_change does (x12 faster charge)
        int m = ((o->big ? 18 : 6) * ORB.mpv + 50) / 100;
        float was = MP.mp;
        if (m > 0) mp_change(m);
        sora_sfx_event("item", 0.5f);  // GUESS / INVENTED SE
        if (ORB.log) LOG("[orb] MP orb %s +%d (MP %.0f -> %.0f%s)\n", o->big ? "big" : "small", m, was, MP.mp, mp_charging() ? ", charging" : "");
        break;
    }
    case ORB_DRIVE: {  // VERIFIED values (small 1, big 3 points); dg_gain applies MP Charge x2 and the live drivemul, and holds in a Drive Form
        dg_gain(o->big ? 3.0f : 1.0f, o->big ? "Drive orb big" : "Drive orb small");
        sora_sfx_event("item", 0.5f);
        break;
    }
    default:
        if (ORB.munny_mode) {
            ORB.munny += o->amt;
            if (ORB.munny > ORB_MUNNY_CAP) ORB.munny = ORB_MUNNY_CAP;
            orb_persist();
            sora_sfx_event("item", 0.5f);
            if (ORB.log) LOG("[orb] munny +%d = %d\n", o->amt, ORB.munny);
        }
    }
}

// every driven frame (sora.c, next to skt_tick); p = Link's position, block = dead / loading
static void orb_tick(uint64_t present, const float p[3], bool block) {
    (void)present;
    uint64_t now = GetTickCount64();
    float dt = ORB.last_ms ? (float)(now - ORB.last_ms) / 1000.0f : 0.016f;
    ORB.last_ms = now;
    if (dt > 0.05f) dt = 0.05f;  // a pause / load gap: no teleporting
    if (orb_off_now()) {
        orb_clear();
        return;
    }
    if (!ORB.init) {  // a hot reload keeps the counter (the slot's file is read at a BotW load)
        ORB.init = true;
        char b[48];
        int m;
        if (GetEnvironmentVariableA(ORB_ENV, b, sizeof b) && sscanf(b, "v1 munny=%d", &m) == 1) ORB.munny = m < 0 ? 0 : m, LOG("[orb] hot reload: munny %d\n", ORB.munny);
    }
    {  // the enemies' last positions (a kill comes after the actor left the list)
        uint64_t a[64];
        float ps[64][3];
        int32_t l[64];
        int n = linkpos_enemies(a, ps, l, 64);
        for (int i = 0; i < n && i < 64; i++) ORB.ec[i].a = a[i], memcpy(ORB.ec[i].p, ps[i], sizeof ORB.ec[i].p);
        if (n > 0) ORB.nec = n > 64 ? 64 : n;
        else if (ORB.nec && ((present & 63) == 0)) ORB.nec = 0;
    }
    if (block && !ORB.was_block) orb_clear();  // dead / loading: the old world's orbs are gone (a load must not leave pickable orbs; no pay: no duplicates)
    ORB.was_block = block;
    if (block || MK.active) return;  // Sora knocked out (Mickey's rescue): the orbs wait, no age, no magnet
    float sc = 0.01f * g_k * (float)ORB.scale / 100.0f, mag = (float)ORB.mag / 10.0f;
    float tgt[3] = {p[0], p[1] + 0.8f, p[2]};
    int fxn = 0;
    for (int i = 0; i < ORB_MAX; i++) {
        Orb *o = &ORB.o[i];
        if (!o->used) continue;
        o->t += dt;
        float dx = tgt[0] - o->pos[0], dy = tgt[1] - o->pos[1], dz = tgt[2] - o->pos[2], d = sqrtf(dx * dx + dy * dy + dz * dz);
        if (d > 150.0f || o->t > (float)ORB.life + 3.0f) {  // too far (GUESS 150 u) / expired (GUESS 15 s + 3 s blink)
            if (d > 150.0f) orb_pay_drive(o);  // out of reach: the Drive is paid, as the old instant kill gain was
            orb_fx_off(o), o->used = false;
            continue;
        }
        if (d < 0.9f && o->t > 0.35f) {  // picked up (GUESS: 0.9 m, not before 0.35 s)
            orb_pick(o), orb_fx_off(o), o->used = false;
            continue;
        }
        if (o->landed || o->t > 0.6f) {
            if (d < mag && o->t > 0.5f) {  // the magnet: faster the closer GUESS
                o->sp += 22.0f * dt;
                float k = o->sp * dt / (d > 0.01f ? d : 0.01f);
                if (k > 1.0f) k = 1.0f;
                o->pos[0] += dx * k, o->pos[1] += dy * k, o->pos[2] += dz * k;
                o->landed = false, o->vel[0] = o->vel[1] = o->vel[2] = 0;
                goto drawn;
            } else o->sp = 0;
        }
        if (!o->landed) {
            o->vel[1] -= 14.0f * dt;  // GUESS gravity
            o->pos[0] += o->vel[0] * dt, o->pos[1] += o->vel[1] * dt, o->pos[2] += o->vel[2] * dt;
            if (o->vel[1] < 0) {
                float a[3] = {o->pos[0], o->pos[1] + 0.6f, o->pos[2]}, b[3] = {o->pos[0], o->pos[1] - 0.6f, o->pos[2]}, h[3] = {0}, n[3] = {0};
                if (linkpos_raycast(a, b, h, n) && n[1] > 0.3f && h[1] + 0.3f >= o->pos[1] - 0.05f) o->pos[1] = h[1] + 0.3f, o->landed = true;
                else if (o->t > 2.0f && o->pos[1] < o->y0 + 0.3f) o->pos[1] = o->y0 + 0.3f, o->landed = true;  // no floor ray (water / thin air): the spawn height
                if (o->landed) o->vel[0] = o->vel[1] = o->vel[2] = 0;
            }
        } else {  // landed: a gentle float
            o->pos[1] += sinf(o->t * 3.0f) * 0.15f * dt;
        }
    drawn:;
        // the visual: fx stand-in, respawned when it ends or when the orb moved away from where it was spawned; the last 3 s blink
        bool blink = o->t > (float)ORB.life && ((int)(o->t * 6.0f) & 1);
        if (ORB.fx && !blink && fxn < ORB_FXMAX && d < 60.0f) {
            fxn++;
            bool alive = fx_mine(o->fx, o->fxg) && sora_fx_alive(o->fx);
            int pid = ORB.model ? sora_fx_id(ORB_PFX[o->kind]) : -1;  // (s131 prize) the real orb: one looping element that follows sora_fx_move (no respawn on move)
            if (alive && o->fxm != (pid >= 0)) orb_fx_off(o), alive = false;  // (s132 review) orbmodel changed live: drop the other look
            if (pid >= 0) {
                if (!alive && sora_fx_count() >= ORB_FXBUSY) { o->fx = -1; continue; }  // (s132 review) slots busy: no new loop this frame (retries)
                if (!alive) {
                    o->fxm = 1;
                    o->fx = sora_fx_spawn(pid, o->pos, 0, 0.01f * g_k * (float)ORB.rscale / 100.0f * (o->big ? 1.4f : 1.0f));  // (big = 1.4x: GUESS, as the stand-ins)
                    o->fxg = o->fx >= 0 && o->fx < 32 ? g_fx_gen[o->fx] : 0;
                } else sora_fx_move(o->fx, o->pos, 0);
                memcpy(o->fxp, o->pos, sizeof o->fxp), o->fxk = 0;
                continue;
            }
            float mv = hypotf(o->pos[0] - o->fxp[0], o->pos[2] - o->fxp[2]) + fabsf(o->pos[1] - o->fxp[1]);
            if (alive && mv > 0.4f && ++o->fxk >= 3) orb_fx_off(o), alive = false;
            if (!alive) {
                int id = sora_fx_id(ORB_FX[o->kind]);
                o->fxm = 0;
                o->fx = id >= 0 ? sora_fx_spawn(id, o->pos, 0, sc * (o->big ? 1.4f : 1.0f)) : -1;
                o->fxg = o->fx >= 0 && o->fx < 32 ? g_fx_gen[o->fx] : 0;
                memcpy(o->fxp, o->pos, sizeof o->fxp), o->fxk = 0;
            } else sora_fx_move(o->fx, o->pos, 0);  // bound elements follow; the others stay (then the respawn above catches up)
        } else if (o->fx >= 0) orb_fx_off(o);  // (fxn cap ORB_FXMAX and 60 u draw range: GUESS)
    }
}

static bool orb_live_set(const char *key, int v) {
    if (!strcmp(key, "orb")) {
        ORB.on = v != 0, LOG("[orb] %s\n", ORB.on ? "on (kills drop orbs)" : "off (a kill's Drive points are instant, as before s131)");
        if (!ORB.on) orb_clear();
    } else if (!strcmp(key, "orbfx")) ORB.fx = v != 0, LOG("[orb] visuals %s\n", ORB.fx ? "on" : "off");
    else if (!strcmp(key, "orbsora")) ORB.sora_only = v != 0, LOG("[orb] %s\n", ORB.sora_only ? "orbs only as Sora (Roxas: instant Drive points)" : "orbs as Roxas too");
    else if (!strcmp(key, "orbmodel")) ORB.model = v != 0, LOG("[orb] orb look: %s\n", ORB.model ? "KH2 prize orb (fx.bin prize_*; stand-ins if the pack lacks them)" : "stand-ins");
    else if (!strcmp(key, "orbrscale")) ORB.rscale = v < 1 ? 1 : v > 1000 ? 1000 : v;
    else if (!strcmp(key, "orbmunny")) ORB.munny_mode = v < 0 ? 0 : v > 2 ? 2 : v, LOG("[orb] munny mode %d (0 none, 1 KH2 counter in the menu, 2 counter kept / menu shows rupees)\n", ORB.munny_mode);
    else if (!strcmp(key, "orbmag")) ORB.mag = v < 0 ? 0 : v > 200 ? 200 : v;
    else if (!strcmp(key, "orblife")) ORB.life = v < 1 ? 1 : v > 120 ? 120 : v;
    else if (!strcmp(key, "orbscale")) ORB.scale = v < 1 ? 1 : v > 1000 ? 1000 : v;
    else if (!strcmp(key, "orbhpdrop")) ORB.hp_drop = v != 0;
    else if (!strcmp(key, "orbhp")) ORB.hpv = v < 0 ? 0 : v > 10000 ? 10000 : v;
    else if (!strcmp(key, "orbmp")) ORB.mpv = v < 0 ? 0 : v > 10000 ? 10000 : v;
    else if (!strcmp(key, "orblog")) ORB.log = v != 0;
    else if (!strcmp(key, "orbmunnyset")) ORB.munny = v < 0 ? 0 : v > ORB_MUNNY_CAP ? ORB_MUNNY_CAP : v, orb_persist(), LOG("[orb] munny set to %d\n", ORB.munny);
    else if (!strcmp(key, "orbpt")) {  // debug: v of each kind at Sora's feet
        float p[3];
        if (!linkpos_link_pos(p)) return true;
        for (int i = 0; i < v && i < 6; i++) {
            if (ORB.hp_drop) orb_spawn(ORB_HP, i & 1, 0, p);
            orb_spawn(ORB_MP, i & 1, 0, p), orb_spawn(ORB_DRIVE, i & 1, 0, p), orb_spawn(ORB_MUNNY, 0, 10, p);
        }
        LOG("[orb] debug drop: %d of each kind at %.1f %.1f %.1f\n", v, p[0], p[1], p[2]);
    } else return false;
    return true;
}
