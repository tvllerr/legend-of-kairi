// sora_magic2.h: (s133 magic) KH2's Magnet and Reflect for Sora (3 tiers each: Magnet / Magnera / Magnega, Reflect /
// Reflera / Reflega - names VERIFIED in KH2's sys.bar messages 1300-1305). Included from sora.c right after
// spell_hit_before (needs SpellObj-era helpers, S, sl_hit, ah_*, dg_hit, kbfx_name, ul_beasts, sora_fx_*, sora_sfx_*).
// Recon: recon/S133_HELPER_MAGIC.md. Every number below is KH2 data with its source, or marked GUESS.
//
// DATA (PHH cache = %LOCALAPPDATA%/ProjectHeartHunters/cache; PHH = Project Heart Hunters, read for data only):
//  - cmds: Magnet cmd 174 / 175 / 176 Cost 30, Reflect cmd 177 / 178 / 179 Cost 10 (file:kh2_first/03system.bin@0x1F24 ..
//    0x2014, mined/sora_spells.kh.json Id 4 / 5; cmds.kh.json). Casting motions (P_EX100.mset): Magnet ground 68 (slot 272,
//    A365, magic_cast @34), air 70, no finisher; Reflect ground 71 (slot 284, magic_cast @0), finisher 72, air 73.
//    Voices (magc Voice) Magnet 24, Reflect 7 / finisher 25 (the DLL has no voice table call for them yet: silent, see report).
//  - scripts: bdx/magic_MAGNET_<n>_mag.bdx / magic_REFLECT_<n>_mag.bdx disassembled with PHH's bdx_vm.disasm (the lk
//    variants are byte-identical): MAGIC_SHOT (key 21) entry word 18.
//      Magnet: target = the lock target's position, else the Keyblade tip (element 1's position); ground trace from
//      (target + height up) down 2 x height; the sphere centre = ground + height (no ground: the target position
//      itself). Pull attack atkp 658 (Interval 0 = each enemy hit ONCE per attack), radius magi+76, created at the shot
//      and freed after the hold. Element 2 (appear) at the centre at the shot; after 16 frames element 3 (the sphere,
//      1 cm higher) grows over magi+64 frames (effect_set_scale((sin((t/grow - 0.5) pi) + 1) / 2)), holds magi+68
//      frames (the pull attack lives to here), then element 4 starts and element 3 shrinks over magi+72 frames, killed.
//      magi (03system pref records 0 / 2 / 4 for the 3 tiers, file:kh2_first/03system.bin@0x185AC / 0x186A4 / 0x1879C):
//      +60 height 180 / 270 / 360, +64 grow 16 / 24 / 32, +68 hold 135 / 150 / 165, +72 shrink 16, +76 radius 250 / 500 / 800.
//      Pull reaction (atkp EnemyReaction 5; 00battle.bin atkp rows @0xBB88 / 0xBBB8 / 0xBBE8): hold radius rolled once
//      at the hit between KnockbackStrength1 / 2 = 40..120 / 70..170 / 100..220, swirl (+0x10) -3 / -6 / -12; per KH frame
//      (PHH magic_vm.pull_step = re:0x40F8E0 / 0x3B7680) the enemy moves clamp((dist - hold) x 0.1, -20, 20) cm toward the
//      centre plus the swirl cm along the flat tangent. Power 100, Drive 5.
//      Reflect: element 0 (the barrier, bound to Sora, 55 frames in the recording = its life, fade 8) + the guard attack
//      atkp 573 (Power 0, a Guard-type attack) of radius magi+80 = 110 / 160 / 210 following it; every time its reflect
//      count rises, element 1 (the flash, 17 frames) starts if the last one is gone. When the barrier element ends: the
//      count c of blocked hits picks the burst: c >= magi+112 (10) -> tier 3, c >= magi+100 (5) -> tier 2, else tier 1; c = 0
//      -> no burst at all. Burst element 4 / 5 / 6 on Sora, then attack A (574 / 576 / 578, Power 50, Interval 10 / 8 / 6 per
//      tier) radius magi+92 / +104 / +116 (240 / 320 / 380 | 300 / 360 / 410 | 380 / 400 / 440), height magi+96 / +108 / +120
//      (150 | 180 | 220) lives magi+84 = 28 frames, then attack B (667 / 668 / 669, Power 550) radius + 20, height + 20, lives
//      magi+88 = 6 frames.
//  - atkp rows (00battle.bin 0x30-byte rows, bytes read here): 658 react 5; 574 react 3, 576 / 578 react 4 (kb 15), 667 / 668
//    react 6 (kb 200 / 300), 669 react 7 (kb 40 / 30); Drive drain 5 (658), 8 (574-578, 667-669).
//  - sounds: sfx.bin already holds "magnet_cast" / "reflect_cast" and the PAX element sounds "magnet_<n>.<e>" /
//    "reflect_<n>.<e>" (no new sound needed). Effects: tools_s133m/magicfx.py (kh2_fx.bin recordings magnet_<n>.0~34 / .1 / .2 /
//    .3 / .4, reflect_<n>.0 / .1 / .4 / .5 / .6) -> recon/wip_s133_magic/install_magicfx.sh.
// GUESSes are tagged "GUESS" below and listed in the report.
#pragma once

