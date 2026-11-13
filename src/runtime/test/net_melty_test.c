// host test for the Melty Net room path of the glue in runtime/sora_net.h (s187/s188): a fake room stands in for
// runtime/melty/sora_melty.cpp (the static sora_melty_* below record what the glue sends and let the test script joins,
// leaves and incoming packets). The game parts are stubbed as in net_glue_test.c.
//   Approach: (a) one NET against a scripted fake room (the other players' packets are made with sora_net_proto.h's pack
//   functions) for every case, plus (b) one two-copy test that swaps the whole glue state (NET, NETED, NETF, NETES and the
//   fake room) between two "games" and routes each one's room sends to the other.
//   Windows: clang -O1 -g -Wall -Wextra -DSORA_MELTY_FAKE -Iruntime runtime/test/net_melty_test.c -o build_h/net_melty_test.exe -lws2_32
//   UBSan:   clang -O1 -g -Wall -Wextra -DSORA_MELTY_FAKE -fsanitize=undefined,bounds -fsanitize-trap=all -Iruntime runtime/test/net_melty_test.c -o build_h/net_melty_test_ubsan.exe -lws2_32
//            (trap mode: no runtime library needed. -fsanitize=address needs MSYS2's compiler-rt package, not installed here;
//            on Linux / with it: -fsanitize=address,undefined -fno-omit-frame-pointer)
//   Linux:   the same without -lws2_32
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
#include "sora_win.h"  // (winsock2 + windows.h, before the socket header, as in the DLL)
#else
#include <time.h>
typedef union { long long QuadPart; } LARGE_INTEGER;
static void QueryPerformanceCounter(LARGE_INTEGER *t) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); t->QuadPart = ts.tv_sec * 1000000000LL + ts.tv_nsec; }
static unsigned long long GetTickCount64(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec * 1000ull + ts.tv_nsec / 1000000; }
#endif
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-parameter"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)
static char g_log[65536];
static void test_log(const char *f, ...) {
    va_list a; va_start(a, f);
    size_t n = strlen(g_log);
    if (n > sizeof g_log - 1024) g_log[0] = 0, n = 0;  // (the fuzz logs a lot: start over rather than cut)
    vsnprintf(g_log + n, sizeof g_log - n, f, a);
    va_end(a);
}
#define LOG test_log
static int count_str(const char *hay, const char *needle) {
    int c = 0;
    for (const char *p = strstr(hay, needle); p; p = strstr(p + 1, needle)) c++;
    return c;
}

// ---- stubs for sora.c / sora_fx.h / sora_net_avatar.h (as net_glue_test.c)
static uint32_t g_fx_gen[32];
static bool g_fx_live[32];
static int g_kills[32], g_nkill;
static void sora_fx_kill(int h) { g_kills[h]++, g_nkill++, g_fx_live[h] = false; }
static bool sora_fx_alive(int h) { return h >= 0 && h < 32 && g_fx_live[h]; }
static inline bool fx_mine(int h, uint32_t gen) { return h >= 0 && h < 32 && g_fx_gen[h] == gen; }
static void avatar_diag(void) {}
static void avatar_off(void) {}
static int g_slot_off[8];
static void avatar_slot_off(int k) { g_slot_off[k]++; }
static void net_party_slot_off(int k) { (void)k; }
static void net_party_off(void) {}
static void net_party_diag(void) {}
static int g_drops;
static void av_drop_events(void);
#include "sora_net_proto.h"
static int g_nfr, g_fr_ret;
static int fire_world_remote(const float c[3], bool last, bool burst, float r, float h, int who, int res[3]) {
    g_nfr++;
    res[0] = 1, res[1] = 0, res[2] = 0;
    return g_fr_ret;
}
static float g_yaw = 1.25f;  // what net_fill puts in our SNAP (the where() heading)
static void net_fill(NetSnap *s);
#define NET_EDMG_NO_EXE
#define NET_ESYNC_NO_EXE
#define ESA_NO_EXE
#define SORA_CLOCK_NO_EXE  // (s191: the test sets the room clock's hour hooks itself: a fake game per copy)
#include "sora_net.h"
static void net_fill(NetSnap *s) { s->yaw = g_yaw; }
static void av_drop_events(void) {
    g_drops++;
    for (int k = 0; k < NET_MAX_PEERS; k++) net_slot_drop(k);
}

// ---- the fake Melty room (SORA_MELTY_FAKE: sora_net_sock.h declares these static; one struct, easy to extend) ----
#define FK_CAP 2048    // biggest packet the test hands the glue
#define FK_IN_MAX 256  // incoming queue
#define FK_OUT_MAX 512 // sends recorded (counted past this, not stored)
typedef struct { uint16_t from; int len; uint8_t b[FK_CAP]; } FkIn;
typedef struct { int dest, len, type, player, where_n; uint8_t b[NET_PKT_MAX + 64]; } FkOut;
static struct {
    int start_ret, start_n; char folder[64], game[64], name[64];
    int connected, self, state;              // state: what sora_melty_state() returns (2 = in the room)
    int frame_n, where_n, recv_n, stop_n;
    float where[4]; char area[32];
    FkOut out[FK_OUT_MAX]; int nout, nout_all, send_ret, send_big, send_nosn;  // send_ret < 0: taken (returns len)
    FkIn in[FK_IN_MAX]; int nin, rin;
    sora_melty_peer_event ev[64]; int nev, rev;
    char logq[8][128]; int nlog, rlog;
    // (s209) the lab notes' calls: setup (and whether the start had come yet), frame, puppet, forget
    int ad_setup_n, ad_setup_after_start, ad_frame_n, ad_puppet_n, ad_forget_n;
    char ad_addon[64], ad_ver[64], ad_pay[32]; float ad_m, ad_dt; int ad_flags, ad_on, ad_drawn, ad_anim, ad_room;
} FK;
static int g_hello_bye_ever;  // HELLO / BYE that ever reached sora_melty_send (never reset: checked at the end)

static int sora_melty_start(const char *folder, const char *game, const char *name) {
    FK.start_n++;
    snprintf(FK.folder, sizeof FK.folder, "%s", folder ? folder : "");
    snprintf(FK.game, sizeof FK.game, "%s", game ? game : "");
    snprintf(FK.name, sizeof FK.name, "%s", name ? name : "");
    return FK.start_ret;
}
static void sora_melty_frame(void) { FK.frame_n++; }
static void sora_melty_where(float x, float y, float z, float yaw, const char *area) {
    FK.where_n++, FK.where[0] = x, FK.where[1] = y, FK.where[2] = z, FK.where[3] = yaw;
    snprintf(FK.area, sizeof FK.area, "%s", area ? area : "(null)");
}
static int sora_melty_send(int peer, const void *packet, int len) {
    const uint8_t *p = (const uint8_t *)packet;
    FK.nout_all++;
    if (len > 1024) FK.send_big++;
    if (len < NET_HDR || p[0] != 'S' || p[1] != 'N') FK.send_nosn++;
    int type = len > 3 ? p[3] : -1;
    if (type == NET_HELLO || type == NET_BYE) g_hello_bye_ever++;
    if (FK.nout < FK_OUT_MAX) {
        FkOut *o = &FK.out[FK.nout++];
        o->dest = peer, o->len = len, o->type = type, o->player = len > 4 ? p[4] : -1, o->where_n = FK.where_n;
        memcpy(o->b, p, (size_t)(len < (int)sizeof o->b ? len : (int)sizeof o->b));
    }
    return FK.send_ret < 0 ? len : FK.send_ret;
}
// as the real one: a packet bigger than the caller's buffer is dropped (returns 0), a 0-length one reads as "none"
static int sora_melty_recv(uint16_t *from, void *buffer, int capacity) {
    FK.recv_n++;
    if (FK.rin >= FK.nin) { FK.rin = FK.nin = 0; return 0; }
    FkIn *p = &FK.in[FK.rin++];
    if (p->len > capacity) return 0;
    if (p->len > 0) memcpy(buffer, p->b, (size_t)p->len);
    if (from) *from = p->from;
    return p->len;
}
static int sora_melty_peer(sora_melty_peer_event *out) {
    if (FK.rev >= FK.nev) { FK.rev = FK.nev = 0; return 0; }
    *out = FK.ev[FK.rev++];
    return 1;
}
static int sora_melty_self(void) { return FK.self; }
static int sora_melty_connected(void) { return FK.connected; }
static __attribute__((unused)) int sora_melty_state(void) { return FK.state; }  // (the main session's upcoming glue asks this)
static int sora_melty_log_next(char *line, int capacity) {
    if (FK.rlog >= FK.nlog) { FK.rlog = FK.nlog = 0; return 0; }
    snprintf(line, (size_t)capacity, "%s", FK.logq[FK.rlog++]);
    return 1;
}
static void sora_melty_stop(void) { FK.stop_n++; }
static void sora_melty_addon_setup(const char *addon, const char *version, const char *payload_format, float metres_per_unit) {
    FK.ad_setup_n++, FK.ad_setup_after_start += FK.start_n > 0, FK.ad_m = metres_per_unit;
    snprintf(FK.ad_addon, sizeof FK.ad_addon, "%s", addon ? addon : "(null)");
    snprintf(FK.ad_ver, sizeof FK.ad_ver, "%s", version ? version : "(null)");
    snprintf(FK.ad_pay, sizeof FK.ad_pay, "%s", payload_format ? payload_format : "(null)");
}
static void sora_melty_addon_frame(float dt, int flags, int on_screen, int drawn, int animated, int room_drawn) {
    FK.ad_frame_n++, FK.ad_dt = dt, FK.ad_flags = flags, FK.ad_on = on_screen, FK.ad_drawn = drawn, FK.ad_anim = animated, FK.ad_room = room_drawn;
}
static void sora_melty_addon_puppet(int peer, float x, float y, float z, int moving, float behind_ms, float delay_ms) { FK.ad_puppet_n++; }
static void sora_melty_addon_forget(int peer) { FK.ad_forget_n++; }
// (s209 step 2) the notes as they happen: BEAT's what + detail recorded; the rest counted (the glue in sora_net.h sends BEATs;
// ACT / PLAYED / HURT / autopilot are sora.c's and sora_net_avatar.h's: runtime/test/net_note_test.c runs their cores)
static int g_beat_n, g_act_n, g_played_n, g_hurt_n;
static char g_beat_what[8][24], g_beat_detail[8][64];
static void sora_melty_addon_act(const char *action, int phase, const char *detail) { g_act_n++; }
static void sora_melty_addon_played(int peer, const char *action, int result, const char *what) { g_played_n++; }
static void sora_melty_addon_hurt(int from, float amount, const char *how) { g_hurt_n++; }
static void sora_melty_addon_beat(const char *what, int value, const char *detail) {
    if (g_beat_n < 8) {
        snprintf(g_beat_what[g_beat_n], sizeof g_beat_what[0], "%s", what ? what : "");
        snprintf(g_beat_detail[g_beat_n], sizeof g_beat_detail[0], "%s", detail ? detail : "");
    }
    g_beat_n++;
}
static int sora_melty_addon_pilot_on(void) { return 0; }
static int sora_melty_addon_take_pilot(sora_melty_pilot *out) { return 0; }
static void sora_melty_addon_report_pilot(int number, int state, float distance) {}
static void sora_melty_addon_steer_target(float x, float y, float z, float arrive_radius) {}
static void sora_melty_addon_steer_stop(void) {}
static void sora_melty_addon_steer_update(float dt, float x, float y, float z, sora_melty_steer *out) { memset(out, 0, sizeof *out); }

// ---- (s191) the fake room's story: ONE for every copy (never swapped by use()), the copy asking = FK.self ----
// Members in join order; the first one leads (the room: the host, then the longest here). A leader's change goes to every
// member that has the story, the leader too (GUESS: the real room echoes it; sora_clock.h does not care). A member that joins
// has story_ready 0 until fst_deliver() hands it every key (the snapshot). A guest's trigger goes to the leader's queue.
#define FST_Q 1024
typedef struct { char key[64]; uint8_t v[64]; int len, del; } FstChg;
typedef struct { uint16_t from; char key[64]; uint8_t a[64]; int len; } FstTrig;
typedef struct { uint16_t id; bool in, ready; FstChg q[FST_Q]; int nq, rq; FstTrig t[16]; int nt, rt; } FstMem;
static struct {
    FstMem m[8];
    uint16_t order[8]; int nm;
    struct { char key[64]; uint8_t v[64]; int len; } kv[16]; int nkv;
    double now;
    int calls, sets, clears, triggers;
} FST;
static FstMem *fst_mem(uint16_t id) {
    for (int i = 0; i < 8; i++) if (FST.m[i].in && FST.m[i].id == id) return &FST.m[i];
    return NULL;
}
static FstMem *fst_me(void) { return FK.self ? fst_mem((uint16_t)FK.self) : NULL; }
static uint16_t fst_leader(void) {
    for (int i = 0; i < FST.nm; i++) { FstMem *m = fst_mem(FST.order[i]); if (m && m->ready) return m->id; }
    return 0;
}
static void fst_push(FstMem *m, const char *key, const void *v, int len, int del) {
    if (m->nq >= FST_Q) { printf("  (fake story queue full)\n"); return; }
    FstChg *c = &m->q[m->nq++];
    snprintf(c->key, sizeof c->key, "%s", key);
    c->len = len < 0 ? 0 : len < 64 ? len : 64, c->del = del;
    if (c->len) memcpy(c->v, v, (size_t)c->len);
}
static void fst_all(const char *key, const void *v, int len, int del) {
    for (int i = 0; i < FST.nm; i++) { FstMem *m = fst_mem(FST.order[i]); if (m && m->ready) fst_push(m, key, v, len, del); }
}
static void fst_reset(void) { memset(&FST, 0, sizeof FST); FST.now = 5e6; }
static void fst_join(uint16_t id) {
    for (int i = 0; i < 8; i++)
        if (!FST.m[i].in) { memset(&FST.m[i], 0, sizeof FST.m[i]), FST.m[i].in = true, FST.m[i].id = id; FST.order[FST.nm++] = id; return; }
}
static void fst_leave(uint16_t id) {
    FstMem *m = fst_mem(id);
    if (m) m->in = false;
    int j = 0;
    for (int i = 0; i < FST.nm; i++) if (FST.order[i] != id) FST.order[j++] = FST.order[i];
    FST.nm = j;
}
static void fst_deliver(void) {
    for (int i = 0; i < FST.nm; i++) {
        FstMem *m = fst_mem(FST.order[i]);
        if (!m || m->ready) continue;
        for (int k = 0; k < FST.nkv; k++) fst_push(m, FST.kv[k].key, FST.kv[k].v, FST.kv[k].len, 0);
        m->ready = true;
    }
}
static int fst_kv(const char *key) { for (int i = 0; i < FST.nkv; i++) if (!strcmp(FST.kv[i].key, key)) return i; return -1; }
static double sora_melty_now_ms(void) { FST.calls++; return FST.now; }
static int sora_melty_story_ready(void) { FST.calls++; FstMem *m = fst_me(); return m && m->ready; }
static int sora_melty_story_leader(void) { FST.calls++; return FK.self && fst_leader() == FK.self; }
static int sora_melty_story_set(const char *key, const void *value, int len) {
    FST.calls++;
    if (!FK.self || fst_leader() != FK.self) return 0;
    int k = fst_kv(key);
    if (len < 0) {
        if (k >= 0) FST.kv[k] = FST.kv[--FST.nkv];
        fst_all(key, NULL, 0, 1);
        return 1;
    }
    if (k < 0 && FST.nkv < 16) k = FST.nkv++, snprintf(FST.kv[k].key, sizeof FST.kv[k].key, "%s", key);
    if (k < 0) return 0;
    FST.kv[k].len = len < 64 ? len : 64;
    memcpy(FST.kv[k].v, value, (size_t)FST.kv[k].len);
    FST.sets++;
    fst_all(key, value, len, 0);
    return 1;
}
static int sora_melty_story_clear(const char *prefix) {
    FST.calls++;
    if (!FK.self || fst_leader() != FK.self) return 0;
    FST.clears++;
    for (int k = 0; k < FST.nkv; k++)
        if (!strncmp(FST.kv[k].key, prefix, strlen(prefix))) { fst_all(FST.kv[k].key, NULL, 0, 1); FST.kv[k--] = FST.kv[--FST.nkv]; }
    return 1;
}
static int sora_melty_story_get(const char *key, void *buffer, int capacity) {
    FST.calls++;
    FstMem *m = fst_me();
    int k = fst_kv(key);
    if (!m || !m->ready || k < 0) return -1;
    int n = FST.kv[k].len < capacity ? FST.kv[k].len : capacity;
    memcpy(buffer, FST.kv[k].v, (size_t)n);
    return n;
}
static int sora_melty_story_trigger(const char *key, const void *args, int len) {
    FST.calls++;
    FstMem *l = fst_mem(fst_leader());
    if (!FK.self || !l || l->id == FK.self || l->nt >= 16) return 0;
    FstTrig *t = &l->t[l->nt++];
    t->from = (uint16_t)FK.self, t->len = len < 64 ? len : 64;
    snprintf(t->key, sizeof t->key, "%s", key);
    if (t->len > 0) memcpy(t->a, args, (size_t)t->len);
    FST.triggers++;
    return 1;
}
static int sora_melty_story_next(char *key, int key_capacity, void *value, int value_capacity, int *value_len, int *deleted) {
    FST.calls++;
    FstMem *m = fst_me();
    if (!m || m->rq >= m->nq) return 0;
    FstChg *c = &m->q[m->rq++];
    snprintf(key, (size_t)key_capacity, "%s", c->key);
    int n = c->len < value_capacity ? c->len : value_capacity;
    if (n > 0) memcpy(value, c->v, (size_t)n);
    *value_len = n, *deleted = c->del;
    return 1;
}
static int sora_melty_trigger_next(uint16_t *from, char *key, int key_capacity, void *args, int args_capacity, int *args_len) {
    FST.calls++;
    FstMem *m = fst_me();
    if (!m || m->rt >= m->nt) return 0;
    FstTrig *t = &m->t[m->rt++];
    *from = t->from;
    snprintf(key, (size_t)key_capacity, "%s", t->key);
    int n = t->len < args_capacity ? t->len : args_capacity;
    if (n > 0) memcpy(args, t->a, (size_t)n);
    *args_len = n;
    return 1;
}

