// (s291 dueltests) ONE stubbed game around the real runtime/sora_duel.h (+ sora_net_duel.h), for runtime/test/duelglue_test.c.
// Compiled once per game: duelglue_game_a.c / duelglue_game_b.c define DG_SELF (DGA / DGB) and include this, so each game keeps its
// own DU / DUS / NETD / NET (all static). No game, no socket: NET is a plain struct with real NetPeerSlot slots (sora_net_peer.h),
// net_send_to keeps the bytes (the test hands them to the other game's net_duel_recv), Link's life sits in a host buffer at
// +0x848 (du_life_set writes it there), PAUSE_STATE is a host word, the fx / sfx / menus / Sora's state are no-ops or plain fields.
// Tests only: nothing of sora_duel.h is changed or redefined.
#ifndef DG_SELF
#error "define DG_SELF (DGA / DGB) before including duelglue_game.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <windows.h>
#include "sora_net_peer.h"
#include "duelglue_api.h"

// ---- the log (one per game) ----
static char g_log[65536];
static void test_log(const char *f, ...) {
    va_list a;
    va_start(a, f);
    size_t n = strlen(g_log);
    if (n < sizeof g_log - 1) vsnprintf(g_log + n, sizeof g_log - n, f, a);
    va_end(a);
}
#define LOG test_log