// ---- tables (tier 0 / 1 / 2 = Lv1 / Lv2 / Lv3)
static const int MG_MAG_H[3] = {180, 270, 360}, MG_MAG_GROW[3] = {16, 24, 32}, MG_MAG_HOLD[3] = {135, 150, 165}, MG_MAG_R[3] = {250, 500, 800};
#define MG_MAG_SHRINK 16
#define MG_MAG_WAIT 16  // the script's first wait (frames from the shot to the sphere element)
static const int MG_PULL_KB1[3] = {40, 70, 100}, MG_PULL_KB2[3] = {120, 170, 220}, MG_PULL_SWIRL[3] = {-3, -6, -12};
#define MG_PULL_RATE 0.1f  // re:0x6239F8
#define MG_PULL_MAX 20.0f  // re:0x623BC8
#define MG_MAG_POW 100     // atkp 658 Power
static const int MG_REF_GUARD[3] = {110, 160, 210};
#define MG_REF_LIFE 55     // frames: the barrier element's recording (reflect_<n>.0 = 55 frames)
static const int MG_REF_N2 = 5, MG_REF_N3 = 10;  // magi+100 / +112
static const int MG_REF_R[3][3] = {{240, 320, 380}, {300, 360, 410}, {380, 400, 440}};  // [burst tier][spell tier]
static const int MG_REF_H[3] = {150, 180, 220};
static const int MG_REF_ROW_A[3] = {574, 576, 578}, MG_REF_ROW_B[3] = {667, 668, 669};
static const int MG_REF_REACT_A[3] = {3, 4, 4}, MG_REF_KB_A[3] = {0, 15, 15};
static const int MG_REF_REACT_B[3] = {6, 6, 7}, MG_REF_KB1_B[3] = {200, 300, 40}, MG_REF_KB2_B[3] = {0, 0, 30};
static const int MG_REF_IV[3] = {10, 8, 6};  // atkp Interval per spell tier (SubId 0 / 512 / 1024)
#define MG_REF_TA 28       // magi+84
#define MG_REF_TB 6        // magi+88
#define MG_REF_POW_A 50
#define MG_REF_POW_B 550

static int g_mg_magnet = 1, g_mg_reflect = 1;  // live "set magnet 0" / "set reflect 0" = the spell is not offered (old behaviour)
static int g_mg_diag = 0;                      // live "set spdiag 1": per-event log lines (radius / pulls / hits / blocks)
static int g_mg_size = 150;                    // GUESS: the biggest KH HP percent (levels table) Magnet can pull: Moblin-size yes, Hinox / Talus / Lynel / bosses no (live "set mgsize")
#define MG_BODY_UP 90                          // GUESS: cm from an enemy's feet to its body centre (as Cure's 90 / the Wisdom shot's 90)
#define MG_BODY_R 40                           // GUESS: cm of enemy body radius added to the attack radius (BotW gives none)

// ---- ownership (sora_unlock.h): Magnet after the 2nd Divine Beast, Reflect after the 3rd; debugall sets the fake beasts to 4
static bool mg_owned(int sp) {
    if (sp < SPELL_MAGNET) return true;
    if (sp == SPELL_MAGNET && !g_mg_magnet) return false;
    if (sp == SPELL_REFLECT && !g_mg_reflect) return false;
    return ul_spell_owned(sp);
}