// ---- (s191) a fake game per copy for the room clock (CK's hour hooks: the exe's test_warp_hour / test_warp_hold_hour)
typedef struct { float hour; bool held, world; int holds, releases; } FakeGame;
static FakeGame g_fg;
static float fg_hour(bool *held) { *held = g_fg.held; return g_fg.world ? g_fg.hour : -1; }
static void fg_hold(float h) {
    if (h < 0) { if (g_fg.held) g_fg.held = false, g_fg.releases++; return; }
    g_fg.held = true, g_fg.hour = h, g_fg.holds++;
}
static void fg_run(double ms) { if (g_fg.world && !g_fg.held) g_fg.hour = (float)clock_wrap24(g_fg.hour + ms * CLOCK_RATE_NORMAL); }
static void ck_reset(float hour) {  // a fresh DLL copy's room clock + its game
    memset(&CK, 0, sizeof CK);
    CK.on = -1, CK.fake = -1, CK.hour = fg_hour, CK.hold = fg_hold, CK.wxmode = 1, CK.fake_wx = CK.fake_cl = -1, CK.take = 1;  // (as CK's initializer)
    memset(&g_fg, 0, sizeof g_fg);
    g_fg.hour = hour, g_fg.world = true;
}

static void fk_reset(void) {
    memset(&FK, 0, sizeof FK);
    FK.send_ret = -1, FK.connected = 1, FK.self = 5, FK.state = 2;
}
static void fk_join(uint16_t id, const char *name) {
    sora_melty_peer_event *e = &FK.ev[FK.nev++];
    memset(e, 0, sizeof *e);
    e->peer = id, e->joined = 1;
    snprintf(e->name, sizeof e->name, "%s", name ? name : "");
}
static void fk_leave(uint16_t id) {
    sora_melty_peer_event *e = &FK.ev[FK.nev++];
    memset(e, 0, sizeof *e);
    e->peer = id, e->joined = 0;
}
static void fk_in(uint16_t from, const uint8_t *b, int len) {
    if (FK.nin >= FK_IN_MAX) { printf("  (fake in-queue full)\n"); return; }
    FkIn *p = &FK.in[FK.nin++];
    p->from = from, p->len = len;
    if (len > 0) memcpy(p->b, b, (size_t)(len < FK_CAP ? len : FK_CAP));
}
static int fk_count(int type) {
    int c = 0;
    for (int i = 0; i < FK.nout; i++) c += FK.out[i].type == type;
    return c;
}
static bool fk_all_to_everyone(void) {
    for (int i = 0; i < FK.nout; i++) if (FK.out[i].dest != SORA_MELTY_EVERYONE) return false;
    return true;
}
// (s196) NET_KILLS sends of kind op (NKO_ASK / NKO_PART) to room player dest (-1 = any); *last = the last one
static int fk_kills(int op, int dest, NetKillsMsg *last) {
    int c = 0;
    for (int i = 0; i < FK.nout; i++)
        if (FK.out[i].type == NET_KILLS && FK.out[i].b[7] == op && (dest < 0 || FK.out[i].dest == dest)) c += net_unpack_kills(FK.out[i].b, FK.out[i].len, NULL, NULL, last);
    return c;
}
// (s197) killsync is ON by default, so a room player who joins is asked for his kill list: those NET_KILLS asks / parts go to that
// ONE room player by design (test 19). Every OTHER packet must still be one send to everyone; each NET_KILLS send must be a good
// ask / part to one joined player (never to everyone). *asks / *parts = how many (the callers check the exact number).
static bool fk_all_to_everyone_but_kills(int *asks, int *parts) {
    int na = 0, np = 0;
    for (int i = 0; i < FK.nout; i++) {
        const FkOut *o = &FK.out[i];
        if (o->type != NET_KILLS) { if (o->dest != SORA_MELTY_EVERYONE) return false; continue; }
        NetKillsMsg m;
        if (o->dest == SORA_MELTY_EVERYONE || o->dest <= 0 || !net_unpack_kills(o->b, o->len, NULL, NULL, &m)) return false;
        if (m.op == NKO_ASK) na++; else np++;
    }
    if (asks) *asks = na;
    if (parts) *parts = np;
    return true;
}

// ---- packets "the other player's code" makes
static int mk_snap(uint8_t player, uint16_t seq, float x, uint8_t *b, int cap) {
    NetSnap s;
    memset(&s, 0, sizeof s);
    s.player = player, s.seq = seq, s.feet[0] = x, s.feet[1] = 2, s.feet[2] = 3, s.yaw = 1.0f, s.hp = 80;
    return net_pack_snap(&s, b, cap);
}
static int mk_edmg(uint8_t player, uint16_t seq, uint32_t hid, uint8_t *b, int cap) {
    NetEdEnt e = {hid, 7, 30, 15, 1, 0};
    return net_pack_edmg(player, seq, &e, 1, b, cap);
}
static int mk_fire(uint8_t player, uint16_t seq, uint8_t *b, int cap) {
    NetFireEnt f = {{10, 20, 30}, 260, 130, 0};
    return net_pack_fire(player, seq, &f, 1, b, cap);
}
static int mk_react(uint8_t player, uint16_t seq, uint8_t *b, int cap) {  // (s194)
    float f[3] = {40, 6, -3};
    NetReactEnt e = nr_ent_make(0x77, 3, 125, 0, 7, "blade", f, false);
    return net_pack_react(player, seq, &e, 1, b, cap);
}
static int mk_esync(uint8_t player, uint16_t seq, uint8_t *b, int cap) {
    NetEsEnt e;
    memset(&e, 0, sizeof e);
    e.hid = 0x88, e.pos[0] = 69;
    return net_pack_esync(player, seq, &e, 1, NULL, 0, b, cap);
}

// ---- fake exe parts (as net_glue_test.c)
static NetExEdmg g_fq[64];
static int g_nfq;
static int fake_take(NetExEdmg *out, int max, int size) {
    if (size != (int)sizeof *out) return -1;
    int n = g_nfq < max ? g_nfq : max;
    memcpy(out, g_fq, sizeof *out * (size_t)n), memmove(g_fq, g_fq + n, sizeof *g_fq * (size_t)(g_nfq - n)), g_nfq -= n;
    return n;
}
static int g_nap;
static int fake_apply(uint32_t hid, int dealt, int after, int f54, int f40, int kill) { g_nap++; return 1; }
static void queue_hit(uint32_t hid, int dealt, int after, int kill) {
    NetExEdmg *e = &g_fq[g_nfq++];
    memset(e, 0, sizeof *e);
    e->hid = hid, e->dealt = dealt, e->after = after, e->kill = kill, e->f54 = 15, e->f40 = 1;
    snprintf(e->name, sizeof e->name, "Enemy_Test");
}
// (s197) kill sync's world hooks (as net_glue_test.c): the open world, the Blood Moon count, the exe reset
static int g_open_v = 1, g_mv = -1, g_rst;
static int fake_open(void) { return g_open_v; }
static int fake_moons(void) { return g_mv; }
static int fake_reset(int *kills) { g_rst++; if (kills) *kills = 0; return 0; }
static int es_enum(uint32_t *h, int max) { h[0] = 0x77; return 1; }
static int es_read(uint32_t hid, EsState *o) { memset(o, 0, sizeof *o); o->hid = hid, o->pos[0] = 3.0f; return 1; }
static void es_pup(uint32_t hid, const EsState *t, int snap) {}
static void es_brn(uint32_t hid, int on) {}
static int es_evs(EsEvent *o, int max) { return 0; }
static void es_sns(uint32_t hid, int on, uint32_t name, const uint32_t *p) {}

static void env_set(const char *k, const char *v) {
#ifdef _WIN32
    _putenv_s(k, v ? v : "");  // ("" removes it)
#else
    if (v) setenv(k, v, 1); else unsetenv(k);
#endif
}
static void reset_all(void) {
    memset(&NET, 0, sizeof NET);  // (NET.inited false: net_init_from_env runs again, as at game start)
    memset(&NETES, 0, sizeof NETES), NETES.bound = true;
    NETES.c.io = (EsIface){es_enum, es_read, es_pup, es_brn, es_evs, es_sns};
    g_net_esync = g_net_esynclog = false, g_net_loop = false;
    memset(&NETED, 0, sizeof NETED), NETED.bound = true, NETED.take = fake_take, NETED.apply = fake_apply, NETED.reset = fake_reset;
    g_open_v = 1, g_mv = -1, g_rst = 0;  // (s197)
    memset(&NETF, 0, sizeof NETF), g_net_fire = true, g_net_fire_en = false, g_net_fire_log = false, g_nfr = g_fr_ret = 0;
    g_nfq = g_nap = 0, g_net_edmg = true, g_net_edmg_log = false;
    memset(&NETR, 0, sizeof NETR), g_net_react = false;  // (s194)
    memset(&NETK, 0, sizeof NETK), NETK.bound = true, g_net_kills_log = false;  // (s196)
    NETK.open_fn = fake_open;  // (s197 S2: the open world; no Blood Moon count unless a case sets NETK.moons_fn)
    g_net_kills = NET_KILLS_DEFAULT;  // (s197: the compiled default, ON, in every case, as after a DLL load)
    memset(g_fx_gen, 0, sizeof g_fx_gen), memset(g_fx_live, 0, sizeof g_fx_live), memset(g_kills, 0, sizeof g_kills);
    memset(g_slot_off, 0, sizeof g_slot_off);
    g_nkill = g_drops = 0, g_log[0] = 0;
    env_set("SAVAGEWIND_SORA_NET", NULL), env_set("SAVAGEWIND_SORA_NET_NAME", NULL), env_set("SAVAGEWIND_SORA_NET_PEERS", NULL);
    env_set("SAVAGEWIND_SORA_NET_PORT", "47871");
}
// a fresh game started by Melty (start returns 1), in the room as peer `self`, first frame done
static uint64_t T0;
static void boot(int self) {
    reset_all();
    fk_reset();
    FK.start_ret = 1, FK.self = self;
    net_init_from_env();
}
static float g_feet[3] = {100.5f, 20.25f, -300.75f};
static void frame(uint64_t now, bool driven) {
    NET.driven = driven;
    net_frame(driven ? g_feet : NULL, now);
}
static int slot_of(uint16_t id) { return net_slot_at(nmelty_ip(id), NMELTY_PORT); }

// 1. start
static void test_start(void) {
    printf("1. start (Melty yes / SAVAGEWIND_SORA_NET=0 / Melty no = UDP)\n");
    boot(5);
    CHECK(FK.start_n == 1 && !strcmp(FK.folder, "LegendOfKairi") && !strcmp(FK.game, "legend-of-kairi") && !strcmp(FK.name, "Sora"));
    CHECK(NET.melty && NET.on && !NET.sock.ok);
    CHECK(strstr(g_log, "[net] melty: started by Melty") && strstr(g_log, "[net] on: Melty Net room (no UDP port)"));
    CHECK(!strstr(g_log, "UDP port 47871"));
    frame(T0, true);
    CHECK(FK.frame_n == 1 && !NET.sock.ok);  // (the room is pumped once per frame)
    net_set_on(false);
    CHECK(!NET.on && NET.melty);
    // the player's name goes to the room
    reset_all(), fk_reset(), FK.start_ret = 1;
    env_set("SAVAGEWIND_SORA_NET_NAME", "Kairi");
    net_init_from_env();
    CHECK(NET.melty && !strcmp(FK.name, "Kairi"));
    net_set_on(false);
    // SAVAGEWIND_SORA_NET=0 keeps this copy out of the room (solo): Melty is not even asked
    reset_all(), fk_reset(), FK.start_ret = 1;
    env_set("SAVAGEWIND_SORA_NET", "0");
    net_init_from_env();
    CHECK(FK.start_n == 0 && !NET.melty && !NET.on);
    // Melty did not start this copy: the UDP path as before (net_glue_test covers it in full). (s189: a copy that Melty DID
    // launch but whose client fails gets UDP only with a pin list, test 14; so this case is a launch outside Melty.)
    reset_all(), fk_reset(), FK.start_ret = 0;
    env_set("SAVAGEWIND_SORA_MELTY", NULL), env_set("SAVAGEWIND_SORA_NET", "1");
    net_init_from_env();
    env_set("SAVAGEWIND_SORA_MELTY", "1");
    CHECK(FK.start_n == 0 && !NET.melty && NET.on && NET.sock.ok && strstr(g_log, "UDP port 47871"));
    for (int i = 0; i < 5; i++) frame(T0 + 1000 + 20 * i, true);
    CHECK(FK.frame_n == 0 && FK.nout_all == 0 && FK.recv_n == 0);  // (no room call at all on UDP)
    CHECK(NET.st.sent + NET.st.err > 0);  // (the HELLO broadcast / SNAPs went to the socket)
    net_set_on(false);
    CHECK(!NET.sock.ok);
    env_set("SAVAGEWIND_SORA_NET", NULL);
}

// 2. not connected yet
static void test_not_connected(void) {
    printf("2. not connected yet: nothing sent, melty_wait counts\n");
    reset_all(), fk_reset();
    FK.start_ret = 1, FK.connected = 0, FK.self = 0;
    net_init_from_env();
    fk_join(7, "Riku");  // (a join before our welcome: he gets a slot, still nothing may go out)
    for (int i = 0; i < 30; i++) frame(T0 + 16 * i, true);
    queue_hit(0xAAAA0001, 5, 8, 0);
    float c[3] = {1, 2, 3};
    net_fire_note(c, false, false, 2, 1, 0);
    frame(T0 + 600, true);
    CHECK(FK.nout_all == 0);
    CHECK(NET.st.melty_wait > 0 && NET.st.sent == 0 && NET.st.err == 0);
    CHECK(NET.melty_self == 0);
    CHECK(!strstr(g_log, "not in the room after 20 s"));
    frame(NET.melty_since + 20001, true);
    frame(NET.melty_since + 20050, true);
    CHECK(count_str(g_log, "not in the room after 20 s") == 1);
    // the room lets us in: sends start
    FK.connected = 1, FK.self = 5;
    uint32_t w = NET.st.melty_wait;
    frame(NET.melty_since + 20200, true);
    CHECK(NET.me == 5 && NET.melty_self == 5 && FK.nout_all > 0 && fk_count(NET_SNAP) >= 1 && NET.st.melty_wait == w);
    // a send the room refuses (returns 0) is an error, nothing else
    FK.send_ret = 0;
    uint32_t e = NET.st.err;
    frame(NET.melty_since + 20400, true);
    CHECK(NET.st.err > e);
    net_set_on(false);
}