// ---- stubs, part 1: sora_net.h's NET + the send (sora_net_duel.h needs NET, net_send_to, LOG, NetLogRate) ----
static struct {
    bool on, melty, driven, dead, downed;
    uint8_t me;
    NetPeerSlot slot[NET_MAX_PEERS];
} NET;
#define TX_MAX 512
static DgPkt g_tx[TX_MAX];
static int g_ntx;
static void net_send_to(uint32_t ip, uint16_t port, const uint8_t *b, int n) {
    if (g_ntx >= TX_MAX || n < 0 || n > (int)sizeof g_tx[0].b) return;
    memcpy(g_tx[g_ntx].b, b, (size_t)n), g_tx[g_ntx].n = n, g_tx[g_ntx].ip = ip, g_tx[g_ntx].port = port;
    g_ntx++;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#include "sora_net_duel.h"

// ---- stubs, part 2: the game (sora.c and the headers sora_duel.h names at its top); no-ops unless a test reads them ----
#define PI 3.14159265358979f
#define KH_FPS 60.0f
#define KH_LOCK_RANGE 2000.0f
#define KH_STICK_DZ 30.0f
static float g_body[3];
static float g_k = 1.0f;
static bool g_on = true, g_loading, g_av_local_ok = true, g_guard = true, g_react, g_air_magic, g_mg_reflect, g_mg_magnet;
static bool g_guard_limit, g_guard_gate, g_abilfix, g_dbgall, g_fmots_on, g_kbthrow, g_dbm_clip, g_stick_round, g_mkpane, g_gl_tail;
static int g_wstick = -1, g_rollinv, g_kt_mp, gl_from_motion, g_hud_life_max = 48, sl_khdiff = 1;
static int g_rc_inv;  // (a Dodge Roll / Reaction Dodge's invulnerable frames)
static int32_t g_lx, g_ly;
static uint64_t g_ad_hold_ms;
static bool g_spark_at;
static float g_sweep_hit[3];
static volatile uint32_t g_pause;  // PAUSE_STATE: 0 = BotW runs, else its pause / menu
#define PAUSE_STATE ((uintptr_t)&g_pause)
// Sora's state (sora.c S) / Guard (sora_guard.h G) / MP / the party / menus
enum { ACT_IDLE = 0, ACT_JUMP = 1 };
#define ACT_AIR(a) ((a) == ACT_JUMP)
enum { SP_NONE = 0, SP_ATTACK, SP_MAGIC, SP_DAMAGE, SP_GUARD, SP_ROLL };
enum { MOT_GUARD = 90, MOT_ROLL = 91, MOT_QRUN = 92 };
static struct { int act, air_frames, motion, mot, swing, special, takeoff, d74, d75; float clock, face, vel[2]; bool req_set, hit; } S;
static struct { int inv; } G;
static struct { float mp, mp_max; int charge, charge_total; } MP = {100, 100, 0, 0};
static struct { bool on; } P;
static struct { int sub, dead; bool shortcut; } M = {-1, 0, false};
static struct { bool on; } MC;
enum { DR_OFF = 0 };
static struct { int st; } DR;
static struct { bool riding; } SKT;
static struct { bool on; } TITLE;
static struct { int shown_kind; } KDG;
enum { REACT_NONE = 0 };
static struct { int kind; } REACT;
static struct { int spell; float cast_at; } C;
static struct { uint32_t on; } AB;
static struct { bool on; } LM;
enum { KT_AIM = 1 };
static struct { int ph; } KT;
static struct { bool on; } GT;
static struct { int def; } g_wpn_cur;
#define IN_MICKEY 0
#define IN_LIMIT 0
#define IN_GUEST 0
#define ROXCMD 0
#define MKCMD 0
enum { BTN_ROLL = 1, BTN_CLIMB = 2, BTN_JUMP = 4, BTN_THROW = 8, BTN_SHORTCUT = 16 };
enum { CMD_FORM = 100, FM_NFORMS = 6, CMD_LIMIT = 120, LM_N = 4, CMD_PLIMIT = 130, PL_NLIM = 3, CMD_SUMMON_CAST = 140, CMD_SUMMON_FPS = 145 };
enum { AB_GUARD = 1, AB_FIRE_BOOST = 2, AB_ELEMENT_UP = 20 };
enum { LINK_GROUND = 0 };
enum { SPELL_FIRE = 0, SPELL_BLIZZARD, SPELL_THUNDER, SPELL_CURE, SPELL_MAGNET, SPELL_REFLECT, NSPELL };
enum { ATKP_KIND_NORMAL = 0 };
typedef struct { const char *name; } KhSpell;
static const KhSpell KH_SPELLS[NSPELL] = {{"Fire"}, {"Blizzard"}, {"Thunder"}, {"Cure"}, {"Magnet"}, {"Reflect"}};
static const char *const SP_TIER_NAME[NSPELL][3] = {{"a", "b", "c"}, {"a", "b", "c"}, {"a", "b", "c"}, {"a", "b", "c"}, {"a", "b", "c"}, {"a", "b", "c"}};
static const int MG_MAG_R[3] = {250, 500, 800}, MG_PULL_KB1[3] = {40, 70, 100}, MG_PULL_KB2[3] = {120, 170, 220}, MG_PULL_SWIRL[3] = {-3, -6, -12};
static const int MG_REF_IV[3] = {10, 8, 6}, MG_REF_POW_A = 50, MG_REF_POW_B = 550;
static const int MG_REF_ROW_A[3] = {1, 2, 3}, MG_REF_ROW_B[3] = {4, 5, 6}, MG_REF_REACT_A[3] = {3, 3, 3}, MG_REF_REACT_B[3] = {8, 8, 8};
typedef struct { float c[3]; bool du_hit; } MgMagnet;
typedef struct { int power, reaction, atkp, kind; float pad[4]; } HitWin;
typedef struct { int lv, str, mag, def; } SlLevel;
static const SlLevel SL_LV[1] = {{1, 12, 8, 13}};
#define SL_KEY_STR 3
#define SL_KEY_MAG 1
static int sl_lv(void) { return 1; }
static int sl_str_of(const SlLevel *l) { return l->str; }
static int sl_mag_of(const SlLevel *l) { return l->mag; }
typedef struct { float x[4], y[4]; uint32_t col[4]; int tex; } SoraHudQuad;
typedef struct { SoraHudQuad *q; int n, cap; } CsOut;
static struct { int seq; bool tried, ok, drew; } CAMP;
static void camp_load(void) { CAMP.tried = true; }
static uint8_t cs_ascii(const int *seq, char c) { (void)seq; return (uint8_t)c; }
static void cs_text(const int *seq, CsOut *o, const uint8_t *m, int n, float x, float y, float sc, const float *tc, float *w, float *h) {
    (void)seq, (void)o, (void)m, (void)n, (void)x, (void)y, (void)sc, (void)tc;
    if (w) *w = 0;
    if (h) *h = 0;
}
// Link's actor: life at +0x848, in a host buffer (du_life_set writes it there)
static uint8_t g_link[0x1000];
static bool g_link_ok = true;
static uint64_t link_actor(void) { return g_link_ok ? (uint64_t)(uintptr_t)g_link : 0; }
static int linkpos_link_life(void) { return g_link_ok ? *(int32_t *)(g_link + 0x848) : -1; }
static void life_set(int v) { *(int32_t *)(g_link + 0x848) = v; }
static bool rd_ok(uint64_t a, size_t n) { uint64_t lo = (uint64_t)(uintptr_t)g_link; return a >= lo && a + n <= lo + sizeof g_link; }
static int link_heal(int v) { (void)v; life_set(g_hud_life_max); return g_hud_life_max; }
static int linkpos_link_hits(void) { return 0; }
static int linkpos_link_ragdoll(void) { return 0; }
static int linkpos_link_state(void) { return LINK_GROUND; }
static bool linkpos_raycast(const float a[3], const float b[3], float h[3], float n[3]) { (void)a, (void)b, (void)h, (void)n; return false; }
static int linkpos_enemies(uint64_t *a, float (*p)[3], int32_t *l, int max) { (void)a, (void)p, (void)l, (void)max; return 0; }
static bool linkpos_enemy_name(uint64_t a, char *nm, int n) { (void)a, (void)nm, (void)n; return false; }
static void linkpos_set_velocity(bool on, float vx, float vz) { (void)on, (void)vx, (void)vz; }
static void linkpos_set_velocity_y(bool on, float vy) { (void)on, (void)vy; }
static void linkpos_camera_fwd(float f[2]) { (void)f; }
static void link_face(void) {}
// the four block reasons du_take_hit reads (each test turns one on)
static bool g_t_guard_win, g_t_reflect, g_t_limit, g_t_adodge;
static bool guard_window_kh(void) { return g_t_guard_win; }
static bool mg_reflect_up(void) { return g_t_reflect; }
static bool pl_invuln(void) { return g_t_limit; }
static bool ad_window(void) { return g_t_adodge; }
static int g_nguard_block, g_nad_block;
static void guard_block(const float p[3], float to_u, int dmg, const char *what, bool win) { (void)p, (void)to_u, (void)dmg, (void)what, (void)win; g_nguard_block++; }
static void ad_block(const float p[3], float to_u, int dmg) { (void)p, (void)to_u, (void)dmg; g_nad_block++; }
static int g_nparty_off;
static void party_all_off(void) { g_nparty_off++; }
static bool fm_in_form(void) { return false; }
static bool fm_no_magic(void) { return false; }
static bool sm_out(void) { return false; }
static bool mp_charging(void) { return false; }
static bool berserk_on(void) { return false; }
static int ab_finisher_dmg(int kind, int v) { (void)kind; return v; }
static bool ab_on(uint32_t on, int id) { (void)on, (void)id; return false; }
static void ah_atkp(int atkp, int *react, int *kb1, int *kb2) { (void)atkp, (void)react, (void)kb1, (void)kb2; }
static float mg_rand01(void) { return 0.5f; }
static bool mg_owned(int sp) { (void)sp; return true; }
static int spell_state(int sp) { (void)sp; return 0; }
static bool mot_of(int m) { (void)m; return false; }
static int mot_st(int m) { return m; }
static void fall_start(int a, float h) { (void)a, (void)h; }
static void fall_motion(int m) { (void)m; }
static bool pl_hold(void) { return false; }
static bool rc_eat_x(void) { return false; }
static bool pl_eat_x(void) { return false; }
static bool skt_eat_x(void) { return false; }
static bool sm_fps_eat_x(void) { return false; }
static void sm_fps_view(float f[2]) { (void)f; }
static void np_stick_for(float face, const float cf[2], float a, float st[2]) { (void)face, (void)cf, (void)a, st[0] = st[1] = 0; }
static int kh_axis(int32_t v) { (void)v; return 0; }
static int qr_lv(void) { return 0; }
static bool roll_learned(void) { return false; }
static bool rcs_show(const char *t, bool b) { (void)t, (void)b; return true; }
static const char *kbfx_name(const char *n) { return n; }
static void kbse_hit(int atkp, float v, const float p[3]) { (void)atkp, (void)v, (void)p; }
static bool kbreach_hit(int blade, int swing, const HitWin *w, uint64_t tag, const float *a, const float *b, const float *ep, float r, float h, float wr, float tu, bool bw) {
    (void)blade, (void)swing, (void)w, (void)tag, (void)a, (void)b, (void)ep, (void)r, (void)h, (void)wr, (void)tu, (void)bw;
    return false;
}
static bool wall_hides(const float e[3]) { (void)e; return false; }
static void sora_sfx_event(const char *n, float v) { (void)n, (void)v; }
static void sora_sfx_hit(int atkp, float v, const float p[3]) { (void)atkp, (void)v, (void)p; }
// the fx: handles 0..31 with a generation; kill / spawn counted
static uint32_t g_fx_gen[32];
static bool g_fx_live[32];
static int g_nfx_spawn, g_nfx_kill;
static bool fx_mine(int h, uint32_t gen) { return h >= 0 && h < 32 && g_fx_live[h] && g_fx_gen[h] == gen; }
static bool sora_fx_alive(int h) { return h >= 0 && h < 32 && g_fx_live[h]; }
static int sora_fx_id(const char *n) { (void)n; return 7; }
static int sora_fx_count(void) { return 0; }
static void sora_fx_event(const char *n, const float p[3], float yaw, float sc) { (void)n, (void)p, (void)yaw, (void)sc; }
static int sora_fx_spawn(int id, const float c[3], float yaw, float sc) {
    (void)id, (void)c, (void)yaw, (void)sc;
    for (int h = 0; h < 32; h++)
        if (!g_fx_live[h]) { g_fx_live[h] = true, g_fx_gen[h]++, g_nfx_spawn++; return h; }
    return -1;
}
static void sora_fx_kill(int h) { if (h >= 0 && h < 32 && g_fx_live[h]) g_fx_live[h] = false, g_nfx_kill++; }
// (s291 duelride) sora.c's g_link_rides + sora_net_horse.h's nh_rides / nh_puppet_near: nobody rides, no horse puppet shown
static volatile int g_link_rides;
static bool nh_rides(int k, uint64_t now) { (void)k, (void)now; return false; }
static float nh_puppet_near(const float at[3]) { (void)at; return -1.0f; }


#include "sora_duel.h"
#pragma clang diagnostic pop

// ---- the exported game ----
static void dg_reset(uint8_t me) {
    memset(&DU, 0, sizeof DU);
    DU.k = -1, DU.ring_fx = -1, DU.fake_mark_fx = -1, DU.hits_seen = -1, DU.hit_swing = -1, DU.last_digit = -9;  // (= its initializer)
    memset(&DUS, 0, sizeof DUS), memset(&NETD, 0, sizeof NETD), memset(&DMP, 0, sizeof DMP), memset(&NET, 0, sizeof NET);
    NET.on = true, NET.melty = true, NET.driven = true, NET.me = me;  // (a Melty room: events once, no UDP resends)
    g_duel = true, g_duel_log = false, g_du_limit_s = 180, g_du_near_m = DU_NEAR_M, g_du_ring_m = DU_RING_M, g_du_enemy = 1;
    g_on = true, g_loading = false, TITLE.on = false, P.on = false, g_guard = true;
    g_body[0] = g_body[1] = g_body[2] = 0, S.face = 0;
    g_hud_life_max = 48, life_set(40), g_pause = 0;
    g_t_guard_win = g_t_reflect = g_t_limit = g_t_adodge = false, G.inv = 0, g_rc_inv = 0;
    g_nguard_block = g_nad_block = g_nparty_off = 0;
    memset(g_fx_live, 0, sizeof g_fx_live), g_nfx_spawn = g_nfx_kill = 0;
    g_ntx = 0, g_log[0] = 0;
}
static void dg_peer(int k, uint8_t player, const char *name, const float feet[3], int st, bool hidden, uint16_t flags, int hp_pct, uint64_t now) {
    NetPeerSlot *s = &NET.slot[k];
    s->used = true, s->ip = 0x0A000002u + (uint32_t)k, s->port = 5000, s->hidden = hidden;
    s->p.st = (PeerState)st, s->p.player = player;
    snprintf(s->p.name, sizeof s->p.name, "%s", name);
    memset(&s->p.ring[0], 0, sizeof s->p.ring[0]);
    s->p.ring[0].player = player, s->p.ring[0].flags = flags, s->p.ring[0].hp = (uint8_t)hp_pct;
    memcpy(s->p.ring[0].feet, feet, sizeof s->p.ring[0].feet);
    s->p.n = 1, s->p.at[0] = now, s->p.last_ms = now;
}
static void dg_no_pose(int k) { NET.slot[k].p.n = 0; }
static void dg_body(float x, float y, float z) { g_body[0] = x, g_body[1] = y, g_body[2] = z; }
static void dg_life(int v) { life_set(v); }
static void dg_pause(bool on) { g_pause = on ? 1u : 0u; }
static void dg_block(int what, bool on) {
    if (what == DGB_GUARD) g_t_guard_win = on;
    else if (what == DGB_REFLECT) g_t_reflect = on;
    else if (what == DGB_INV) G.inv = on ? 40 : 0;
    else if (what == DGB_RCINV) g_rc_inv = on ? 20 : 0;
    else if (what == DGB_ADODGE) g_t_adodge = on;
    else if (what == DGB_LIMIT) g_t_limit = on;
}
static bool dg_live_key(const char *key, int v) { return du_live_key(key, v); }
static void dg_ask(int k, uint64_t now) { du_ask(k, now); }
static void dg_accept(uint64_t now) { du_accept(now); }
static void dg_on_msg(int k, const NetDuelMsg *m, uint64_t now) { du_on_msg(k, m, now); }
static void dg_recv(int k, const uint8_t *b, int n, uint64_t now) { net_duel_recv(k, b, n, now); }
static void dg_frame(uint64_t now) { du_frame(now); }
static void dg_input(uint64_t *press, uint64_t *held, bool can) { du_input(press, held, can); }
static bool dg_spell(int kind, int power, const float c[3], float r, float lo, float hi) { return du_spell(kind, power, c, r, lo, hi); }
static void dg_idle_long(void) { DU.idle_since = 1; }
static DgView dg_view(void) {
    DgView v;
    memset(&v, 0, sizeof v);
    v.phase = DU.sm.phase, v.role = DU.sm.role, v.result = DU.sm.result, v.reason = DU.sm.reason, v.k = DU.k, v.prompt = DU.prompt;
    v.prompt_k = DU.prompt_k, v.id = DU.sm.id, v.nblocked = DU.nblocked, v.nhits_taken = DU.nhits_taken;
    v.nguard_block = g_nguard_block, v.nad_block = g_nad_block, v.life = linkpos_link_life();
    snprintf(v.banner, sizeof v.banner, "%s", DU.banner);
    for (int i = 0; i < NET_MAX_PEERS; i++) v.others_on += DU.other[i].on;
    for (int h = 0; h < 32; h++) v.fx_live += g_fx_live[h];
    v.nfx_spawn = g_nfx_spawn, v.nfx_kill = g_nfx_kill, v.live = DUS.live;
    v.my_time = DU.sm.my_time, v.his_time = DU.sm.his_time, v.my_pm = DU.sm.my_pm, v.his_pm = DU.sm.his_pm;
    return v;
}
static int dg_ntx(void) { return g_ntx; }
static const DgPkt *dg_tx(int i) { return i >= 0 && i < g_ntx ? &g_tx[i] : NULL; }
static void dg_tx_clear(void) { g_ntx = 0; }
static const char *dg_log(void) { return g_log; }
static void dg_log_clear(void) { g_log[0] = 0; }

const DgGame DG_SELF = {DG_NAME, dg_reset, dg_peer, dg_no_pose, dg_body, dg_life, dg_pause, dg_block, dg_live_key, dg_ask, dg_accept,
                        dg_on_msg, dg_recv, dg_frame, dg_input, dg_spell, dg_idle_long, dg_view, dg_ntx, dg_tx, dg_tx_clear,
                        dg_log, dg_log_clear};