// ---- effects (appended to fx.bin by recon/wip_s133_magic/install_magicfx.sh; looked up by name, a missing pack = no effects, logged once)
static int mg_have_fx(void) {
    static int have = -1;
    if (have < 0 && sora_fx_id("fire_ring") >= 0) {  // (the pack is up when a known element resolves; sp_fx probes the same way)
        have = sora_fx_id("magnet_start") >= 0 && sora_fx_id("reflect_barrier") >= 0;
        LOG("[magic] fx pack %s\n", have ? "has the Magnet / Reflect elements" : "has NO Magnet / Reflect elements: spells run without effects (run recon/wip_s133_magic/install_magicfx.sh with the game closed)");
    }
    return have > 0;
}
static const char *mg_fxname(const char *base, int tier, char *buf, size_t n) {  // "<base>", "<base>_2", "<base>_3"
    if (tier <= 0) return base;
    snprintf(buf, n, "%s_%d", base, tier + 1);
    return buf;
}
// (s133 review) a handle + its g_fx_gen: exe fx handles are slot numbers that get reused once an effect ends, so a bare
// int may already be another effect's (an orb, a spark, the Drive aura) - kill / move / alive only through these
typedef struct { int h; uint32_t g; } MgFx;
static const MgFx MG_NOFX = {-1, 0};
static MgFx mg_fx(const char *base, int tier, const float pos[3], float yaw, float scale) {
    MgFx f = MG_NOFX;
    if (!mg_have_fx()) return f;
    char nb[48];
    f.h = sora_fx_event(mg_fxname(base, tier, nb, sizeof nb), pos, yaw, scale);
    if (f.h < 0) f.h = -1;
    else if (f.h < 32) f.g = g_fx_gen[f.h];
    return f;
}
static bool mg_alive(MgFx *f) {
    if (f->h >= 0 && ((f->h < 32 && !fx_mine(f->h, f->g)) || !sora_fx_alive(f->h))) *f = MG_NOFX;
    return f->h >= 0;
}
static void mg_kill(MgFx *f) { if (mg_alive(f)) sora_fx_kill(f->h); *f = MG_NOFX; }
static void mg_move(MgFx *f, const float p[3], float yaw) { if (mg_alive(f)) sora_fx_move(f->h, p, yaw); }
static void mg_sfx_at(const char *fmt, int tier, int elem, const float pos[3]) {  // "magnet_%d.%d": the PAX element's own sound (sfx.bin)
    char nm[40];
    snprintf(nm, sizeof nm, fmt, tier + 1, elem);
    int id = sora_sfx_id(nm);
    if (id >= 0) sora_sfx_play_at(id, 1.0f, pos);
}

// ---- Magnet
#define MG_PULL_N 4  // (s133 review) the exe has 8 enemy-velocity slots (linkpos.c) shared with the air-hold: Magnet takes at most 4 GUESS
typedef struct { uint64_t actor; float hold, swirl; bool on; } MgPull;
typedef struct {
    bool on, started_in_form;
    int tier, slot0;
    float t, c[3], q_last;
    MgFx fx_start, fx_sphere, fx_end;
    bool end_started, pull_open;
    uint64_t hit[32];
    int nhit, npulled, nunmov, nfull;
    MgPull pull[8];
    float next_diag;
    bool from_lock;
} MgMagnet;
static MgMagnet MGN;

static float mg_curve(float x) { x = x < 0 ? 0 : x > 1 ? 1 : x; return (sinf((x - 0.5f) * PI) + 1.0f) * 0.5f; }  // the script's scale curve
static uint32_t mg_rng = 0x12345678u;
static float mg_rand01(void) { mg_rng ^= mg_rng << 13, mg_rng ^= mg_rng >> 17, mg_rng ^= mg_rng << 5; return (float)(mg_rng & 0xffffff) / 16777216.0f; }

static bool mg_movable(uint64_t actor) {  // GUESS: KH2 pulls everything but bosses; here the levels table's size and the controller override must allow it
    for (int i = 0; i < SL_NC; i++)
        if (SL.c[i].used && SL.c[i].actor == actor && SL.c[i].named) return !(SL.c[i].flags & 1) && SL.c[i].pct && SL.c[i].pct <= g_mg_size;
    return true;
}

static void mg_pull_release(MgMagnet *m) {
    for (int i = 0; i < 8; i++)
        if (m->pull[i].on) {
            if (g_ah_evel) g_ah_evel(m->pull[i].actor, false, 0, 0, 0);
            m->pull[i].on = false;
        }
}