// 3. our room id
static void test_self_id(void) {
    printf("3. our room id = NET.melty_self = NET.me\n");
    boot(5);
    CHECK(NET.melty_self == 0);  // (not read before the first frame)
    frame(T0, false);
    CHECK(NET.melty_self == 5 && NET.me == 5);
    frame(T0 + 20, false);
    CHECK(NET.me == 5);
    boot(300);  // (an id past 255: the low byte)
    frame(T0, false);
    CHECK(NET.melty_self == 300 && NET.me == 44);
    boot(512);  // (low byte 0: re-rolled, never 0)
    frame(T0, false);
    CHECK(NET.melty_self == 512 && NET.me != 0);
    net_set_on(false);
}

// 4. joins, names
static void name_is(const char *in, int cap, uint16_t peer, const char *want) {
    char out[NET_NAME_MAX + 1 + 64];
    memset(out, 'Q', sizeof out);
    nmelty_name(out, cap, in, peer);
    bool ok = !strcmp(out, want) && strlen(out) < (size_t)cap && out[cap] == 'Q';  // (never past cap)
    CHECK(ok);
    if (!ok) printf("    nmelty_name -> \"%s\", want \"%s\"\n", out, want);
}
static void test_join_names(void) {
    printf("4. joins make slots, names through nmelty_name\n");
    int cap = NET_NAME_MAX + 1;
    name_is("Riku", cap, 7, "Riku");
    name_is("Ri\xC3\xA9ku", cap, 7, "Ri?ku");         // 2-byte UTF-8 = one '?'
    name_is("\xE2\x9C\xA8Star", cap, 7, "?Star");      // 3-byte
    name_is("\xF0\x9F\x98\x80!", cap, 7, "?!");        // 4-byte
    name_is("a\tb\x01" "c\x7f", cap, 7, "a?b?c?");     // control chars / DEL
    name_is("", cap, 9, "Player 9");
    name_is(NULL, cap, 3, "Player 3");
    name_is("   ", cap, 9, "Player 9");
    name_is("Bob   ", cap, 9, "Bob");
    name_is("\x80\x80\xBF", cap, 4, "Player 4");      // stray continuation bytes only
    name_is("Player", 3, 1, "Pl");                     // a tiny cap
    char n63[64];
    memset(n63, 'x', 63), n63[63] = 0;
    name_is(n63, cap, 7, "xxxxxxxxxxxxxxx");          // 63 bytes -> cut to 15
    for (int i = 0; i < 31; i++) n63[2 * i] = (char)0xC3, n63[2 * i + 1] = (char)0xA9;
    n63[62] = 'Z';
    name_is(n63, cap, 7, "???????????????");          // 31 x U+00E9 + Z -> 15 '?'
    name_is("Room Player 65535", cap, 65535, "Room Player 655");
    {
        char o[NET_NAME_MAX + 1];
        nmelty_name(o, cap, "", 65535);
        CHECK(!strcmp(o, "Player 65535"));
    }

    boot(5);
    frame(T0, false);
    fk_join(7, "Riku");
    fk_join(5, "Me");      // our own id: no slot
    fk_join(0, "Zero");    // id 0: no slot
    fk_join(8, "Ri\xC3\xA9ku\x01");
    frame(T0 + 20, false);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 2 && NET.nmroom == 2);
    int k = slot_of(7);
    CHECK(k >= 0 && !strcmp(NET.slot[k].p.name, "Riku") && NET.slot[k].p.player == 7 && NET.slot[k].p.st == PEER_LIVE);
    CHECK(strstr(g_log, "joined: Riku (Melty room peer 7)") != NULL);
    k = slot_of(8);
    CHECK(k >= 0 && !strcmp(NET.slot[k].p.name, "Ri?ku?"));
    CHECK(slot_of(5) < 0 && slot_of(0) < 0);
    // a second join of the same id (a rename): no second slot
    fk_join(7, "Riku2");
    frame(T0 + 40, false);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 2 && NET.nmroom == 2);
    // a 63-byte name and an empty one through the room
    memset(n63, 'y', 63), n63[63] = 0;
    fk_leave(8);
    fk_join(9, n63);
    frame(T0 + 60, false);
    k = slot_of(9);
    CHECK(k >= 0 && strlen(NET.slot[k].p.name) == NET_NAME_MAX && NET.slot[k].p.name[0] == 'y');
    fk_leave(9);
    fk_join(10, "");
    frame(T0 + 80, false);
    k = slot_of(10);
    CHECK(k >= 0 && !strcmp(NET.slot[k].p.name, "Player 10"));
    CHECK(strstr(g_log, "joined: Player 10 (Melty room peer 10)") != NULL);
    net_set_on(false);
}

// 5. leave frees at once; quiet keeps the slot faded
static void test_leave_and_quiet(void) {
    printf("5. a leave frees the slot at once; a quiet room player is faded, never timed out\n");
    boot(5);
    fk_join(7, "Riku"), fk_join(8, "Kairi");
    frame(T0, false);
    int k7 = slot_of(7), k8 = slot_of(8);
    CHECK(k7 >= 0 && k8 >= 0);
    uint8_t b[NET_PKT_MAX];
    fk_in(7, b, mk_snap(7, 1, 1, b, sizeof b));
    fk_in(8, b, mk_snap(8, 1, 2, b, sizeof b));
    frame(T0 + 20, false);
    CHECK(NET.slot[k7].st.snap == 1 && NET.slot[k8].st.snap == 1);
    fk_leave(7);
    frame(T0 + 40, false);
    CHECK(slot_of(7) < 0 && !NET.slot[k7].used && g_slot_off[k7] >= 1 && NET.nmroom == 1);
    CHECK(strstr(g_log, "\"Riku\" left (Melty room)") != NULL);
    fk_leave(99);  // (a leave of someone never here)
    frame(T0 + 60, false);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 1);
    // Kairi goes quiet
    frame(T0 + 3000, false);
    CHECK(NET.slot[k8].used && NET.slot[k8].p.st == PEER_FADING);
    uint64_t t[] = {9500, 10500, 11000, 30000, 120000, 600000};
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
        frame(T0 + t[i], false);
        CHECK(NET.slot[k8].used && slot_of(8) == k8 && NET.slot[k8].p.st == PEER_FADING);
    }
    PeerPose q = peer_sample(&NET.slot[k8].p, T0 + 600000);
    CHECK(q.alpha == 0.0f);  // (held faded out)
    CHECK(!strstr(g_log, "timed out"));
    // he speaks again: live
    fk_in(8, b, mk_snap(8, 2, 3, b, sizeof b));
    frame(T0 + 600020, false);
    CHECK(NET.slot[k8].p.st == PEER_LIVE && NET.slot[k8].st.snap == 2);
    net_set_on(false);
}

// 6. SNAP: one send to EVERYONE per tick, where() first; never HELLO / BYE
static void test_snap_send(void) {
    printf("6. SNAP = one send to everyone per tick, where() before it, no HELLO / BYE\n");
    boot(5);
    frame(T0, true);
    int alone = fk_count(NET_SNAP);
    CHECK(alone == 1);  // (alone in the room: our SNAP still goes, the room places us by it)
    fk_join(7, "Riku"), fk_join(8, "Kairi");
    frame(T0 + 1, true);
    uint16_t seq0 = NET.seq;
    int w0 = FK.where_n, s0 = fk_count(NET_SNAP);
    for (int i = 0; i < 100; i++) frame(T0 + 10 + 10 * i, true);  // 1 s
    int snaps = fk_count(NET_SNAP) - s0;
    CHECK(snaps >= 19 && snaps <= 21);
    CHECK((uint16_t)(NET.seq - seq0) == snaps);  // one send per SNAP tick, with 2 room players (not one per peer)
    CHECK(FK.where_n - w0 == snaps);
    bool order = true, dest = true, unp = true;
    int nth = 0;
    for (int i = 0; i < FK.nout; i++) {
        FkOut *o = &FK.out[i];
        if (o->type != NET_SNAP) continue;
        nth++;
        order = order && o->where_n == nth;  // (the nth SNAP went right after the nth where())
        dest = dest && o->dest == SORA_MELTY_EVERYONE;
        int type; uint8_t pl; uint16_t sq; NetSnap s; char nm[NET_NAME_MAX + 1];
        unp = unp && net_unpack(o->b, o->len, &type, &pl, &sq, &s, nm) && type == NET_SNAP && pl == NET.me && s.feet[0] == g_feet[0];
    }
    CHECK(order && dest && unp);
    CHECK(FK.where[0] == g_feet[0] && FK.where[1] == g_feet[1] && FK.where[2] == g_feet[2] && FK.where[3] == g_yaw && !strcmp(FK.area, ""));
    CHECK(fk_count(NET_HELLO) == 0 && fk_count(NET_BYE) == 0 && FK.send_big == 0 && FK.send_nosn == 0);
    // (s190, spec 4.1) the area the game side set goes to the room with the next SNAP; NULL = not known = ""
    net_set_area("Dungeon012");
    for (int i = 0; i < 10; i++) frame(T0 + 1100 + 10 * i, true);
    CHECK(!strcmp(FK.area, "Dungeon012"));
    net_set_area(NULL);
    for (int i = 0; i < 10; i++) frame(T0 + 1300 + 10 * i, true);
    CHECK(!strcmp(FK.area, ""));
    // "net name" asks for a HELLO round on UDP: nothing in the room
    CHECK(net_live_line("net name Kairi"));
    for (int i = 0; i < 10; i++) frame(T0 + 2000 + 10 * i, true);
    // a pin typed while in the room: still nothing but one send to everyone
    CHECK(net_live_line("net peer 192.168.1.50"));
    for (int i = 0; i < 10; i++) frame(T0 + 3000 + 10 * i, true);
    {   // (s197 killsync on by default: Riku and Kairi were each asked ONCE for their kill list, to them only; nothing else per player)
        int na = -1, np = -1;
        NetKillsMsg km;
        CHECK(fk_all_to_everyone_but_kills(&na, &np) && na == 2 && np == 0 && fk_kills(NKO_ASK, 7, &km) == 1 && fk_kills(NKO_ASK, 8, &km) == 1);
    }
    // net off: no BYE
    net_live_set("net", 0);
    CHECK(!NET.on);
    for (int i = 0; i < 10; i++) frame(T0 + 4000 + 10 * i, true);
    CHECK(fk_count(NET_HELLO) == 0 && fk_count(NET_BYE) == 0);
    int n_off = FK.nout_all;
    for (int i = 0; i < 10; i++) frame(T0 + 5000 + 10 * i, true);
    CHECK(FK.nout_all == n_off);  // (off: nothing at all)
    // loading (no Link): SNAPs still go, flagged NF_LOADING
    net_live_set("net", 1);
    for (int i = 0; i < 10; i++) frame(T0 + 6000 + 10 * i, false);
    CHECK(FK.nout_all > n_off);
    net_set_on(false);
}

// 7. EDMG / FIRE / ESYNC: once each, one send to everyone
static void test_events_once(void) {
    printf("7. EDMG / FIRE / ESYNC: each packet once, one send to everyone (no 3x repeat)\n");
    boot(5);
    fk_join(7, "Riku"), fk_join(8, "Kairi");
    frame(T0, false);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 2);
    queue_hit(0xAAAA0001, 5, 8, 0);
    frame(T0 + 100, false);
    CHECK(fk_count(NET_EDMG) == 1 && NETED.st.tx_pkt == 1 && NETED.st.tx_send == 1);
    uint64_t later[] = {100 + NET_EDMG_REP_MS, 100 + 2 * NET_EDMG_REP_MS, 100 + 3 * NET_EDMG_REP_MS, 5000};
    for (size_t i = 0; i < 4; i++) frame(T0 + later[i], false);
    CHECK(fk_count(NET_EDMG) == 1 && NETED.st.tx_send == 1 && NETED.nq == 0);
    for (int i = 0; i < 20; i++) queue_hit(0xBBBB0000u + (uint32_t)i, 5, 8, 0);  // 20 hits = 2 packets
    frame(T0 + 6000, false);
    for (int i = 1; i < 6; i++) frame(T0 + 6000 + i * NET_EDMG_REP_MS, false);
    CHECK(fk_count(NET_EDMG) == 3 && NETED.st.tx_pkt == 3 && NETED.st.tx_send == 3);
    {   // the packets are the mod's own, whole
        int seen = 0;
        for (int i = 0; i < FK.nout; i++) {
            if (FK.out[i].type != NET_EDMG) continue;
            NetEdEnt e[NET_EDMG_MAX]; uint8_t pl = 0; uint16_t sq = 0;
            seen += net_unpack_edmg(FK.out[i].b, FK.out[i].len, &pl, &sq, e) > 0 && pl == NET.me;
        }
        CHECK(seen == 3);
    }
    float c[3] = {-1234.5f, 87.25f, 4321.0f};
    net_fire_note(c, true, true, 2.6f, 1.3f, 1);
    frame(T0 + 7000, false);
    for (int i = 1; i < 6; i++) frame(T0 + 7000 + i * NET_EDMG_REP_MS, false);
    frame(T0 + 9000, false);
    CHECK(fk_count(NET_FIRE) == 1 && NETF.st.tx_send == 1 && NETF.nout == 0);
    uint8_t pk[NET_ESYNC_PKT];
    int n = mk_esync(NET.me, 1, pk, sizeof pk);
    NetEsDest d;
    d.nd = net_dests(d.d, false);
    net_esync_send(pk, n, &d);
    CHECK(fk_count(NET_ESYNC) == 1);
    {   // (s197 killsync on by default: Riku and Kairi never answer, so each is asked at T0 and once more KS_REASK_MELTY_MS later, to him only)
        int na = -1, np = -1;
        NetKillsMsg km;
        CHECK(KS_REASK_MELTY_MS == 5000 && fk_all_to_everyone_but_kills(&na, &np) && na == 4 && np == 0);
        CHECK(fk_kills(NKO_ASK, 7, &km) == 2 && fk_kills(NKO_ASK, 8, &km) == 2);
    }
    // alone in the room: hits / fire are not shared (as on UDP alone)
    fk_leave(7), fk_leave(8);
    frame(T0 + 10000, false);
    int e0 = fk_count(NET_EDMG), f0 = fk_count(NET_FIRE);
    queue_hit(0xCCCC0001, 5, 8, 0);
    net_fire_note(c, false, false, 2.6f, 1.3f, 0);
    frame(T0 + 10100, false);
    CHECK(fk_count(NET_EDMG) == e0 && fk_count(NET_FIRE) == f0 && NETED.st.tx_drop >= 1);
    net_set_on(false);
}