static void mg_sphere_set(MgMagnet *m, float s, float to_u) {  // GUESS: the exe has no effect_set_scale yet (exe need in the report): the sphere is respawned at stepped scales (1/8 steps)
    float q = floorf(s * 8.0f + 0.5f) / 8.0f;
    if (q == m->q_last) return;
    m->q_last = q;
    mg_kill(&m->fx_sphere);
    if (q <= 0) return;
    float c[3] = {m->c[0], m->c[1] + 0.01f * g_k, m->c[2]};  // the script lifts element 3 by 1 cm
    m->fx_sphere = mg_fx("magnet_sphere", m->tier, c, S.face, to_u * q);
}

static void mg_magnet_end(MgMagnet *m, const char *why) {
    if (!m->on) return;
    mg_pull_release(m);
    mg_kill(&m->fx_sphere);
    if (strcmp(why, "finished")) mg_kill(&m->fx_start), mg_kill(&m->fx_end);  // (the end burst plays out after the script ends; a cancel cuts it)
    LOG("[magic] magnet end (%s): %d hit, %d pulled, %d not movable, %d no free pull slot, fx live %d\n", why, m->nhit, m->npulled, m->nunmov, m->nfull, sora_fx_count());
    m->on = false;
}

static int mg_hit(const char *name, uint64_t actor, const float epos[3], const float from[3], int power, int atkp, int react, int kb1, int kb2, int f54, float to_u) {
    int d = sl_hit(actor, -1, power, g_dmg_base, 1);  // Mag-based like the other spells (levels: sl_hit magic 1)
    f54 = ah_hit_f54(actor, epos, f54, react, to_u);
    linkpos_damage_enemy_ex(actor, d, f54, 0);  // element 0: atkp Element 5 (neither Fire / Blizzard / Thunder)
    ah_hit(actor, epos, from, react, kb1, kb2, f54, to_u, name);
    dg_hit(atkp, actor);
    if (sora_fx_count() < 24) sora_fx_event(kbfx_name("hit"), epos, 0, to_u);  // (s133 review: no spark when the 32 slots are nearly full - a big burst evicted the barrier) GUESS: KH2 re:0x3D1B40 picks the held Keyblade's hit spark for any attack object's hit
    sora_sfx_hit(atkp, 1.0f, epos);
    LOG("[magic] %s HIT %llx: damage %d (atkp %d power %d) f54 %#x\n", name, (unsigned long long)actor, d, atkp, power, f54);
    return d;
}

static void mg_magnet_spawn(const float feet[3], float face, uint64_t target, float to_u) {
    MgMagnet *m = &MGN;
    mg_magnet_end(m, "recast");
    memset(m, 0, sizeof *m);
    m->on = true, m->tier = spell_tier(), m->q_last = -1, m->pull_open = true;
    m->fx_start = m->fx_sphere = m->fx_end = MG_NOFX;
    m->started_in_form = fm_in_form(), m->slot0 = SLS.slot;
    uint64_t actor[64];
    float pos[64][3], p[3];
    int32_t life[64];
    int n = linkpos_enemies(actor, pos, life, 64);
    bool lock = false;
    for (int i = 0; i < n && !lock; i++)
        if (target && actor[i] == target && life[i] > 0) memcpy(p, pos[i], sizeof p), lock = true;
    if (!lock) cast_tip(feet, to_u, p);  // KH2: element 1's position = the Keyblade tip (GUESS cast_tip, as the other casts)
    float h = MG_MAG_H[m->tier] * to_u, a[3] = {p[0], p[1] + h, p[2]}, b[3] = {p[0], p[1] - h, p[2]}, hit[3], nr[3];  // from (pos + h) down 2 h
    if (linkpos_raycast(a, b, hit, nr)) m->c[0] = hit[0], m->c[1] = hit[1] + h, m->c[2] = hit[2];
    else memcpy(m->c, p, sizeof m->c);  // the script keeps the target position when there is no ground
    m->from_lock = lock;
    m->fx_start = mg_fx("magnet_start", m->tier, m->c, face, to_u);
    mg_sfx_at("magnet_%d.%d", m->tier, 2, m->c);
    LOG("[magic] %s cast: centre %.2f %.2f %.2f (%s), height %d cm, radius %d cm, grow %d + hold %d + shrink %d frames, pull window %d frames (target %llx)\n",
        SP_TIER_NAME[SPELL_MAGNET][m->tier], m->c[0], m->c[1], m->c[2], lock ? "on the lock target" : "ahead of Sora", MG_MAG_H[m->tier],
        MG_MAG_R[m->tier], MG_MAG_GROW[m->tier], MG_MAG_HOLD[m->tier], MG_MAG_SHRINK, MG_MAG_WAIT + MG_MAG_GROW[m->tier] + MG_MAG_HOLD[m->tier],
        (unsigned long long)target);
}

static void mg_magnet_tick(float kh, float to_u) {
    MgMagnet *m = &MGN;
    if (!m->on) return;
    int T = m->tier;
    float grow = (float)MG_MAG_GROW[T], hold = (float)MG_MAG_HOLD[T];
    float t_grow = MG_MAG_WAIT, t_hold = t_grow + grow, t_pull_end = t_hold + hold, t_end = t_pull_end + MG_MAG_SHRINK;
    m->t += kh;
    // the sphere: grows from the 16th frame, holds, shrinks after the pull attack ends (element 4 starts there)
    if (m->t >= t_grow && m->t < t_pull_end) mg_sphere_set(m, mg_curve(m->t < t_hold ? (m->t - t_grow) / grow : 1.0f), to_u);
    if (m->t >= t_pull_end) {
        if (!m->end_started) {
            m->end_started = true;
            float c1[3] = {m->c[0], m->c[1] + 0.01f * g_k, m->c[2]};
            m->fx_end = mg_fx("magnet_end", T, c1, S.face, to_u);
            mg_sfx_at("magnet_%d.%d", T, 4, m->c);
        }
        float left = t_end - m->t;
        if (left > 0) mg_sphere_set(m, mg_curve(left / MG_MAG_SHRINK), to_u);
        else if (m->fx_sphere.h >= 0) mg_kill(&m->fx_sphere), m->q_last = 0;
    }
    if (m->pull_open && m->t > t_pull_end) {  // the script's attack_free: the held enemies are let go
        m->pull_open = false;
        mg_pull_release(m);
        LOG("[magic] magnet pull over at t %.0f: %d hit, %d pulled\n", m->t, m->nhit, m->npulled);
    }
    if (m->pull_open) {
        uint64_t actor[64];
        float pos[64][3];
        int32_t life[64];
        int n = linkpos_enemies(actor, pos, life, 64);
        float R = (MG_MAG_R[T] + MG_BODY_R) * to_u, up = MG_BODY_UP * to_u;
        for (int i = 0; i < n; i++) {
            if (life[i] <= 0) continue;
            float ec[3] = {pos[i][0], pos[i][1] + up, pos[i][2]};
            float dx = ec[0] - m->c[0], dy = ec[1] - m->c[1], dz = ec[2] - m->c[2];
            if (dx * dx + dy * dy + dz * dz > R * R) continue;
            bool seen = false;
            for (int k = 0; k < m->nhit && !seen; k++) seen = m->hit[k] == actor[i];
            if (seen) continue;
            if (m->nhit >= 32) continue;  // (s133 review: a full list must not re-hit the 33rd every frame)
            m->hit[m->nhit++] = actor[i];
            mg_hit("magnet", actor[i], pos[i], m->c, MG_MAG_POW, 658, 5, MG_PULL_KB1[T], MG_PULL_KB2[T], 0x0f, to_u);
            // the pull reaction: hold radius rolled once at the hit (re:0x40FBF0), the swirl from the row
            bool can = ah_cc_ok() && g_ah_evel && mg_movable(actor[i]) && !ah_find(actor[i]);
            if (!can) { m->nunmov++; LOG("[magic] magnet: %llx not pulled (%s): hit only\n", (unsigned long long)actor[i], !ah_cc_ok() || !g_ah_evel ? "this exe has no linkpos_enemy_vel" : !mg_movable(actor[i]) ? "too big / a boss" : "held by the air-hold"); continue; }
            int slot = -1;
            for (int k = 0; k < MG_PULL_N && slot < 0; k++) if (!m->pull[k].on) slot = k;
            if (slot < 0) { m->nfull++; continue; }
            m->pull[slot].on = true, m->pull[slot].actor = actor[i];
            m->pull[slot].hold = MG_PULL_KB1[T] + mg_rand01() * (MG_PULL_KB2[T] - MG_PULL_KB1[T]);
            m->pull[slot].swirl = (float)MG_PULL_SWIRL[T];
            m->npulled++;
            LOG("[magic] magnet: pulling %llx, hold radius %.0f cm, swirl %.0f cm/frame\n", (unsigned long long)actor[i], m->pull[slot].hold, m->pull[slot].swirl);
        }
        // one velocity per pulled enemy per frame (the controller override persists until set again / released)
        for (int k = 0; k < 8; k++) {
            MgPull *pl = &m->pull[k];
            if (!pl->on) continue;
            int i = 0;
            while (i < n && actor[i] != pl->actor) i++;
            if (i == n || life[i] <= 0 || ah_find(pl->actor)) {
                if (g_ah_evel && !ah_find(pl->actor)) g_ah_evel(pl->actor, false, 0, 0, 0);  // (s133 review: the air-hold owns it now - don't switch its override off)
                pl->on = false;
                continue;
            }
            float d[3] = {(m->c[0] - pos[i][0]) / to_u, (m->c[1] - pos[i][1]) / to_u, (m->c[2] - pos[i][2]) / to_u};  // KH cm, feet to the centre
            float dist = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            if (dist < 1e-3f) continue;
            float u[3] = {d[0] / dist, d[1] / dist, d[2] / dist};
            float v = (dist - pl->hold) * MG_PULL_RATE;
            v = v > MG_PULL_MAX ? MG_PULL_MAX : v < -MG_PULL_MAX ? -MG_PULL_MAX : v;
            float tl = sqrtf(u[0] * u[0] + u[2] * u[2]), tx = tl > 1e-4f ? u[2] / tl : 0, tz = tl > 1e-4f ? -u[0] / tl : 0;  // flat RotY(pi/2); GUESS handedness vs KH2's y-down axes
            float mv[3] = {u[0] * v + tx * pl->swirl, u[1] * v, u[2] * v + tz * pl->swirl};  // cm per KH frame
            float vel[3];
            for (int c = 0; c < 3; c++) {
                vel[c] = mv[c] * KH_FPS * to_u;
                vel[c] = fmaxf(-g_ah_vmax, fminf(g_ah_vmax, vel[c]));
            }
            g_ah_evel(pl->actor, true, vel[0], vel[1], vel[2]);
            if (g_mg_diag && m->t >= m->next_diag)
                LOG("[magic] magnet pull %llx: dist %.0f cm hold %.0f -> %.1f cm/frame, v %.1f %.1f %.1f u/s\n", (unsigned long long)pl->actor, dist, pl->hold, v, vel[0], vel[1], vel[2]);
        }
        if (m->t >= m->next_diag) m->next_diag = m->t + 30;
    }
    if (m->t >= t_end) mg_magnet_end(m, "finished");
}