// 8. receive
static void test_receive(void) {
    printf("8. receive: SNAP applied to his slot; strangers / leavers / HELLO / BYE / bad packets dropped; id clash re-rolls\n");
    boot(5);
    fk_join(7, "Riku"), fk_join(8, "Kairi");
    frame(T0, false);
    int k = slot_of(7);
    CHECK(k >= 0);
    uint8_t b[NET_PKT_MAX + 64];
    int n = mk_snap(7, 10, 42.0f, b, sizeof b);
    fk_in(7, b, n);
    frame(T0 + 100, false);
    CHECK(NET.slot[k].st.snap == 1 && NET.slot[k].last_snap_ms == T0 + 100 && NET.slot[k].p.n == 1);
    CHECK(NET.slot[k].p.ring[0].feet[0] == 42.0f && NET.slot[k].p.player == 7 && NET.st.recv == 1);
    g_log[0] = 0;
    net_diag_log();
    {
        char want[64];
        snprintf(want, sizeof want, "[net] diag slot %d \"Riku\" snap rx 1 last ", k + 1);
        CHECK(strstr(g_log, want) != NULL);
        CHECK(strstr(g_log, "[net] melty: in the room, our room peer id 5") != NULL);
    }
    // a peer id the room never announced
    uint32_t other = NET.st.other;
    fk_in(99, b, mk_snap(99, 1, 1, b, sizeof b));
    fk_in(99, b, mk_edmg(99, 1, 0x1234, b, sizeof b));
    frame(T0 + 120, false);
    CHECK(slot_of(99) < 0 && NET.st.other == other + 2 && g_nap == 0);
    // our own id from the room
    fk_in(5, b, mk_snap(5, 1, 1, b, sizeof b));
    frame(T0 + 130, false);
    CHECK(NET.st.other == other + 3);
    // HELLO / BYE from the room (an old build): dropped, his slot and name kept
    fk_in(7, b, net_pack_hello(7, 11, "Imposter", b, sizeof b));
    fk_in(7, b, net_pack_bye(7, 12, b, sizeof b));
    frame(T0 + 140, false);
    CHECK(NET.st.other == other + 5 && slot_of(7) == k && !strcmp(NET.slot[k].p.name, "Riku"));
    // EDMG / FIRE / ESYNC from a room player: applied (once)
    fk_in(7, b, mk_edmg(7, 500, 0x4444, b, sizeof b));
    fk_in(7, b, mk_fire(7, 600, b, sizeof b));
    fk_in(7, b, mk_esync(7, 700, b, sizeof b));
    frame(T0 + 150, false);
    CHECK(g_nap == 1 && NETED.st.rx_pkt == 1 && g_nfr == 1 && NETF.st.rx_pkt == 1 && NETES.c.st.rx_off == 1);
    // bad: wrong version, bad magic, too short, bad SNAP size
    uint32_t bad = NET.st.bad, snaps = NET.slot[k].st.snap;
    n = mk_snap(7, 20, 1, b, sizeof b);
    b[2] = NET_VERSION - 1;
    fk_in(7, b, n);
    n = mk_snap(7, 21, 1, b, sizeof b);
    b[0] = 'X';
    fk_in(7, b, n);
    n = mk_snap(7, 22, 1, b, sizeof b);
    fk_in(7, b, 5);
    fk_in(7, b, n - 1);
    frame(T0 + 160, false);
    CHECK(NET.st.bad == bad + 4 && NET.slot[k].st.snap == snaps);
    CHECK(strstr(g_log, "packet from Melty room peer 7 has protocol version") != NULL);
    // from a peer who left
    fk_leave(8);
    frame(T0 + 170, false);
    other = NET.st.other;
    fk_in(8, b, mk_snap(8, 1, 1, b, sizeof b));
    frame(T0 + 180, false);
    CHECK(slot_of(8) < 0 && NET.st.other == other + 1);
    // OUR player byte from another room id: we re-roll, no crash, nothing sent back
    CHECK(NET.me == 5);
    int sends = FK.nout_all;
    n = mk_snap(5, 30, 7, b, sizeof b);
    fk_in(7, b, n);
    frame(T0 + 190, false);
    CHECK(NET.me != 5 && strstr(g_log, "player id clash with Riku") != NULL);
    CHECK(FK.nout_all == sends);  // (the receive sends nothing: no echo loop)
    uint8_t me = NET.me;
    for (int i = 0; i < 4; i++) {  // he keeps his byte 5: we stay as we are now
        fk_in(7, b, mk_snap(5, (uint16_t)(31 + i), 7, b, sizeof b));
        frame(T0 + 200 + 10 * (uint64_t)i, false);
    }
    CHECK(NET.me == me && NET.me != 5 && slot_of(7) == k && count_str(g_log, "player id clash") == 1);
    net_set_on(false);
}

// 9. a new seat
static void test_new_seat(void) {
    printf("9. a new seat (self id changes): every slot starts over\n");
    boot(5);
    fk_join(7, "Riku"), fk_join(8, "Kairi");
    frame(T0, false);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 2 && NET.nmroom == 2);
    uint8_t b[NET_PKT_MAX];
    fk_in(7, b, mk_snap(7, 1, 1, b, sizeof b));
    frame(T0 + 20, false);
    FK.self = 12;
    frame(T0 + 40, false);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 0 && NET.nmroom == 0 && NET.melty_self == 12 && NET.me == 12);
    CHECK(strstr(g_log, "new seat in the room (peer 5 -> 12)") != NULL);
    // the new room names everyone again
    fk_join(7, "Riku"), fk_join(9, "Namine");
    frame(T0 + 60, false);
    int k = slot_of(7);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 2 && k >= 0 && NET.slot[k].st.snap == 0 && NET.slot[k].p.n == 0);
    // self 0 for a moment (between rooms) is not a new seat
    FK.self = 0;
    frame(T0 + 80, false);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 2 && NET.melty_self == 12);
    FK.self = 12;
    frame(T0 + 100, false);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 2);
    net_set_on(false);
}

// 10. net off in the room
static void test_net_off(void) {
    printf("10. net off in the room: still pumped, packets thrown away, roster kept, net on re-makes the slots\n");
    boot(5);
    fk_join(7, "Riku"), fk_join(8, "Kairi");
    frame(T0, true);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 2);
    NetKillsMsg km;
    // (s197 killsync on by default) the frame before net 0 sent our SNAP + one kill-list ask to each of the 2 new room players
    CHECK(FK.nout_all == 3 && fk_count(NET_SNAP) == 1 && fk_kills(NKO_ASK, 7, &km) == 1 && fk_kills(NKO_ASK, 8, &km) == 1);
    CHECK(net_live_set("net", 0) && !NET.on && NET.melty);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 0 && NET.nmroom == 2);
    CHECK(strstr(g_log, "[net] off: still in the Melty room") != NULL);
    uint8_t b[NET_PKT_MAX];
    for (int i = 0; i < 40; i++) fk_in(7, b, mk_snap(7, (uint16_t)(1 + i), 1, b, sizeof b));
    int fr = FK.frame_n;
    uint32_t recv = NET.st.recv;
    frame(T0 + 100, true);
    CHECK(FK.frame_n == fr + 1 && FK.nin == 0 && NET.st.recv == recv);  // pumped, thrown away
    fk_join(9, "Namine");  // the roster moves on while off
    fk_leave(8);
    frame(T0 + 120, true);
    CHECK(NET.nmroom == 2 && netslot_count(NET.slot, NET_MAX_PEERS) == 0);
    CHECK(FK.nout_all == 3);  // (only the SNAP + the 2 kill asks of the frame before net 0; nothing after: no ask to Namine while off)
    CHECK(net_live_set("net", 1) && NET.on && !NET.sock.ok);
    frame(T0 + 140, true);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 2 && slot_of(7) >= 0 && slot_of(9) >= 0 && slot_of(8) < 0);
    // (s197) net on again = new slots = asked again: Riku a 2nd time, Namine once, Kairi (gone) never again
    CHECK(fk_kills(NKO_ASK, 7, &km) == 2 && fk_kills(NKO_ASK, 9, &km) == 1 && fk_kills(NKO_ASK, 8, &km) == 1);
    CHECK(count_str(g_log, "[kills] kill sync on (the compiled default since s197") == 2);  // (one line per net on: boot + this one)
    // BUG: net_set_on(true) (sora_net.h:296) re-rolls NET.me for the room too, and net_melty_pump only copies our room id
    // into NET.me when the id CHANGES (sora_net.h:486), so after net 0 -> 1 (or "set netport", Sora mode off -> on) we play
    // under a random byte instead of our room id: the "room ids are unique" promise (sora_net.h:493) is gone and a clash
    // with a peer (1 in 256 each) costs a re-roll. (Fixed in its own commit: net_set_on(true) takes NET.melty_self back.)
    CHECK(NET.me == (uint8_t)NET.melty_self);
    // Sora mode off: parked, the room still pumped and drained; Sora back on resumes
    net_sora_off();
    CHECK(!NET.on && NET.parked && netslot_count(NET.slot, NET_MAX_PEERS) == 0);
    int out_parked = FK.nout_all;
    for (int i = 0; i < 10; i++) fk_in(7, b, mk_snap(7, (uint16_t)(100 + i), 1, b, sizeof b));
    fr = FK.frame_n;
    net_sora_off();
    net_sora_off();
    CHECK(FK.frame_n == fr + 2 && FK.nin == 0 && !NET.on);
    CHECK(FK.nout_all == out_parked);  // (s197: parked = no kill ask either)
    frame(T0 + 300, true);
    CHECK(NET.on && !NET.parked && netslot_count(NET.slot, NET_MAX_PEERS) == 2);
    CHECK(fk_kills(NKO_ASK, 7, &km) == 3 && fk_kills(NKO_ASK, 9, &km) == 2);  // (s197: back on = new slots = asked again)
    CHECK(NET.me == (uint8_t)NET.melty_self);  // BUG: same as above (sora_net.h:296)
    CHECK(g_hello_bye_ever == 0);
    net_set_on(false);
}

// 11. a full table
static void test_full_table(void) {
    printf("11. more room players than NET_MAX_PEERS (%d)\n", NET_MAX_PEERS);
    boot(5);
    for (int i = 0; i <= NET_MAX_PEERS; i++) {
        char nm[16];
        snprintf(nm, sizeof nm, "P%d", 20 + i);
        fk_join((uint16_t)(20 + i), nm);
    }
    for (int i = 0; i < 5; i++) frame(T0 + 20 * (uint64_t)i, false);
    uint16_t extra = (uint16_t)(20 + NET_MAX_PEERS);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == NET_MAX_PEERS && slot_of(extra) < 0 && NET.nmroom == NET_MAX_PEERS + 1);
    CHECK(count_str(g_log, "refused") == 1);  // (logged once, not every frame)
    // his packets are dropped, no crash
    uint8_t b[NET_PKT_MAX];
    fk_in(extra, b, mk_snap((uint8_t)extra, 1, 1, b, sizeof b));
    frame(T0 + 200, false);
    CHECK(slot_of(extra) < 0);
    // a slotted player leaves: the extra one is in, in the same frame
    fk_leave(20);
    frame(T0 + 220, false);
    CHECK(slot_of(20) < 0 && slot_of(extra) >= 0 && netslot_count(NET.slot, NET_MAX_PEERS) == NET_MAX_PEERS);
    // a crowd past the roster's 16
    for (int i = 0; i < 40; i++) fk_join((uint16_t)(100 + i), "Crowd");
    frame(T0 + 240, false);
    CHECK(NET.nmroom <= 16 && netslot_count(NET.slot, NET_MAX_PEERS) == NET_MAX_PEERS);
    for (int i = 0; i < 40; i++) fk_leave((uint16_t)(100 + i));
    frame(T0 + 260, false);
    CHECK(NET.nmroom == NET_MAX_PEERS);
    net_set_on(false);
}

// 12. hostile input
static uint64_t g_rng = 0x9E3779B97F4A7C15ull;
static uint32_t rnd(void) { g_rng ^= g_rng << 13, g_rng ^= g_rng >> 7, g_rng ^= g_rng << 17; return (uint32_t)(g_rng >> 16); }
static void test_hostile(void) {
    printf("12. hostile input: odd lengths, fuzz with valid headers (fixed seed)\n");
    boot(5);
    fk_join(7, "Riku"), fk_join(8, "Kairi");
    frame(T0, true);
    static uint8_t b[FK_CAP];
    int lens[] = {0, 1, 6, 7, NET_PKT_MAX, NET_PKT_MAX + 64, NET_PKT_MAX + 65, FK_CAP};
    int fed = 0;
    for (size_t i = 0; i < sizeof lens / sizeof lens[0]; i++) {
        for (int j = 0; j < lens[i]; j++) b[j] = (uint8_t)rnd();
        if (lens[i] >= 3) b[0] = 'S', b[1] = 'N', b[2] = NET_VERSION;
        if (lens[i] >= 4) b[3] = NET_SNAP;
        fk_in(7, b, lens[i]);
        fed += lens[i] > 0 && lens[i] <= NET_PKT_MAX + 64;
    }
    fk_in(7, b, mk_snap(7, 100, 5, b, sizeof b)), fed++;  // (a good one behind them)
    for (int i = 0; i < 20 && FK.nin; i++) frame(T0 + 20 + 20 * (uint64_t)i, true);
    CHECK(FK.nin == 0 && NET.st.recv == (uint32_t)fed);  // (a 0-length / too-big packet reads as "none" this frame; the rest come next frame)
    {
        int k7 = slot_of(7);
        CHECK(k7 >= 0 && NET.slot[k7].p.n > 0 && NET.slot[k7].p.ring[NET.slot[k7].p.n - 1].feet[0] == 5);
    }
    // fuzz: valid header + random body, mutated good packets, plain random bytes; from joined / stranger / our own ids
    uint16_t froms[] = {7, 8, 99, 5, 0, 65535};
    int types[] = {NET_SNAP, NET_EDMG, NET_FIRE, NET_ESYNC, NET_HELLO, NET_BYE, NET_REACT};
    uint32_t recv0 = NET.st.recv;
    int fed2 = 0;
    uint64_t now = T0 + 1000;
    for (int it = 0; it < 6000; it++) {
        if (it == 3000) g_net_esync = true, g_net_react = true;  // (the second half also goes through the enemy-sync + reaction receive)
        int len, kind = rnd() % 10;
        if (kind < 4) {  // valid header, random body
            len = NET_HDR + (int)(rnd() % (NET_PKT_MAX + 64 - NET_HDR + 1));
            for (int j = 0; j < len; j++) b[j] = (uint8_t)rnd();
            net_hdr(b, (int)(rnd() % 9), (uint8_t)rnd(), (uint16_t)rnd());
        } else if (kind < 8) {  // a good packet, a few bytes flipped (header kept), sometimes cut / grown
            int t = types[rnd() % 7];
            uint8_t pl = (uint8_t)(rnd() % 4 ? 7 : rnd());
            uint16_t sq = (uint16_t)rnd();
            if (t == NET_SNAP) len = mk_snap(pl, sq, (float)(rnd() % 1000), b, FK_CAP);
            else if (t == NET_EDMG) len = mk_edmg(pl, sq, rnd(), b, FK_CAP);
            else if (t == NET_FIRE) len = mk_fire(pl, sq, b, FK_CAP);
            else if (t == NET_ESYNC) len = mk_esync(pl, sq, b, FK_CAP);
            else if (t == NET_HELLO) len = net_pack_hello(pl, sq, "Fuzz", b, FK_CAP);
            else if (t == NET_REACT) len = mk_react(pl, sq, b, FK_CAP);
            else len = net_pack_bye(pl, sq, b, FK_CAP);
            int flips = (int)(rnd() % 6);
            for (int f = 0; f < flips && len > NET_HDR; f++) b[NET_HDR + rnd() % (uint32_t)(len - NET_HDR)] = (uint8_t)rnd();
            if (rnd() % 8 == 0) len += (int)(rnd() % 40) - 20;
            if (len < 0) len = 0;
            if (len > FK_CAP) len = FK_CAP;
        } else {  // anything
            len = (int)(rnd() % (NET_PKT_MAX + 128));
            for (int j = 0; j < len; j++) b[j] = (uint8_t)rnd();
        }
        fk_in(froms[rnd() % 6], b, len);
        fed2 += len > 0 && len <= NET_PKT_MAX + 64;
        if (FK.nin >= 48) {
            for (int i = 0; i < 200 && FK.nin; i++) frame(now += 16, rnd() % 2);
        }
        if (it % 500 == 0) fk_join((uint16_t)(7 + rnd() % 2), "Riku"), fk_leave((uint16_t)(7 + rnd() % 2));  // (the roster moves too)
        if (it % 1500 == 0) fk_join(7, "Riku"), fk_join(8, "Kairi");
    }
    for (int i = 0; i < 200 && FK.nin; i++) frame(now += 16, true);
    CHECK(FK.nin == 0 && NET.st.recv - recv0 == (uint32_t)fed2);
    CHECK(slot_of(99) < 0 && slot_of(0) < 0 && slot_of(65535) < 0 && slot_of(5) < 0);  // (only the room's joins make slots)
    for (int k = 0; k < NET_MAX_PEERS; k++)
        if (NET.slot[k].used) CHECK(nmelty_is(NET.slot[k].ip, NET.slot[k].port) && (nmelty_peer(NET.slot[k].ip) == 7 || nmelty_peer(NET.slot[k].ip) == 8));
    {   // (s197 killsync on by default: the moving roster gets kill asks, each to Riku or Kairi only; with this seed no fuzzed packet is a good ask, so no part goes out)
        int na = -1, np = -1;
        NetKillsMsg km;
        CHECK(NET.on && NET.melty && g_hello_bye_ever == 0 && FK.send_big == 0 && FK.send_nosn == 0 && fk_all_to_everyone_but_kills(&na, &np));
        CHECK(na > 0 && np == 0 && fk_kills(NKO_ASK, 7, &km) + fk_kills(NKO_ASK, 8, &km) == na);
    }
    printf("    fuzz: %d packets, bad %u, other %u, edmg rx %u, fire rx %u, react rx %u (bad %u)\n", fed2, NET.st.bad, NET.st.other, NETED.st.rx_pkt, NETF.st.rx_pkt, NETR.st.rx_pkt, NETR.st.rx_bad);
    CHECK(NETR.rx.n <= NR_RXQ && NETR.tx.n == 0 && NETR.nout == 0);  // (s194: nothing received is ever sent on)
    g_net_esync = false, g_net_react = false;
    net_set_on(false);
}

// (b) two copies in one room: the whole glue state swapped per "game", each one's room sends handed to the other
typedef struct {
    __typeof__(NET) net; __typeof__(NETED) ed; __typeof__(NETF) fi; __typeof__(NETES) es; __typeof__(FK) fk;
    __typeof__(CK) ck; FakeGame fg;  // (s191: each copy's room clock + game; the story FST is the room's, shared)
    int nap, nfr; uint16_t id;
} Copy;
static Copy g_cp[2];
static int g_cur = -1;
static void use(int c) {
    if (g_cur == c) return;
    if (g_cur >= 0) {
        Copy *o = &g_cp[g_cur];
        o->net = NET, o->ed = NETED, o->fi = NETF, o->es = NETES, o->fk = FK, o->nap = g_nap, o->nfr = g_nfr, o->ck = CK, o->fg = g_fg;
    }
    Copy *n = &g_cp[c];
    NET = n->net, NETED = n->ed, NETF = n->fi, NETES = n->es, FK = n->fk, g_nap = n->nap, g_nfr = n->nfr, CK = n->ck, g_fg = n->fg;
    g_cur = c;
}
// what copy `c` sent since the last call goes into the other copy's in-queue, from c's room id
static void route(int c) {
    use(c);
    FkOut out[64];
    int n = FK.nout < 64 ? FK.nout : 64;
    memcpy(out, FK.out, sizeof out[0] * (size_t)n);
    FK.nout = 0;
    uint16_t from = g_cp[c].id;
    use(1 - c);
    for (int i = 0; i < n; i++) if (out[i].dest == SORA_MELTY_EVERYONE || out[i].dest == FK.self) fk_in(from, out[i].b, out[i].len);
}
static void test_two_copies(void) {
    printf("(b) two copies in one room: SNAP / EDMG / FIRE end to end\n");
    g_cur = -1;
    for (int c = 0; c < 2; c++) {
        boot(c ? 9 : 4);
        g_cp[c].id = c ? 9 : 4;
        fk_join(c ? 4 : 9, c ? "Sora" : "Riku");  // (each room names the other)
        Copy *o = &g_cp[c];
        o->net = NET, o->ed = NETED, o->fi = NETF, o->es = NETES, o->fk = FK, o->nap = 0, o->nfr = 0, o->ck = CK, o->fg = g_fg;
    }
    uint64_t now = T0 + 50000;
    for (int f = 0; f < 60; f++) {
        now += 16;
        for (int c = 0; c < 2; c++) {
            use(c);
            if (f == 20 && c == 0) queue_hit(0xDDDD0001, 9, 1, 0);  // Sora hits an enemy
            if (f == 30 && c == 1) { float p[3] = {5, 6, 7}; net_fire_note(p, false, false, 2, 1, 0); }  // Riku casts Fire
            frame(now, true);
            route(c);
        }
    }
    use(1);  // Riku's game
    int k = slot_of(4);
    CHECK(k >= 0 && !strcmp(NET.slot[k].p.name, "Sora") && NET.slot[k].st.snap >= 15 && NET.slot[k].p.player == 4);
    CHECK(g_nap == 1 && NETED.st.rx_pkt == 1 && NET.st.bad == 0 && NET.st.other == 0);  // Sora's hit applied once on Riku's game
    use(0);  // Sora's game
    k = slot_of(9);
    CHECK(k >= 0 && !strcmp(NET.slot[k].p.name, "Riku") && NET.slot[k].st.snap >= 15 && NET.slot[k].p.player == 9);
    CHECK(g_nfr == 1 && NETF.st.rx_pkt == 1 && NET.st.bad == 0 && NET.st.other == 0);  // Riku's Fire once on Sora's game
    CHECK(g_hello_bye_ever == 0);
    // Riku leaves the room: gone at once on Sora's game
    fk_leave(9);
    frame(now + 16, true);
    CHECK(slot_of(9) < 0 && netslot_count(NET.slot, NET_MAX_PEERS) == 0);
    for (int c = 0; c < 2; c++) use(c), net_set_on(false);
    g_cur = -1;
}

// 14. (s188 G1) Melty Net is asked only when Melty launched this copy; (G2) a refused / closed / silent room = the client
// stopped, back to UDP (with a pin list) or solo; (G9) sora_mod_unload leaves the room
static void test_gate_and_fallback(void) {
    printf("14. gate (MELTY_NET_DIR / SAVAGEWIND_SORA_MELTY), fall-back to UDP, unload\n");
    // G1: no Melty launch -> never asked, even if a melty.properties would be found (start_ret 1) and net is on
    env_set("SAVAGEWIND_SORA_MELTY", NULL), env_set("MELTY_NET_DIR", NULL);
    reset_all(), fk_reset(), FK.start_ret = 1;
    env_set("SAVAGEWIND_SORA_NET", "1");
    net_init_from_env();
    CHECK(FK.start_n == 0 && !NET.melty && NET.on && NET.sock.ok);
    net_set_on(false);
    // MELTY_NET_DIR set = launched by Melty; blank does not count; SAVAGEWIND_SORA_MELTY=0 = never
    reset_all(), fk_reset(), FK.start_ret = 1;
    env_set("MELTY_NET_DIR", "D:\\melty\\contract");
    net_init_from_env();
    CHECK(FK.start_n == 1 && NET.melty && NET.on && !NET.sock.ok);
    net_set_on(false);
    reset_all(), fk_reset(), FK.start_ret = 1;
    env_set("MELTY_NET_DIR", "  \t");
    net_init_from_env();
    CHECK(FK.start_n == 0 && !NET.melty);
    reset_all(), fk_reset(), FK.start_ret = 1;
    env_set("MELTY_NET_DIR", "D:\\melty\\contract"), env_set("SAVAGEWIND_SORA_MELTY", "0");
    net_init_from_env();
    CHECK(FK.start_n == 0 && !NET.melty && !NET.on);
    env_set("MELTY_NET_DIR", NULL), env_set("SAVAGEWIND_SORA_MELTY", "1");
    // launched by Melty but the client did not start: stopped, said, UDP as usual
    reset_all(), fk_reset(), FK.start_ret = 0;
    net_init_from_env();
    CHECK(FK.start_n == 1 && FK.stop_n == 1 && !NET.melty && !NET.on && strstr(g_log, "Melty Net did not start"));
    // (s189 review S2) ... with the exe's Melty default SAVAGEWIND_SORA_NET=1 (main.c) too: no pin list = no UDP port
    reset_all(), fk_reset(), FK.start_ret = 0;
    env_set("SAVAGEWIND_SORA_NET", "1");
    net_init_from_env();
    CHECK(FK.start_n == 1 && FK.stop_n == 1 && !NET.melty && !NET.on && !NET.sock.ok && strstr(g_log, "playing solo (no UDP port opened)"));
    CHECK(net_live_set("net", 1) && NET.on && NET.sock.ok);  // ("set net 1" by hand still turns LAN play on)
    net_set_on(false);
    // ... and with a pin list: UDP
    reset_all(), fk_reset(), FK.start_ret = 0;
    env_set("SAVAGEWIND_SORA_NET", "1"), env_set("SAVAGEWIND_SORA_NET_PEERS", "100.64.0.9");
    net_init_from_env();
    CHECK(!NET.melty && NET.on && NET.sock.ok && net_npins() == 1 && strstr(g_log, "UDP with your peer list"));
    net_set_on(false);
    // G2: the room closes for good (state -1) while we play with one friend: no pins = solo, no socket, slot freed
    boot(5);
    fk_join(7, "Riku");
    frame(T0, true);
    CHECK(NET.melty && slot_of(7) >= 0);
    FK.state = -1, FK.connected = 0, FK.self = 0;
    int out0 = FK.nout_all;
    frame(T0 + 50, true);
    CHECK(!NET.melty && FK.stop_n == 1 && !NET.on && !NET.sock.ok && netslot_count(NET.slot, NET_MAX_PEERS) == 0);
    CHECK(NET.nmroom == 0 && NET.melty_self == 0 && strstr(g_log, "Melty Net is off for this run") && strstr(g_log, "playing solo"));
    int fr0 = FK.frame_n;
    for (int i = 0; i < 5; i++) frame(T0 + 100 + 20 * i, true);
    CHECK(FK.frame_n == fr0 && FK.nout_all == out0 && FK.stop_n == 1);  // (the room is never touched again)
    // ... with a pin list (a Tailscale / LAN setup): UDP comes on in its place
    boot(5);
    env_set("SAVAGEWIND_SORA_NET_PEERS", "100.64.0.9");
    frame(T0, true);
    FK.state = -1, FK.connected = 0, FK.self = 0;
    frame(T0 + 50, true);
    CHECK(!NET.melty && NET.on && NET.sock.ok && net_npins() == 1 && strstr(g_log, "back to the UDP path"));
    net_set_on(false);
    // (s189 review S1) the room closes while Sora mode is off (parked): no pins = Sora coming back stays solo, no socket
    boot(5);
    frame(T0, true);
    net_sora_off();
    CHECK(NET.melty && !NET.on && NET.parked);
    FK.state = -1, FK.connected = 0, FK.self = 0;
    net_sora_off();
    CHECK(!NET.melty && !NET.on && !NET.parked && strstr(g_log, "playing solo"));
    frame(T0 + 200, true);
    CHECK(!NET.on && !NET.sock.ok);
    // ... with a pin list: still parked, UDP resumes when Sora is back
    boot(5);
    env_set("SAVAGEWIND_SORA_NET_PEERS", "100.64.0.9");
    frame(T0, true);
    net_sora_off();
    FK.state = -1, FK.connected = 0, FK.self = 0;
    net_sora_off();
    CHECK(!NET.melty && !NET.on && NET.parked && strstr(g_log, "back to the UDP path"));
    frame(T0 + 200, true);
    CHECK(NET.on && NET.sock.ok && net_npins() == 1);
    net_set_on(false);
    // no seat ever: still waiting at 30 s (said at 20 s), dropped after 60 s
    boot(5);
    FK.connected = 0, FK.self = 0, FK.state = 1;
    frame(T0 + 30000, true);
    CHECK(NET.melty && FK.stop_n == 0 && strstr(g_log, "not in the room after 20 s"));
    frame(T0 + 70000, true);
    CHECK(!NET.melty && FK.stop_n == 1 && strstr(g_log, "no seat in the room after 60 s"));
    // ... but a seat we had and lost for a while (a network blip: state 1) is NOT dropped by the 60 s rule
    boot(5);
    frame(T0, true);
    CHECK(NET.melty_self == 5);
    FK.connected = 0, FK.self = 0, FK.state = 1;
    frame(T0 + 90000, true);
    CHECK(NET.melty && FK.stop_n == 0);
    // G9: the DLL copy goes (sora_mod_unload: net off, then out of the room)
    net_set_on(false);
    net_melty_unload();
    CHECK(!NET.melty && FK.stop_n == 1 && strstr(g_log, "left the room"));
    net_melty_unload();
    CHECK(FK.stop_n == 1);  // (once)
}

// (s191) a value written by a stand-in leader (not a copy), straight into the room's story
static void fst_raw(double hour, double rate, int len) {
    ClockVal v = {FST.now, hour, rate, -1, -1, -1};
    uint8_t b[CLOCK_WIRE_LEN];
    clock_encode(&v, b);
    int k = fst_kv(CLOCK_KEY);
    if (k < 0) k = FST.nkv++, snprintf(FST.kv[k].key, sizeof FST.kv[k].key, "%s", CLOCK_KEY);
    memcpy(FST.kv[k].v, b, (size_t)len), FST.kv[k].len = len;
    fst_all(CLOCK_KEY, b, len, 0);
}
// one frame of this copy: the room clock and our game's clock move 16 ms
static uint64_t g_fnow;
static void cframe(bool driven) {
    g_fnow += 16, FST.now += 16;
    fg_run(16);
    frame(g_fnow, driven);
}
static bool hnear(double a, double b, double e) { return clock_dist(a, b) <= e; }
// (s198 b) a fake of the exe's fs_save_event ring (services/fs.c): events in order, the reader keeps its own seq
static struct { int slot[64], kind[64]; uint64_t n; } FSEV;
static void fsev_push(int slot, int kind) { FSEV.slot[FSEV.n % 64] = slot, FSEV.kind[FSEV.n % 64] = kind, FSEV.n++; }
static int fsev_get(uint64_t *seq, int *slot, int *kind, char *dir, int n) {
    (void)dir, (void)n;
    if (*seq >= FSEV.n) return 0;
    *slot = FSEV.slot[*seq % 64], *kind = FSEV.kind[*seq % 64], ++*seq;
    return 1;
}