// ---- Reflect
typedef struct { uint64_t actor; float next; } MgRehit;
typedef struct {
    int phase;  // 0 off, 1 barrier up, 2 burst A (Power 50 rows, re-hit by Interval), 3 burst B (Power 550, once)
    bool form0;
    int tier, btier, slot0, blocks, nhit;
    float t, bt;
    MgFx fx_barrier, fx_flash, fx_burst;
    MgRehit rh[32];
    int nrh;
    uint64_t last_block_frame;
} MgReflect;
static MgReflect MGR;

static bool mg_reflect_up(void) { return MGR.phase == 1; }  // sora_guard.h: while true every hit on Link is blocked (GUESS: all hits, as PHH's A36; KH2's sphere only takes those that enter it)

static void mg_reflect_end(MgReflect *r, const char *why) {
    if (!r->phase) return;
    mg_kill(&r->fx_barrier);
    mg_kill(&r->fx_flash);
    if (strncmp(why, "burst over", 10)) mg_kill(&r->fx_burst);  // (the burst element is 61-85 frames, the attacks 34: it plays out; a cancel cuts it)
    LOG("[magic] reflect end (%s): %d blocked, %d burst hits, fx live %d\n", why, r->blocks, r->nhit, sora_fx_count());
    r->phase = 0;
}