// 15. (s191, spec 4.4) the room clock through the glue, one copy
static void test_clock_glue(void) {
    printf("15. room clock glue: UDP / solo untouched, the fake clock, leader write, story drain, mclock 0, unload\n");
    g_fnow = T0 + 200000;
    // UDP (Melty did not launch us): no story call, no [clock] line, the hour untouched
    reset_all(), fk_reset(), fst_reset(), ck_reset(12);
    env_set("SAVAGEWIND_SORA_MELTY", NULL), env_set("SAVAGEWIND_SORA_NET", "1");  // (after reset_all: it clears SAVAGEWIND_SORA_NET)
    FK.start_ret = 0;
    net_init_from_env();
    CHECK(!NET.melty && NET.on);
    for (int i = 0; i < 20; i++) cframe(true);
    CHECK(FST.calls == 0 && !strstr(g_log, "[clock]") && g_fg.holds == 0 && !g_fg.held && CK.c.role == CLOCK_OFF);
    // the dev aid "set mclockfake <hour>": the guest path with a local fake clock, also outside a room
    CHECK(net_live_set("mclockfake", 21));
    cframe(true);
    CHECK(g_fg.held && hnear(g_fg.hour, 21, 1.0 / 60) && CK.c.role == CLOCK_GUEST && strstr(g_log, "[clock] fake room clock: hour 21.00"));
    {
        char t[24];
        ck_state(t, sizeof t);
        CHECK(!strncmp(t, "21.0", 4) && t[strlen(t) - 2] == 'G' && t[strlen(t) - 1] == 'h');
    }
    ck_fake(6.5f);
    cframe(true);
    CHECK(g_fg.held && hnear(g_fg.hour, 6.5, 1.0 / 60) && strstr(g_log, "(the room's clock jumped)"));
    CHECK(net_live_set("mclockfake", -1));
    CHECK(!g_fg.held && g_fg.releases == 1 && CK.c.role == CLOCK_OFF && strstr(g_log, "[clock] fake room clock off"));
    int h0 = g_fg.holds;
    for (int i = 0; i < 10; i++) cframe(true);
    CHECK(g_fg.holds == h0 && FST.calls == 0);
    ck_fake(30);  // (refused)
    CHECK(CK.fake < 0 && strstr(g_log, "mclockfake 30.00 refused"));
    // (task 2) the fake value's weather / Blood Moon count; the same hour again (a live-file rewrite) changes nothing
    ck_fake(9);
    cframe(true);
    h0 = g_fg.holds;
    ck_fake(9);
    CHECK(net_live_set("mclockfakewx", 2) && CK.c.val.weather == 2 && CK.c.val.moons == 0 && CK.c.val.climate == -1);
    CHECK(net_live_set("mclockfakebm", 1) && CK.c.val.moons == 1 && CK.c.val.weather == 2);
    CHECK(net_live_set("mclockfakecl", 4) && CK.c.val.climate == 4);
    CHECK(net_live_set("mclockfakewx", -1) && CK.c.val.weather == -1);
    cframe(true);
    CHECK(g_fg.holds == h0 && hnear(g_fg.hour, 9, 1.0 / 60));  // (no jump back: the fake value goes on from its hour now)
    CHECK(net_live_set("mclockwx", 2) && CK.wxmode == 2 && strstr(g_log, "whatever the climate"));
    net_live_set("mclockwx", 1);
    // (s198) set mclocktake 0 = the s191 new-leader behaviour, copied into the core on the next frame; 1 again = the default
    CHECK(CK.take == 1 && net_live_set("mclocktake", 0) && CK.take == 0 && strstr(g_log, "the s191 behaviour"));
    cframe(true);
    CHECK(!CK.c.take_on);
    CHECK(net_live_set("mclocktake", 1) && CK.take == 1);
    cframe(true);
    CHECK(CK.c.take_on);
    // set mclocklead 1: the leader core against a loopback (a local ms clock): the first write after a second, nothing sent
    CHECK(net_live_set("mclocklead", 1) && CK.fake < 0 && strstr(g_log, "mclockfake is on") == NULL);
    {
        uint64_t t_end = GetTickCount64() + 1300;
        while (GetTickCount64() < t_end) {
            cframe(true);
#ifdef _WIN32
            Sleep(15);
#else
            nanosleep(&(struct timespec){0, 15000000L}, NULL);
#endif
        }
    }
    CHECK(CK.c.role == CLOCK_LEADER && strstr(g_log, "[clock] would write: botw.clock:world") && strstr(g_log, "(27 bytes; set mclocklead"));
    CHECK(FST.calls == 0 && !g_fg.held);
    CHECK(net_live_set("mclocklead", 0));
    CHECK(CK.c.role == CLOCK_OFF);
    net_set_on(false);
    env_set("SAVAGEWIND_SORA_NET", NULL), env_set("SAVAGEWIND_SORA_MELTY", "1");

    // alone in a room: off until the story is in, then the leader; writes the key once the rate is measured
    boot(5);
    fst_reset(), ck_reset(9.25f);
    fst_join(5);
    for (int i = 0; i < 10; i++) cframe(true);
    CHECK(CK.on == 1 && CK.c.role == CLOCK_OFF && strstr(g_log, "room clock on (default: on when Melty started the game)"));
    fst_deliver();
    for (int i = 0; i < 70; i++) cframe(true);
    int k = fst_kv(CLOCK_KEY);
    ClockVal v;
    CHECK(CK.c.role == CLOCK_LEADER && FST.sets == 1 && k >= 0 && FST.kv[k].len == 27 && clock_decode(FST.kv[k].v, 27, &v));
    CHECK(hnear(clock_at(&v, FST.now), g_fg.hour, 0.001) && v.rate == CLOCK_RATE_NORMAL && v.weather == -1 && v.moons == 0);
    CHECK(!g_fg.held && g_fg.holds == 0 && strstr(g_log, "[clock] off -> leader (we lead the room's story)"));
    // the drain: other keys and the guests' triggers are taken out (nothing piles up), the clock key goes to the clock
    FstMem *me = fst_me();
    fst_push(me, "botw.flag:Clear_Dungeon012", "\1", 1, 0);
    fst_push(me, "botw.dead:3201844131", "\1", 1, 0);
    me->t[me->nt].from = 9, snprintf(me->t[me->nt].key, 64, "botw.dead:1"), me->t[me->nt++].len = 0;
    cframe(true);
    CHECK(me->rq == me->nq && me->rt == me->nt);
    // set mclock 0: off (the leader held nothing), set mclock 1: back
    CHECK(net_live_set("mclock", 0) && CK.on == 0);
    cframe(true);
    CHECK(CK.c.role == CLOCK_OFF && strstr(g_log, "[clock] leader -> off"));
    CHECK(net_live_set("mclock", 1));
    cframe(true);
    CHECK(CK.c.role == CLOCK_LEADER);
    // (s198 b) BotW's save reads come from the exe's fs ring every frame: one from before our first look is passed over, a
    // read while that slot's write is not written out is a check, a read = a load (the leader keeps the room's clock)
    fsev_push(3, 0);
    CK.save_ev = fsev_get;
    cframe(true);
    CHECK(!CK.c.reload && CK.sev_live);
    fsev_push(3, 1), fsev_push(3, 0);
    cframe(true);
    CHECK(!CK.c.reload);
    fsev_push(3, 2), fsev_push(3, 0);
    cframe(true);
    CHECK(CK.c.reload && CK.c.st.reloads == 1 && strstr(g_log, "[clock] leader: our game reads a save (slot 3)"));
    CK.save_ev = NULL;
    net_set_on(false);
    net_melty_unload();

    // a guest (a stand-in leads): its hour is held where the room says; unload releases it; mclock 0 too
    boot(5);
    fst_reset(), ck_reset(3);
    fst_join(3), fst_join(5), fst_deliver();
    fst_raw(17.5, CLOCK_RATE_NORMAL, 24);  // (24 bytes, as Melty's connector test writes it)
    cframe(true);
    CHECK(CK.c.role == CLOCK_GUEST && g_fg.held && hnear(g_fg.hour, 17.5, 0.01) && g_fg.holds == 1);
    for (int i = 0; i < 200; i++) cframe(i & 1);  // (3.2 s, driven or not: once a second)
    CHECK(g_fg.holds >= 3 && g_fg.holds <= 5 && hnear(g_fg.hour, 17.5 + 3.2 / 60, 1.0 / 60));
    CHECK(net_live_set("mclock", 0));
    cframe(true);
    CHECK(!g_fg.held && g_fg.releases == 1 && CK.c.role == CLOCK_OFF);
    CHECK(net_live_set("mclock", 1));
    cframe(true);
    CHECK(g_fg.held && CK.c.role == CLOCK_GUEST);
    // Sora mode off: the clock still runs (net_sora_off)
    h0 = g_fg.holds;
    for (int i = 0; i < 70; i++) FST.now += 16, net_sora_off();
    CHECK(g_fg.holds > h0 && g_fg.held);
    // the room drops (closed for good): released
    FK.state = -1, FK.connected = 0, FK.self = 0;
    cframe(true);
    CHECK(!NET.melty && !g_fg.held && g_fg.releases == 2 && CK.c.role == CLOCK_OFF);
    // unload while a guest: released
    boot(5);
    ck_reset(3);
    fst_raw(10, 0, 26);
    cframe(true);
    CHECK(g_fg.held && hnear(g_fg.hour, 10, 1e-4));
    net_set_on(false);
    net_melty_unload();
    CHECK(!g_fg.held && g_fg.releases == 1 && strstr(g_log, "released the hour (this copy of the mod is going)"));
}

// 16. (s191) two copies, one room story: the leader's clock reaches the guest; the leader leaves, the guest leads
static void test_clock_two_copies(void) {
    printf("16. room clock, two copies: the guest follows the leader's hour; the leader leaves, the guest takes over\n");
    g_cur = -1;
    fst_reset();
    for (int c = 0; c < 2; c++) {
        boot(c ? 9 : 4);
        g_cp[c].id = c ? 9 : 4;
        fk_join(c ? 4 : 9, c ? "Sora" : "Riku");
        ck_reset(c ? 20.0f : 9.0f);
        Copy *o = &g_cp[c];
        o->net = NET, o->ed = NETED, o->fi = NETF, o->es = NETES, o->fk = FK, o->nap = 0, o->nfr = 0, o->ck = CK, o->fg = g_fg;
    }
    fst_join(4), fst_join(9), fst_deliver();
    uint64_t now = T0 + 400000;
    // both games' clocks run (unless held); the room clock moves once per frame of the pair
    #define PAIR_FRAMES(n, both) for (int f = 0; f < (n); f++) { now += 16, FST.now += 16; \
        for (int c = 0; c < ((both) ? 2 : 1); c++) { int cc = (both) ? c : 1; use(cc); fg_run(16); frame(now, true); route(cc); } }
    PAIR_FRAMES(190, true);  // 3 s
    use(0);
    float leader_hour = g_fg.hour;
    CHECK(CK.c.role == CLOCK_LEADER && !g_fg.held && g_fg.holds == 0);
    use(1);
    CHECK(CK.c.role == CLOCK_GUEST && g_fg.held && hnear(g_fg.hour, leader_hour, 1.0 / 60 + 0.001));
    CHECK(strstr(g_log, "guest: hour 9.") != NULL);
    // the leader sleeps at a campfire (+6 h): the guest follows at once
    use(0);
    g_fg.hour = (float)clock_wrap24(g_fg.hour + 6);
    PAIR_FRAMES(3, true);
    use(0);
    leader_hour = g_fg.hour;
    use(1);
    CHECK(hnear(g_fg.hour, leader_hour, 1.0 / 60 + 0.001));
    // the leader's copy leaves the room: the guest's game leads, its hold goes, it writes the key from its synced hour
    int sets0 = FST.sets;
    fst_leave(4);
    use(1);
    fk_leave(4);
    float synced = g_fg.hour;
    PAIR_FRAMES(1, false);
    use(1);
    CHECK(CK.c.role == CLOCK_LEADER && !g_fg.held && g_fg.releases == 1 && strstr(g_log, "[clock] guest -> leader"));
    PAIR_FRAMES(75, false);  // 1.2 s
    use(1);
    int k = fst_kv(CLOCK_KEY);
    ClockVal v;
    CHECK(FST.sets > sets0 && k >= 0 && clock_decode(FST.kv[k].v, FST.kv[k].len, &v) && hnear(v.hour, synced + 1.2 / 60, 1.5 / 60));
    CHECK(hnear(g_fg.hour, synced + 1.2 / 60, 0.01));  // (its own clock runs on from the room's hour)
    // the first copy comes back (a rejoin): it obeys the new leader once it has the story
    use(0);
    fst_join(4);
    fk_join(9, "Riku");
    PAIR_FRAMES(5, true);
    use(0);
    CHECK(CK.c.role == CLOCK_OFF);
    fst_deliver();
    PAIR_FRAMES(5, true);
    use(1);
    float lead = g_fg.hour;
    use(0);
    CHECK(CK.c.role == CLOCK_GUEST && g_fg.held && hnear(g_fg.hour, lead, 1.0 / 60 + 0.001) && strstr(g_log, "[clock] off -> guest"));
    #undef PAIR_FRAMES
    for (int c = 0; c < 2; c++) use(c), net_set_on(false), net_melty_unload();
    g_cur = -1;
}

// 17. (s191 task 2, spec 4.1) areas in a room: our AREA is a room event to everyone (on a change, on a join, every NET_AREA_MELTY_MS:
// the room delivers events reliably); a room player on another map is hidden (his SNAPs still kept in his slot, his FIRE dropped),
// shown again when we are on his map; EDMG both ways is never filtered; a room that ALSO hides him (no SNAPs at all) changes nothing.
// (The connector itself delivers every PLAYER / LANE entity whatever its level, sora_melty.cpp sora_melty_frame.)
static int fk_last_area(char out[NET_AREA_MAX + 1]) {  // AREA sends recorded; out = the last one's text
    int c = 0;
    for (int i = 0; i < FK.nout; i++)
        if (FK.out[i].type == NET_AREA && net_unpack_area(FK.out[i].b, FK.out[i].len, NULL, NULL, out)) c++;
    return c;
}
static void test_areas_room(void) {
    printf("17. areas: AREA = a room event (change / join / 10 s); another map = hidden, FIRE dropped; EDMG always\n");
    boot(5);
    fk_join(7, "Riku");
    net_set_area("Dungeon012");
    frame(T0, true);
    char a[NET_AREA_MAX + 1] = "";
    int na = -1, np = -1;
    NetKillsMsg km;
    CHECK(!strcmp(FK.area, "Dungeon012") && fk_last_area(a) == 1 && !strcmp(a, "Dungeon012") && fk_all_to_everyone_but_kills(&na, &np));
    CHECK(na == 1 && np == 0 && fk_kills(NKO_ASK, 7, &km) == 1);  // (s197 killsync on by default: Riku is asked for his kill list, to him only)
    for (int i = 1; i < 99; i++) frame(T0 + 100 * (uint64_t)i, true);  // (9.9 s: no repeat; UDP would have sent 4 more)
    CHECK(fk_last_area(a) == 1);
    frame(T0 + NET_AREA_MELTY_MS, true);
    CHECK(fk_last_area(a) == 2);
    fk_join(8, "Kairi");  // a join: ours goes at once
    frame(T0 + NET_AREA_MELTY_MS + 100, true);
    CHECK(fk_last_area(a) == 3);
    int k = slot_of(7);
    CHECK(k >= 0);
    // Riku is on MainField: hidden; his SNAP still goes into his slot; his Fire is not burnt here; his hit is applied
    uint8_t b[NET_PKT_MAX + 64];
    uint64_t t = T0 + NET_AREA_MELTY_MS + 200;
    fk_in(7, b, net_pack_area(7, 1, "MainField", b, sizeof b));
    fk_in(7, b, mk_snap(7, 10, 42.0f, b, sizeof b));
    fk_in(7, b, mk_fire(7, 600, b, sizeof b));
    fk_in(7, b, mk_edmg(7, 500, 0x4444, b, sizeof b));
    frame(t, true);
    CHECK(NET.slot[k].hidden && NET.slot[k].st.snap == 1 && NET.slot[k].p.ring[0].feet[0] == 42.0f && g_nfr == 0 && NETF.st.rx_area == 1 && g_nap == 1);
    CHECK(count_str(g_log, "[net] area: peer \"Riku\" is on MainField (we are on Dungeon012): hidden") == 1);
    // our EDMG / FIRE still go to everyone (EDMG: hids are unique; FIRE: the receivers judge it by our AREA)
    int e0 = fk_count(NET_EDMG), f0 = fk_count(NET_FIRE);
    queue_hit(0xAAAA0002, 5, 8, 1);
    float c[3] = {10, 0, -20};
    net_fire_note(c, true, false, 2.6f, 1.3f, 0);
    frame(t + 100, true);
    CHECK(fk_count(NET_EDMG) == e0 + 1 && fk_count(NET_FIRE) == f0 + 1 && fk_all_to_everyone_but_kills(&na, &np));
    // (s197) Riku (never answers) asked at T0, +5 s, +10 s; Kairi once at her join: each to him / her only
    CHECK(na == 4 && np == 0 && fk_kills(NKO_ASK, 7, &km) == 3 && fk_kills(NKO_ASK, 8, &km) == 1);
    // the room ALSO hides him (no SNAPs come for 30 s): the slot is kept (a room player leaves only when the room says so), no more lines
    for (int i = 2; i < 300; i++) frame(t + 100 * (uint64_t)i, true);
    t += 30000;
    CHECK(slot_of(7) == k && NET.slot[k].hidden && count_str(g_log, "[net] area: peer \"Riku\"") == 1);
    // we walk out onto MainField: shown at once (his AREA is 30 s old: still known, Melty forgets after 5 x 10 s), his Fire burns here
    int a0 = fk_last_area(a);
    CHECK(a0 == 6);  // (3 + the 10 s repeats at 20, 30 and 40 s)
    net_set_area("MainField");
    fk_in(7, b, mk_snap(7, 11, 43.0f, b, sizeof b));
    fk_in(7, b, mk_fire(7, 601, b, sizeof b));
    frame(t, true);
    CHECK(!NET.slot[k].hidden && g_nfr == 1 && fk_last_area(a) == a0 + 1 && !strcmp(a, "MainField"));
    CHECK(count_str(g_log, "[net] area: peer \"Riku\" is on MainField (we are on MainField): shown") == 1);
    net_set_area(NULL);
    net_set_on(false);
}