static void mg_reflect_spawn(const float feet[3], float face, float to_u) {
    MgReflect *r = &MGR;
    int carry = r->phase == 1 ? r->blocks : 0;  // the script's counter survives a re-cast while the barrier is up (wp[8] accumulates)
    if (r->phase == 1) { mg_kill(&r->fx_barrier), mg_kill(&r->fx_flash); r->phase = 0; }
    else mg_reflect_end(r, "recast");
    memset(r, 0, sizeof *r);
    r->phase = 1, r->tier = spell_tier(), r->blocks = carry, r->form0 = fm_in_form(), r->slot0 = SLS.slot;
    r->fx_barrier = r->fx_flash = r->fx_burst = MG_NOFX;
    r->fx_barrier = mg_fx("reflect_barrier", r->tier, feet, face, to_u);
    mg_sfx_at("reflect_%d.%d", r->tier, 0, feet);
    LOG("[magic] %s cast: barrier on Sora for %d frames, guard radius %d cm%s\n", SP_TIER_NAME[SPELL_REFLECT][r->tier], MG_REF_LIFE, MG_REF_GUARD[r->tier],
        carry ? " (blocked count carried over)" : "");
}

// sora_guard.h calls this for a hit the barrier swallowed (the exe drop / the life refund / a reflected beam)
static void mg_block(const float p[3], float to_u, int dmg, const char *how) {
    MgReflect *r = &MGR;
    if (r->phase != 1) return;
    r->blocks++;
    if (!mg_alive(&r->fx_flash)) {  // the script starts element 1 only when the last one is gone
        float f[3] = {p[0], p[1], p[2]};
        r->fx_flash = mg_fx("reflect_flash", r->tier, f, S.face, to_u);
        mg_sfx_at("reflect_%d.%d", r->tier, 1, f);
    }
    LOG("[magic] reflect BLOCK #%d (damage %d, %s) at %.3f %.3f %.3f\n", r->blocks, dmg, how, p[0], p[1], p[2]);
}

static bool mg_rehit_due(MgReflect *r, uint64_t actor, float iv) {  // an attack's per-target interval slot (re:0x3CF600)
    for (int i = 0; i < r->nrh; i++)
        if (r->rh[i].actor == actor) {
            if (r->bt < r->rh[i].next) return false;
            r->rh[i].next = r->bt + iv;
            return true;
        }
    if (r->nrh >= 32) return false;  // (s133 review: no slot = no re-hit timer: skip, else it hits every frame)
    r->rh[r->nrh].actor = actor, r->rh[r->nrh++].next = r->bt + iv;
    return true;
}

static void mg_reflect_burst_hits(MgReflect *r, const float feet[3], float to_u, bool second) {
    int B = r->btier, T = r->tier;
    float R = (MG_REF_R[B][T] + (second ? 20 : 0)) * to_u, H = (MG_REF_H[B] + (second ? 20 : 0)) * to_u;
    uint64_t actor[64];
    float pos[64][3];
    int32_t life[64];
    int n = linkpos_enemies(actor, pos, life, 64);
    for (int i = 0; i < n; i++) {
        float dx = pos[i][0] - feet[0], dz = pos[i][2] - feet[2], dy = pos[i][1] - feet[1];
        if (life[i] <= 0 || dx * dx + dz * dz > R * R || dy < -H || dy > 2 * H) continue;  // the cylinder convention of Fire's ring (r, -h .. 2h); GUESS for the vertical window
        if (second) {
            r->nhit++;
            int react = MG_REF_REACT_B[B];
            mg_hit("reflect", actor[i], pos[i], feet, MG_REF_POW_B, MG_REF_ROW_B[B], react, MG_REF_KB1_B[B], MG_REF_KB2_B[B], react == 7 ? 0x14 : 0x11, to_u);
        } else if (mg_rehit_due(r, actor[i], (float)MG_REF_IV[T])) {
            r->nhit++;
            mg_hit("reflect", actor[i], pos[i], feet, MG_REF_POW_A, MG_REF_ROW_A[B], MG_REF_REACT_A[B], MG_REF_KB_A[B], 0, MG_REF_REACT_A[B] == 4 ? 0x14 : 0x0f, to_u);
        }
    }
}

static void mg_reflect_tick(float kh, float to_u, const float feet[3]) {
    MgReflect *r = &MGR;
    if (!r->phase) return;
    float f0 = r->phase == 1 ? r->t : r->bt;
    if (r->phase == 1) {
        r->t += kh;
        mg_move(&r->fx_barrier, feet, S.face);  // bound to Sora (bone 0x4000)
        mg_move(&r->fx_flash, feet, S.face);
        if (g_mg_diag && (int)(r->t / 10) != (int)(f0 / 10)) LOG("[magic] reflect up: t %.0f / %d, %d blocked, guard r %d cm\n", r->t, MG_REF_LIFE, r->blocks, MG_REF_GUARD[r->tier]);
        if (r->t < MG_REF_LIFE) return;
        mg_kill(&r->fx_barrier), mg_kill(&r->fx_flash);
        if (r->blocks <= 0) { mg_reflect_end(r, "barrier over, nothing blocked: no burst"); return; }
        r->btier = r->blocks >= MG_REF_N3 ? 2 : r->blocks >= MG_REF_N2 ? 1 : 0;
        char base[24];
        snprintf(base, sizeof base, "reflect_burst%d", r->btier + 1);
        r->fx_burst = mg_fx(base, r->tier, feet, S.face, to_u);
        mg_sfx_at("reflect_%d.%d", r->tier, 4 + r->btier, feet);
        r->phase = 2, r->bt = 0, r->nrh = 0;
        LOG("[magic] reflect burst tier %d (%d blocked; >= %d tier 2, >= %d tier 3): radius %d cm height %d cm, rows %d then %d\n", r->btier + 1, r->blocks, MG_REF_N2, MG_REF_N3,
            MG_REF_R[r->btier][r->tier], MG_REF_H[r->btier], MG_REF_ROW_A[r->btier], MG_REF_ROW_B[r->btier]);
        mg_reflect_burst_hits(r, feet, to_u, false);
        return;
    }
    r->bt += kh;
    mg_move(&r->fx_burst, feet, S.face);
    if (r->phase == 2) {
        mg_reflect_burst_hits(r, feet, to_u, false);
        if (r->bt >= MG_REF_TA) r->phase = 3, mg_reflect_burst_hits(r, feet, to_u, true);
    } else if (r->bt >= MG_REF_TA + MG_REF_TB) {
        mg_reflect_end(r, "burst over");
    }
}

// ---- common
static void mg_cancel(const char *why) {  // load / KO / Drive Form / unload / switch off: every handle and enemy override freed
    if (MGN.on) mg_magnet_end(&MGN, why);
    if (MGR.phase) mg_reflect_end(&MGR, why);
}

static void mg_spawn(int kind, const float feet[3], float face, uint64_t target) {
    float to_u = 0.01f * g_k;
    if (kind == SPELL_MAGNET) mg_magnet_spawn(feet, face, target, to_u);
    else mg_reflect_spawn(feet, face, to_u);
}

static void mg_tick(float kh, float to_u, const float feet[3]) {
    if (!MGN.on && !MGR.phase) return;
    const char *why = NULL;
    bool form = fm_in_form();
    int slot = SLS.slot;
    if (linkpos_link_life() <= 0) why = "Sora KO";
    else if ((MGN.on && (MGN.started_in_form != form || MGN.slot0 != slot)) || (MGR.phase && (MGR.form0 != form || MGR.slot0 != slot))) why = form != (MGN.on ? MGN.started_in_form : MGR.form0) ? "Drive Form" : "slot change / load";
    else if (!g_mg_magnet && MGN.on) why = "magnet 0";
    else if (!g_mg_reflect && MGR.phase) why = "reflect 0";
    if (why) { mg_cancel(why); return; }
    mg_magnet_tick(kh, to_u);
    mg_reflect_tick(kh, to_u, feet);
}

static bool mg_live_set(const char *key, int v) {  // s133 magic
    if (!strcmp(key, "magnet")) { g_mg_magnet = v != 0; if (!v && MGN.on) mg_magnet_end(&MGN, "set magnet 0"); }
    else if (!strcmp(key, "reflect")) { g_mg_reflect = v != 0; if (!v && MGR.phase) mg_reflect_end(&MGR, "set reflect 0"); }
    else if (!strcmp(key, "spdiag")) g_mg_diag = v != 0;
    else if (!strcmp(key, "mgsize")) g_mg_size = v;
    else return false;
    LOG("[magic] live %s = %d (magnet %d, reflect %d, spdiag %d, mgsize %d)\n", key, v, g_mg_magnet, g_mg_reflect, g_mg_diag, g_mg_size);
    return true;
}