// 18. (s194) NET_REACT in a room: a hit on a puppet goes out ONCE to everyone and waits while the room is not connected; a room
// player's reaction is taken once and queued for the frame body; a room id that never joined is dropped
static void test_react_room(void) {
    printf("18. NET_REACT: once to everyone, waits for the connection, received from a room player\n");
    boot(5);
    fk_join(7, "Riku");
    frame(T0, false);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 1);
    g_net_react = true, g_net_esync = true, NETES.active = true, NETES.c.me = NET.me;
    EsEnt *p = es_get(&NETES.c, 0x88, T0);
    p->local = true, p->owner = 7, p->brain = 0;  // (Riku owns it, a puppet here)
    float f[3] = {1, 2, 3};
    CHECK(net_react_wanted() && net_react_hit(0x88, f, 3, 125, 0, 7, "blade", false));
    frame(T0 + 100, false);
    for (int i = 1; i < 6; i++) frame(T0 + 100 + i * NET_EDMG_REP_MS, false);
    int na = -1, np = -1;
    NetKillsMsg km;
    CHECK(fk_count(NET_REACT) == 1 && NETR.st.tx_pkt == 1 && NETR.st.tx_send == 1 && NETR.nout == 0 && fk_all_to_everyone_but_kills(&na, &np));
    CHECK(na == 1 && np == 0 && fk_kills(NKO_ASK, 7, &km) == 1);  // (s197 killsync on by default: Riku's kill-list ask, to him only)
    {
        NetReactEnt e[NET_REACT_MAX]; uint8_t pl = 0; uint16_t sq = 0;
        int seen = 0;
        for (int i = 0; i < FK.nout; i++)
            if (FK.out[i].type == NET_REACT) seen += net_unpack_react(FK.out[i].b, FK.out[i].len, &pl, &sq, e, NULL) == 1 && pl == NET.me && e[0].hid == 0x88;
        CHECK(seen == 1);
    }
    // the room drops out: the packet waits (not lost, not repeated), goes out once when it is back
    FK.connected = 0;
    CHECK(net_react_hit(0x88, f, 4, 30, 0, 0x14, "thunder", true));
    for (int i = 0; i < 5; i++) frame(T0 + 1000 + i * NET_EDMG_REP_MS, false);
    CHECK(fk_count(NET_REACT) == 1 && NETR.nout == 1);
    FK.connected = 1;
    for (int i = 0; i < 5; i++) frame(T0 + 2000 + i * NET_EDMG_REP_MS, false);
    CHECK(fk_count(NET_REACT) == 2 && NETR.nout == 0 && NETR.st.tx_send == 2);
    // received: from Riku, twice the same seq (a stall's backlog): taken once, queued; a room id that never joined: dropped
    uint8_t b[NET_PKT_MAX];
    int n = mk_react(7, 300, b, sizeof b);
    fk_in(7, b, n), fk_in(7, b, n);
    fk_in(99, b, mk_react(99, 301, b, sizeof b));
    frame(T0 + 3000, false);
    CHECK(NETR.st.rx_pkt == 1 && NETR.st.rx_dup == 1 && NETR.rx.n == 1 && NETR.rx.q[0].e.hid == 0x77);
    g_net_react = false, g_net_esync = false, NETES.active = false;
    net_set_on(false);
}

// 19. (s196) NET_KILLS in a room: a new room player is asked ONCE, to him only (no re-ask before KS_REASK_MELTY_MS, none once done);
// his parts are taken; his ask is answered once per part, to him only; nothing goes out while the room is not connected
// (fk_kills is defined up with fk_all_to_everyone)
static void test_kills_room(void) {
    printf("19. NET_KILLS: a room player asked once, to him only; answered once per part, to him only\n");
    boot(5);
    // (s197) ON with no live key: the compiled default, said once at net on; `set killsync 1` is then a no-op (no second line)
    CHECK(NET_KILLS_DEFAULT && g_net_kills && count_str(g_log, "[kills] kill sync on (the compiled default since s197, no live key needed") == 1);
    CHECK(net_live_set("killsync", 1) && g_net_kills && count_str(g_log, "[kills] kill sync") == 1);
    fk_join(7, "Riku");
    frame(T0, false);
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 1);
    NetKillsMsg m;
    CHECK(fk_kills(NKO_ASK, 7, &m) == 1 && fk_kills(NKO_ASK, -1, &m) == 1 && m.have == 0 && m.nonce == NETK.ask[0].nonce);
    for (int i = 1; i < 40; i++) frame(T0 + 100 * i, false);  // (4 s: no re-ask in a room before 5 s)
    CHECK(fk_kills(NKO_ASK, -1, &m) == 1);
    uint8_t b[NET_KILLS_PKT];
    uint32_t h[2] = {0x1001, 0x1002};
    uint16_t ag[2] = {5, 6};
    fk_in(7, b, net_pack_kills_part(7, 1, m.nonce, 3, 0, 1, h, ag, 2, b, (int)sizeof b));
    frame(T0 + 4100, false);
    CHECK(NETK.L.n == 2 && NETK.ask[0].done && NETK.L.src[0] == KSRC_CATCH);
    for (int i = 0; i < 100; i++) frame(T0 + 4200 + 100 * i, false);  // (done: never asked again)
    CHECK(fk_kills(NKO_ASK, -1, &m) == 1);
    // his ask: our 2 kills in one part, to him
    fk_in(7, b, net_pack_kills_ask(7, 2, 0xABCD, 0, 0, NET_KILLS_MOON_NONE, b, (int)sizeof b));
    frame(T0 + 15000, false);
    frame(T0 + 15016, false);
    CHECK(fk_kills(NKO_PART, 7, &m) == 1 && fk_kills(NKO_PART, -1, &m) == 1 && m.nonce == 0xABCD && m.k == 2 && m.hid[1] == 0x1002 && m.n == 1);
    // the room drops out: his next ask (s197 S3: past KS_ANS_NEW_MS after the last) waits for it, then once
    FK.connected = 0;
    fk_in(7, b, net_pack_kills_ask(7, 3, 0xABCE, 0, 0, NET_KILLS_MOON_NONE, b, (int)sizeof b));
    for (int i = 0; i < 5; i++) frame(T0 + 15000 + KS_ANS_NEW_MS + 100 + 16 * i, false);
    CHECK(fk_kills(NKO_PART, -1, &m) == 1);
    FK.connected = 1;
    for (int i = 0; i < 5; i++) frame(T0 + 18000 + 16 * i, false);
    CHECK(fk_kills(NKO_PART, 7, &m) == 2 && m.nonce == 0xABCE);
    // (s197) `set killsync 0` still turns it all off: list gone, a new room player is not asked, Riku's ask is not answered
    int out0 = FK.nout;
    CHECK(net_live_set("killsync", 0) && !g_net_kills && NETK.L.n == 0 && strstr(g_log, "[kills] kill sync off"));
    fk_join(8, "Kairi");
    fk_in(7, b, net_pack_kills_ask(7, 4, 0xABCF, 0, 0, NET_KILLS_MOON_NONE, b, (int)sizeof b));
    for (int i = 0; i < 70; i++) frame(T0 + 19000 + 100 * i, false);  // (7 s: past a room re-ask too)
    bool kills_out = false;
    for (int i = out0; i < FK.nout; i++) kills_out = kills_out || FK.out[i].type == NET_KILLS;
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == 2 && !kills_out && NETK.st.rx_off == 1 && NETK.L.n == 0);
    net_set_on(false);
}

// 20. (s197 review M1) a room's Blood Moon is shared by the room clock, but a guest's can be OWED (he is in a shrine): Riku (host) had
// it at Tm, our count only goes up when ours plays later. The age rule keeps both directions safe: Riku's reload gets none of our
// pre-Tm kills; our reload after OUR moon drops Riku's kills made between his moon and ours. (s197 S2: kills in the shrine are not kept.)
static void test_kills_owed_moon(void) {
    printf("20. NET_KILLS + an owed Blood Moon in a room: the age rule both ways\n");
    boot(5);
    NETK.moons_fn = fake_moons, g_mv = 2;
    fk_join(7, "Riku");
    frame(T0, false);
    NetKillsMsg m;
    CHECK(fk_kills(NKO_ASK, 7, &m) == 1 && m.moon == NET_KILLS_MOON_NONE);
    uint8_t b[NET_KILLS_PKT];
    fk_in(7, b, net_pack_kills_part(7, 1, m.nonce, 1, 0, 1, NULL, NULL, 0, b, (int)sizeof b));  // (his list: empty)
    frame(T0 + 100, false);
    CHECK(NETK.ask[0].done && NETK.moons_known && NETK.moons == 2);
    queue_hit(0xF1, 40, 0, 1);
    frame(T0 + 1000, false);  // (our kill, before the room's moon)
    CHECK(NETK.L.n == 1 && ks_find(&NETK.L, 0xF1) == 0);
    uint64_t Tm = T0 + 10000;  // (the room's Blood Moon: Riku plays it; we are in a shrine: ours is owed, our count stays 2)
    g_open_v = 0;
    queue_hit(0xF0, 40, 0, 1);
    frame(Tm + 5000, false);  // (a kill in the shrine: not kept)
    CHECK(NETK.L.n == 1 && ks_find(&NETK.L, 0xF0) < 0 && NETK.st.not_open == 1);
    g_open_v = 1;
    queue_hit(0xF2, 40, 0, 1);
    frame(Tm + 10000, false);  // (out of the shrine, the moon still owed: a kill after the room's moon)
    CHECK(NETK.L.n == 2 && NETK.moons == 2 && !NETK.moon_seen);
    // Riku reloads 30 s after his moon and asks us: only F2 (20 s old) goes, F1 (39 s: before his moon) is held back
    int p0 = fk_kills(NKO_PART, 7, &m);
    fk_in(7, b, net_pack_kills_ask(7, 2, 0xC0DE, 0, 0, 30, b, (int)sizeof b));
    frame(Tm + 30000, false);
    frame(Tm + 30016, false);
    CHECK(fk_kills(NKO_PART, 7, &m) == p0 + 1 && m.nonce == 0xC0DE && m.k == 1 && m.hid[0] == 0xF2 && m.age[0] == 20);
    // our owed moon plays at last: the list goes, our moon time is now, the exe reset
    g_mv = 3;
    frame(Tm + 40000, false);
    CHECK(NETK.L.n == 0 && NETK.moon_seen && NETK.moon_t == Tm + 40000 && g_rst == 1 && NETK.st.clr_moon == 1);
    // we reload 10 s later: our ask says 10 s; Riku's answer has kills from between his moon and ours (dropped) and after ours
    net_kills_on_load(1);
    int a0 = fk_kills(NKO_ASK, 7, &m);
    frame(Tm + 50000, false);
    CHECK(fk_kills(NKO_ASK, 7, &m) == a0 + 1 && m.moon == 10);
    uint32_t h[4] = {0xE1, 0xE2, 0xE3, 0xE4};
    uint16_t ag[4] = {45, 5, 9, NET_KILLS_AGE_OLD};  // (Tm+5 s: after his moon, before ours; Tm+45 s; Tm+41 s: inside the slack; not known)
    fk_in(7, b, net_pack_kills_part(7, 3, m.nonce, 2, 0, 1, h, ag, 4, b, (int)sizeof b));
    frame(Tm + 50100, false);
    CHECK(NETK.ask[0].done && NETK.L.n == 1 && ks_find(&NETK.L, 0xE2) == 0 && NETK.st.hid_old == 3);
    net_set_on(false);
}

// 21. (s198 cap) five players in the room (we + 4 others; NET_MAX_PEERS 3): the 4th other has no slot, is never drawn, every packet
// of his is dropped with no side effect, he is not in the enemy election / kill catch-up, his "refused" line is said once; a slotted
// player's leave gives the free slot to the one who has waited LONGEST (roster kept in join order); a crowd past the roster's 16
// says each refusal once (no spam), and a leave of someone the roster never stored changes nothing
static void test_fifth_peer(void) {
    printf("21. five players in the room: the extra one is ignored cleanly; a free slot goes to the longest waiting\n");
    fst_reset(), ck_reset(12);
    boot(5);
    fk_join(20, "A"), fk_join(21, "B"), fk_join(22, "C"), fk_join(23, "D");
    frame(T0, false);
    const uint16_t X = 23;  // (the 4th other = the 5th player of the room)
    CHECK(netslot_count(NET.slot, NET_MAX_PEERS) == NET_MAX_PEERS && slot_of(20) >= 0 && slot_of(21) >= 0 && slot_of(22) >= 0);
    CHECK(slot_of(X) < 0 && NET.nmroom == 4);
    NetKillsMsg km;
    CHECK(fk_kills(NKO_ASK, X, &km) == 0 && fk_kills(NKO_ASK, 20, &km) == 1);  // (kill catch-up: the slotted are asked, he is not)
    for (int i = 1; i < 100; i++) frame(T0 + 16 * (uint64_t)i, false);
    CHECK(count_str(g_log, "refused") == 1 && count_str(g_log, "(Melty room peer 23)") == 1);  // (one line, names his room id)
    // every packet type from him: dropped before any handler (sora_net.h net_recv_all: no slot = NET.st.other), nothing applied
    g_net_react = true, g_net_esync = true;
    uint8_t b[NET_PKT_MAX + 64];
    for (int p = 20; p <= 22; p++) fk_in((uint16_t)p, b, mk_snap((uint8_t)p, 1, (float)p, b, sizeof b));
    frame(T0 + 2000, true);  // (the three slotted are live now)
    int other0 = (int)NET.st.other, nap0 = g_nap, nfr0 = g_nfr, kout0 = fk_kills(NKO_PART, -1, &km), react0 = (int)NETR.st.rx_pkt;
    uint32_t kills0 = NETK.st.rx_bad + NETK.st.hid_rx;
    fk_in(X, b, mk_snap((uint8_t)X, 1, 99.0f, b, sizeof b));
    fk_in(X, b, mk_edmg((uint8_t)X, 1, 0x5555, b, sizeof b));
    fk_in(X, b, mk_fire((uint8_t)X, 1, b, sizeof b));
    fk_in(X, b, net_pack_area((uint8_t)X, 1, "Dungeon012", b, sizeof b));
    fk_in(X, b, mk_react((uint8_t)X, 1, b, sizeof b));
    fk_in(X, b, mk_esync((uint8_t)X, 1, b, sizeof b));
    fk_in(X, b, net_pack_kills_ask((uint8_t)X, 1, 0xBEEF, 0, 0, NET_KILLS_MOON_NONE, b, (int)sizeof b));
    uint32_t hx[1] = {0x6666};
    uint16_t ax[1] = {3};
    fk_in(X, b, net_pack_kills_part((uint8_t)X, 2, 0x1234, 1, 0, 1, hx, ax, 1, b, (int)sizeof b));
    fk_in(X, b, mk_snap(5, 2, 98.0f, b, sizeof b));  // (and one that wears OUR player byte: no id clash re-roll for a stranger)
    uint8_t me0 = NET.me;
    frame(T0 + 2016, true);
    frame(T0 + 2032, true);
    CHECK((int)NET.st.other == other0 + 9 && slot_of(X) < 0 && NET.me == me0);
    CHECK(g_nap == nap0 && g_nfr == nfr0 && (int)NETR.st.rx_pkt == react0 && NETR.rx.n == 0);
    CHECK(fk_kills(NKO_PART, -1, &km) == kout0 && fk_kills(NKO_PART, X, &km) == 0 && NETK.st.rx_bad + NETK.st.hid_rx == kills0 && ks_find(&NETK.L, 0x6666) < 0);
    for (int k = 0; k < NET_MAX_PEERS; k++) CHECK(NET.slot[k].used && NET.slot[k].p.player != X && !NET.slot[k].hidden);
    // the enemy election: we + the 3 slotted, never him
    CHECK(NETES.c.np == 4);
    for (int i = 0; i < NETES.c.np; i++) CHECK(NETES.c.pl[i].id != X);
    // a story trigger of his in our queue (we lead the fake story): drained, nothing done with it
    fst_join(5), fst_join(X), fst_deliver();
    FstMem *me = fst_mem(5);
    CHECK(me != NULL);
    if (me) {
        FstTrig *t = &me->t[me->nt++];
        t->from = X, t->len = 0, snprintf(t->key, sizeof t->key, "botw.dead:1");
        frame(T0 + 2048, true);
        CHECK(me->rt == me->nt && fst_kv("botw.dead:1") < 0);
    }
    fst_reset(), ck_reset(12);
    g_net_react = false, g_net_esync = false, NETES.active = false;
    // a 6th joins (also waiting), then B leaves: D (waiting since T0) gets the slot, not the newer E
    fk_join(24, "E");
    frame(T0 + 3000, false);
    CHECK(slot_of(24) < 0 && count_str(g_log, "(Melty room peer 24)") == 1);
    int ask0 = fk_kills(NKO_ASK, X, &km);
    fk_leave(21);
    frame(T0 + 3016, false);
    CHECK(slot_of(21) < 0 && slot_of(X) >= 0 && slot_of(24) < 0 && NET.nmroom == 4);
    CHECK(count_str(g_log, "joined: D (Melty room peer 23)") == 1);
    CHECK(fk_kills(NKO_ASK, X, &km) == ask0 + 1);  // (slotted now: asked for his kill list like any new player)
    int k = slot_of(X);
    CHECK(k >= 0);
    if (k >= 0) {
        fk_in(X, b, mk_snap((uint8_t)X, 3, 77.0f, b, sizeof b));
        frame(T0 + 3032, false);
        CHECK(NET.slot[k].st.snap == 1 && NET.slot[k].p.st == PEER_LIVE);  // (his packets count from now on)
    }
    // a crowd: the roster fills to 16 (12 more stored, the rest not), 13 wait; 100 frames later each NEW refusal was said ONCE
    g_log[0] = 0;
    for (int i = 0; i < 30; i++) fk_join((uint16_t)(100 + i), "Crowd");
    for (int i = 0; i < 100; i++) frame(T0 + 4000 + 16 * (uint64_t)i, false);
    CHECK(NET.nmroom == 16 && netslot_count(NET.slot, NET_MAX_PEERS) == NET_MAX_PEERS);
    CHECK(count_str(g_log, "refused") == 12 && count_str(g_log, "(Melty room peer 1") == 12);  // (100..111 once each; E said before; 112..129 never stored)
    // a leave of someone the roster never stored (129): nothing changes; one stored (100) leaves: 15, slots as they were
    int k20 = slot_of(20), k22 = slot_of(22), kx = slot_of(X);
    fk_leave(129);
    frame(T0 + 6000, false);
    CHECK(NET.nmroom == 16 && slot_of(20) == k20 && slot_of(22) == k22 && slot_of(X) == kx);
    fk_leave(100);
    frame(T0 + 6016, false);
    CHECK(NET.nmroom == 15 && slot_of(20) == k20 && slot_of(22) == k22 && slot_of(X) == kx);
    // A leaves: the longest waiting of the rest is E (24), then C leaves: the crowd in join order (101)
    fk_leave(20);
    frame(T0 + 6032, false);
    CHECK(slot_of(24) >= 0 && slot_of(101) < 0);
    fk_leave(22);
    frame(T0 + 6048, false);
    CHECK(slot_of(101) >= 0 && slot_of(102) < 0);
    net_set_on(false);
}

// 22. (s207) the solo build: SAVAGEWIND_SORA_SOLO=1 (melty-setup/play.py COOP = False) = no room, no socket, whatever else says on
static void test_solo(void) {
    printf("22. solo build (SAVAGEWIND_SORA_SOLO=1): no room, no socket, nothing turns it on\n");
    env_set("SAVAGEWIND_SORA_SOLO", "1");
    // Melty launched us, the client would start, the exe's default NET=1 and a pin list: still nothing
    reset_all(), fk_reset(), FK.start_ret = 1, FK.self = 5, g_net_solo_said = false;
    env_set("SAVAGEWIND_SORA_NET", "1"), env_set("SAVAGEWIND_SORA_NET_PEERS", "100.64.0.9");
    net_init_from_env();
    CHECK(FK.start_n == 0 && !NET.melty && !NET.on && !NET.sock.ok && net_npins() == 0);
    CHECK(count_str(g_log, "[net] solo build: co-op is off (SAVAGEWIND_SORA_SOLO=1") == 1 && !strstr(g_log, "[net] on:") && !strstr(g_log, "pinned"));
    for (int i = 0; i < 5; i++) frame(T0 + 20 * i, true);
    CHECK(FK.frame_n == 0 && FK.nout_all == 0 && FK.recv_n == 0 && !NET.on && !NET.sock.ok);
    // "set net 1" (a live file) and a pin line: refused, no socket
    CHECK(net_live_set("net", 1) && !NET.on && !NET.sock.ok && strstr(g_log, "[net] still off (solo build)"));
    CHECK(!strstr(g_log, "net stays off"));  // (said at the start already: once a copy)
    net_live_line("net peer 100.64.0.10");
    CHECK(net_live_set("net", 1) && !NET.on && !NET.sock.ok);
    // Sora mode off and back: nothing resumes
    net_sora_off();
    frame(T0 + 500, true);
    CHECK(!NET.on && !NET.sock.ok && !NET.parked && FK.start_n == 0);
    // MELTY_NET_DIR (Melty's lab) and SAVAGEWIND_SORA_MELTY=1: still never asked
    reset_all(), fk_reset(), FK.start_ret = 1;
    env_set("MELTY_NET_DIR", "D:\\melty\\contract");
    net_init_from_env();
    CHECK(FK.start_n == 0 && !NET.melty && !NET.on);
    env_set("MELTY_NET_DIR", NULL);
    // net_set_on(true) itself (any other path) is refused, and says it once when the start did not
    reset_all(), fk_reset(), g_net_solo_said = false;
    NET.inited = true;
    net_set_on(true);
    net_set_on(true);
    CHECK(!NET.on && !NET.sock.ok && count_str(g_log, "[net] solo build: co-op is off, net stays off") == 1);
    // SOLO=0 / unset: as before (Melty's room)
    env_set("SAVAGEWIND_SORA_SOLO", "0");
    boot(5);
    CHECK(FK.start_n == 1 && NET.melty && NET.on && !strstr(g_log, "solo build"));
    net_set_on(false);
    env_set("SAVAGEWIND_SORA_SOLO", NULL);
    boot(5);
    CHECK(FK.start_n == 1 && NET.melty && NET.on && !strstr(g_log, "solo build"));
    net_set_on(false);
    env_set("SAVAGEWIND_SORA_NET_PEERS", NULL);
}

// 23. (s209) Melty's lab notes, the glue's part: the HELLO's who-am-I before the start (the release id from SORA_ROOT's
// .melty-ready.json), FRAME's counts once a frame in a room only (cleared every frame), never in the solo build / on UDP
static char g_tmp[512];
static void test_notes(void) {
    printf("23. lab notes glue: setup before the start, the release id, FRAME counts in a room only, solo / UDP quiet\n");
    env_set("SORA_ROOT", NULL);
    boot(5);
    CHECK(FK.ad_setup_n == 1 && FK.ad_setup_after_start == 0 && FK.start_n == 1);
    CHECK(!strcmp(FK.ad_addon, "savage-wind/legend-of-kairi") && !strcmp(FK.ad_ver, "dev+net11") && !strcmp(FK.ad_pay, "kairi-sn11") && FK.ad_m == 1.0f);
    CHECK(NET_VERSION == 11);  // (the strings above say 11: a new NET_VERSION changes them by itself; this line only pins the test)
    // FRAME: the first call has no frame time (nothing sent), then each frame hands this frame's counts, which are cleared
    g_nnote.on_screen = 9;
    net_note_frame(SORA_MELTY_FRAME_FOCUSED);
    CHECK(FK.ad_frame_n == 0 && g_nnote.on_screen == 0);
    Sleep(2);
    g_nnote = (NetNoteCount){1, 2, 2};
    net_note_frame(SORA_MELTY_FRAME_FOCUSED | SORA_MELTY_FRAME_PAUSED);
    CHECK(FK.ad_frame_n == 1 && FK.ad_on == 1 && FK.ad_drawn == 2 && FK.ad_anim == 2 && FK.ad_room == 0 && FK.ad_flags == 2052);
    CHECK(FK.ad_dt > 0 && FK.ad_dt < 1 && g_nnote.drawn == 0 && g_nnote.on_screen == 0 && g_nnote.animated == 0);
    Sleep(2);
    net_note_frame(0);
    CHECK(FK.ad_frame_n == 2 && FK.ad_drawn == 0 && FK.ad_on == 0);  // (nobody drawn this frame: 0, not last frame's)
    net_set_on(false);
    // the release id Melty's setup wrote into the game folder (SORA_ROOT): "<16 hex>+net11", with or without a trailing slash
    char path[600];
    snprintf(path, sizeof path, "%s/.melty-ready.json", g_tmp);
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f) fputs("{\"ready\": \"savage-wind-kairi-melty-v2:8FFF2CA146B9805A\", \"at\": \"2026-11-11T10:00:00\"}", f), fclose(f);
    env_set("SORA_ROOT", g_tmp);
    boot(5);
    CHECK(FK.ad_setup_n == 1 && !strcmp(FK.ad_ver, "8fff2ca146b9805a+net11"));
    net_set_on(false);
    char slash[600];
    snprintf(slash, sizeof slash, "%s/", g_tmp);
    env_set("SORA_ROOT", slash);
    boot(5);
    CHECK(FK.ad_setup_n == 1 && !strcmp(FK.ad_ver, "8fff2ca146b9805a+net11"));
    net_set_on(false);
    remove(path);
    boot(5);
    CHECK(FK.ad_setup_n == 1 && !strcmp(FK.ad_ver, "dev+net11"));  // (no marker: a dev / GitHub copy)
    net_set_on(false);
    env_set("SORA_ROOT", NULL);
    // a start that fails: setup was called (before it), FRAME never goes (NET.melty false), counts still cleared
    reset_all(), fk_reset(), FK.start_ret = 0;
    net_init_from_env();
    CHECK(FK.ad_setup_n == 1 && !NET.melty);
    g_nnote = (NetNoteCount){1, 1, 1};
    net_note_frame(0), Sleep(2), net_note_frame(0);
    CHECK(FK.ad_frame_n == 0 && g_nnote.drawn == 0);
    // SAVAGEWIND_SORA_NET=0 (out of the room): not even set up
    reset_all(), fk_reset(), FK.start_ret = 1;
    env_set("SAVAGEWIND_SORA_NET", "0");
    net_init_from_env();
    CHECK(FK.ad_setup_n == 0 && FK.start_n == 0);
    env_set("SAVAGEWIND_SORA_NET", NULL);
    // UDP (Melty did not launch us): nothing
    reset_all(), fk_reset(), FK.start_ret = 1;
    env_set("SAVAGEWIND_SORA_MELTY", NULL), env_set("SAVAGEWIND_SORA_NET", "1");
    net_init_from_env();
    for (int i = 0; i < 3; i++) frame(T0 + 20 * i, true), net_note_frame(0), Sleep(1);
    CHECK(FK.ad_setup_n == 0 && FK.ad_frame_n == 0 && NET.on && !NET.melty);
    net_set_on(false);
    env_set("SAVAGEWIND_SORA_MELTY", "1"), env_set("SAVAGEWIND_SORA_NET", NULL);
    // the solo build: no setup, no FRAME
    env_set("SAVAGEWIND_SORA_SOLO", "1");
    reset_all(), fk_reset(), FK.start_ret = 1, g_net_solo_said = false;
    net_init_from_env();
    for (int i = 0; i < 3; i++) frame(T0 + 20 * i, true), net_note_frame(SORA_MELTY_FRAME_FOCUSED), Sleep(1);
    CHECK(FK.ad_setup_n == 0 && FK.ad_frame_n == 0 && FK.start_n == 0 && !NET.melty);
    env_set("SAVAGEWIND_SORA_SOLO", NULL);
}

// 24. (s209 step 2) the BEAT notes the net glue says: "level" on a change to a known area, "kill" for each enemy this game
// killed (kept on the kill list or not); only in a Melty room (not on UDP, not in the solo build)
static void test_beats(void) {
    printf("24. BEAT glue: level on an area change, kill for our own kills, in a Melty room only\n");
    boot(5);
    g_beat_n = 0;
    net_set_area("MainField");
    net_set_area("MainField");  // (the same: nothing)
    net_set_area("");           // (not known: nothing)
    net_set_area("Dungeon012");
    // ("MainField" -> "" is not said: not known; "" -> "Dungeon012" is a change)
    CHECK(g_beat_n == 2 && !strcmp(g_beat_what[0], "level") && !strcmp(g_beat_detail[0], "MainField") &&
          !strcmp(g_beat_what[1], "level") && !strcmp(g_beat_detail[1], "Dungeon012"));
    g_beat_n = 0;
    net_kills_own(0x1234abcd, "Enemy_Bokoblin_Junior", GetTickCount64());
    net_kills_own(0x1234abce, "Enemy_Stal_Bokoblin_Bone", GetTickCount64());  // (not kept on the list: still a kill)
    net_kills_own(0, "Enemy_Bokoblin", GetTickCount64());                     // (no HashId: nothing)
    CHECK(g_beat_n == 2 && !strcmp(g_beat_what[0], "kill") && !strcmp(g_beat_detail[0], "Enemy_Bokoblin_Junior") &&
          !strcmp(g_beat_detail[1], "Enemy_Stal_Bokoblin_Bone"));
    net_set_on(false);
    // UDP (Melty did not launch us): no BEAT at all
    reset_all(), fk_reset(), FK.start_ret = 1;
    env_set("SAVAGEWIND_SORA_MELTY", NULL), env_set("SAVAGEWIND_SORA_NET", "1");
    net_init_from_env();
    g_beat_n = 0;
    net_set_area("MainField"), net_set_area("Dungeon001");
    net_kills_own(0x1234abcd, "Enemy_Bokoblin_Junior", GetTickCount64());
    CHECK(g_beat_n == 0 && !NET.melty && NET.on);
    net_set_on(false);
    env_set("SAVAGEWIND_SORA_MELTY", "1"), env_set("SAVAGEWIND_SORA_NET", NULL);
    // the solo build: none either
    env_set("SAVAGEWIND_SORA_SOLO", "1");
    reset_all(), fk_reset(), FK.start_ret = 1, g_net_solo_said = false;
    net_init_from_env();
    g_beat_n = 0;
    net_set_area("MainField"), net_set_area("Dungeon001");
    net_kills_own(0x1234abcd, "Enemy_Bokoblin_Junior", GetTickCount64());
    CHECK(g_beat_n == 0 && !NET.melty);
    env_set("SAVAGEWIND_SORA_SOLO", NULL);
    CHECK(g_act_n == 0 && g_played_n == 0 && g_hurt_n == 0);  // (the net glue itself never says these)
}

int main(int argc, char **argv) {
    T0 = GetTickCount64();
    fst_reset(), ck_reset(12);  // (s191: the room clock's hooks are there from the start; no story = it stays off)
    env_set("SAVAGEWIND_SORA_MELTY", "1");  // (s188 G1: every case below is a copy that Melty launched, unless it says otherwise)
    test_start();
    test_not_connected();
    test_self_id();
    test_join_names();
    test_leave_and_quiet();
    test_snap_send();
    test_events_once();
    test_receive();
    test_new_seat();
    test_net_off();
    test_full_table();
    test_hostile();
    test_two_copies();
    test_gate_and_fallback();
    test_clock_glue();
    test_clock_two_copies();
    test_areas_room();
    test_react_room();
    test_kills_room();
    test_kills_owed_moon();
    test_fifth_peer();
    test_solo();
    const char *tmp = argc > 1 ? argv[1] : getenv("TEMP");
    snprintf(g_tmp, sizeof g_tmp, "%s", tmp && *tmp ? tmp : ".");
    test_notes();
    test_beats();
    CHECK(g_hello_bye_ever == 0);
    printf("net_melty_test: %d/%d passed\n", runs - fails, runs);
    return fails ? 1 : 0;
}
