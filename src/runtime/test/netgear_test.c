// host test for runtime/sora_net_gear_core.h + sora_net_gear.h + NET_GEAR in sora_net_proto.h (s289 netgear: a friend's BotW
// weapon drawn in his Sora's hand). Part 1, the pure core: the wire (sizes, letters, exact lengths, round trips), our list,
// the send decision, the forget rule, and the weapon bone against the exe's own placement of Link's weapon (sora_draw.c's grip
// -> world and linkpos.c wpn_model, copied below) over random grips / facings / offsets / scales. Part 2, the glue on stubs
// (no game, no socket, no exe): keys (default 0 = nothing sent, nothing kept), sends on a change / a join / the refresh and
// never per frame, the receive (late ones, a slot's new owner, forgotten when quiet), loopback, the log lines.
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/netgear_test.c -o build_h/netgear_test.exe
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "sora_net_proto.h"
#include "sora_net_peer.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

// ---- stubs for the glue (sora_net.h's NET / net_dests / net_send_all / g_net_loop, sora.c's LOG, the room, the clock) ----
static char g_log[32768];
static int g_nlog;
static void test_log(const char *f, ...) {
    va_list a;
    va_start(a, f);
    size_t n = strlen(g_log);
    vsnprintf(g_log + n, sizeof g_log - n, f, a);
    va_end(a);
    g_nlog++;
}
#define LOG test_log
static void log_clear(void) { g_log[0] = 0, g_nlog = 0; }
#define LOGGED(s) (strstr(g_log, (s)) != NULL)

static uint64_t g_ms = 100000;
static uint64_t GetTickCount64(void) { return g_ms; }
static bool g_net_loop;
static struct { bool on, melty; uint8_t me; NetPeerSlot slot[NET_MAX_PEERS]; } NET;
static bool g_room_up = true;
static bool sora_melty_connected(void) { return g_room_up; }
#define NET_DESTS_MAX 8
static int g_ndest;  // peer addresses net_dests hands out
static int net_dests(NetAddr *d, bool hello) {
    (void)hello;
    for (int i = 0; i < g_ndest; i++) d[i].ip = 0x0100007fu + (uint32_t)i, d[i].port = 5000;
    return g_ndest;
}
static int g_nsent, g_last_n, g_last_nd;
static uint8_t g_last[NET_PKT_MAX];
static void net_send_all(const NetAddr *d, int nd, const uint8_t *b, int n) {
    (void)d;
    g_nsent++, g_last_n = n, g_last_nd = nd;
    memcpy(g_last, b, (size_t)n);
}
#include "sora_net_gear.h"

// ================================================================ part 1: the pure core
static NetGearItem item(uint8_t kind, uint8_t flags, const char *name) {
    NetGearItem it;
    memset(&it, 0, sizeof it);
    it.kind = kind, it.flags = flags;
    snprintf(it.name, sizeof it.name, "%s", name);
    return it;
}

static void test_wire(void) {
    CHECK(NET_GEAR == 12 && NET_DUEL == 11 && NET_VERSION == 12);  // (a new type, no version bump)
    CHECK(NET_GEAR_N == 4 && NET_GEAR_NAME_MAX == 31 && NET_GEAR_PKT == 144 && NET_GEAR_PKT <= NET_PKT_MAX);
    uint8_t b[NET_GEAR_PKT + 8];
    NetGearItem in[NET_GEAR_N], out[NET_GEAR_N];
    int n = 0, type = 0;
    uint8_t pl = 0;
    uint16_t sq = 0;
    char name[NET_NAME_MAX + 1];
    // nothing in his hands: hdr + count 0
    int len = net_pack_gear(9, 0xBEEF, NULL, 0, b, (int)sizeof b);
    CHECK(len == NET_HDR + 1 && b[0] == 'S' && b[1] == 'N' && b[2] == NET_VERSION && b[3] == NET_GEAR && b[4] == 9 && b[7] == 0);
    CHECK(net_unpack_gear(b, len, &pl, &sq, out, &n) && pl == 9 && sq == 0xBEEF && n == 0);
    CHECK(net_unpack(b, len, &type, &pl, &sq, NULL, name) && type == NET_GEAR);
    // one weapon in his hand
    in[0] = item(NGK_MELEE, NGF_HAND, "Weapon_Sword_001");
    len = net_pack_gear(3, 7, in, 1, b, (int)sizeof b);
    CHECK(len == NET_HDR + 1 + 3 + 16 && b[7] == 1 && b[8] == NGK_MELEE && b[9] == NGF_HAND && b[10] == 16 && !memcmp(b + 11, "Weapon_Sword_001", 16));
    memset(out, 0x55, sizeof out);
    CHECK(net_unpack_gear(b, len, &pl, &sq, out, &n) && n == 1 && out[0].kind == NGK_MELEE && out[0].flags == NGF_HAND && !strcmp(out[0].name, "Weapon_Sword_001"));
    CHECK(!net_unpack(b, len - 1, &type, &pl, &sq, NULL, name) && !net_unpack(b, len + 1, &type, &pl, &sq, NULL, name));  // (exact length)
    CHECK(!net_unpack_gear(b, len - 1, &pl, &sq, out, &n));
    {   // a count past the entries / an entry past the end / a name of length 0 or 32 / a letter outside net_area_chars
        uint8_t c[NET_GEAR_PKT + 8];
        memcpy(c, b, (size_t)len), c[7] = 2;
        CHECK(!net_unpack(c, len, &type, &pl, &sq, NULL, name));
        memcpy(c, b, (size_t)len), c[7] = 5;
        CHECK(!net_unpack(c, len, &type, &pl, &sq, NULL, name));
        memcpy(c, b, (size_t)len), c[10] = 0;
        CHECK(!net_unpack(c, len, &type, &pl, &sq, NULL, name));
        memcpy(c, b, (size_t)len), c[10] = 17;
        CHECK(!net_unpack(c, len, &type, &pl, &sq, NULL, name));
        const char bad[] = {'/', '.', ' ', 0, '\\', ':', (char)0xC3};
        for (size_t i = 0; i < sizeof bad; i++) {
            memcpy(c, b, (size_t)len), c[11 + 6] = (uint8_t)bad[i];
            CHECK(!net_unpack(c, len, &type, &pl, &sq, NULL, name));
        }
        memcpy(c, b, (size_t)len), c[3] = NET_AREA;  // (the same bytes as another type: its own rules)
        CHECK(!net_unpack_gear(c, len, &pl, &sq, out, &n));
    }
    // four entries, each kind (unknown kinds travel too: a newer build's shield / bow), the longest names
    in[0] = item(NGK_MELEE, 0, "Weapon_Lsword_001");
    in[1] = item(NGK_SHIELD, NGF_HAND, "Weapon_Shield_001");
    in[2] = item(NGK_BOW, NGF_HAND, "abcdefghijklmnopqrstuvwxyz01234");  // 31
    in[3] = item(200, 0xFF, "x");
    len = net_pack_gear(1, 2, in, 4, b, (int)sizeof b);
    CHECK(len == NET_HDR + 1 + 4 * 3 + 17 + 17 + 31 + 1 && len <= NET_GEAR_PKT);
    CHECK(net_unpack_gear(b, len, &pl, &sq, out, &n) && n == 4 && ngear_same(in, 4, out, 4));
    // the packer refuses what the wire cannot carry
    NetGearItem t = item(NGK_MELEE, 1, "Weapon_Sword_001");
    memset(t.name, 'a', sizeof t.name);  // (32 letters, no 0)
    CHECK(net_pack_gear(1, 1, &t, 1, b, (int)sizeof b) < 0);
    t = item(NGK_MELEE, 1, "");
    CHECK(net_pack_gear(1, 1, &t, 1, b, (int)sizeof b) < 0);
    t = item(NGK_MELEE, 1, "../x");
    CHECK(net_pack_gear(1, 1, &t, 1, b, (int)sizeof b) < 0);
    CHECK(net_pack_gear(1, 1, in, 5, b, (int)sizeof b) < 0 && net_pack_gear(1, 1, in, 1, b, NET_HDR + 3) < 0);
    // worst case fits
    for (int i = 0; i < 4; i++) in[i] = item(NGK_MELEE, 1, "abcdefghijklmnopqrstuvwxyz01234");
    CHECK(net_pack_gear(1, 1, in, 4, b, NET_GEAR_PKT) == NET_GEAR_PKT);
}

static void test_local_same_hand(void) {
    NetGearItem a[NET_GEAR_N], b[NET_GEAR_N];
    CHECK(ngear_local(a, NULL, true) == 0 && ngear_local(a, "", true) == 0);
    CHECK(ngear_local(a, "Weapon_Sword_001", true) == 1 && a[0].kind == NGK_MELEE && a[0].flags == NGF_HAND && !strcmp(a[0].name, "Weapon_Sword_001"));
    CHECK(ngear_local(b, "Weapon_Sword_001", false) == 1 && b[0].flags == 0);
    CHECK(ngear_local(b, "abcdefghijklmnopqrstuvwxyz012345", true) == 0);  // (32: too long)
    CHECK(ngear_local(b, "Weapon Sword", true) == 0);
    CHECK(ngear_same(a, 1, a, 1) && !ngear_same(a, 1, b, 0) && ngear_same(a, 0, b, 0));
    ngear_local(b, "Weapon_Sword_001", false);
    CHECK(!ngear_same(a, 1, b, 1));  // (in hand vs put away)
    ngear_local(b, "Weapon_Sword_002", true);
    CHECK(!ngear_same(a, 1, b, 1));
    CHECK(!strcmp(ngear_hand(a, 1), "Weapon_Sword_001") && ngear_hand(a, 0) == NULL && ngear_hand(NULL, 3) == NULL);
    NetGearItem l[3] = {item(NGK_SHIELD, NGF_HAND, "Weapon_Shield_001"), item(NGK_MELEE, 0, "Weapon_Sword_002"), item(NGK_MELEE, NGF_HAND, "Weapon_Spear_001")};
    CHECK(!strcmp(ngear_hand(l, 3), "Weapon_Spear_001") && ngear_hand(l, 2) == NULL);
}

static void test_due_fresh(void) {
    // the first one at once; a change after the 250 ms gap; a join likewise; else only the refresh
    CHECK(ngear_due(false, false, false, 1000, 0, 0));
    CHECK(!ngear_due(false, false, true, 1000, 3000, 900));
    CHECK(!ngear_due(true, false, true, 1000, 3000, 900) && ngear_due(true, false, true, 1150, 3000, 900));
    CHECK(!ngear_due(false, true, true, 1100, 3000, 900) && ngear_due(false, true, true, 1150, 3000, 900));
    CHECK(ngear_due(false, false, true, 3000, 3000, 900));
    CHECK(ngear_due(true, false, true, 500, 3000, 900));  // (the clock went back: no wait)
    // a weapon flicking every frame: at most one send per 250 ms (+ the refresh)
    {
        uint64_t last = 0, next = 0;
        bool any = false;
        int sends = 0;
        for (uint64_t t = 10000; t < 20000; t += 16) {
            bool changed = true;
            if (ngear_due(changed, false, any, t, next, last)) sends++, any = true, last = t, next = t + NET_GEAR_MS;
        }
        CHECK(sends >= 38 && sends <= 41);
    }
    CHECK(!ngear_fresh(0, 5000, 2000));
    CHECK(ngear_fresh(1000, 1000 + 5 * 2000, 2000) && !ngear_fresh(1000, 1001 + 5 * 2000, 2000));
    CHECK(ngear_fresh(9000, 5000, 2000));  // (a reading from "the future": kept)
}

// ---- the exe's placement of Link's weapon in the LOCAL Sora's hand, copied (lead 0) ----
// sora_draw.c skin_publish (s128 wpn): the grip bone b (model rows, cm) of a body at feet t facing face, x scale -> world g
static void exe_grip_world(const float b[12], const float t[3], float face, float scale, float w[12]) {
    float s = sinf(face), c = cosf(face);
    for (int r = 0; r < 3; r++) {
        float x = b[r * 3], y = b[r * 3 + 1], z = b[r * 3 + 2];
        float wx = c * x + s * z, wy = y, wz = -s * x + c * z, l = sqrtf(wx * wx + wy * wy + wz * wz);
        if (l < 1e-6f) l = 1, wx = (r == 0), wy = (r == 1), wz = (r == 2);
        w[0 * 4 + r] = wx / l, w[1 * 4 + r] = wy / l, w[2 * 4 + r] = wz / l;
    }
    w[3] = t[0] + (c * b[9] + s * b[11]) * scale, w[7] = t[1] + b[10] * scale, w[11] = t[2] + (-s * b[9] + c * b[11]) * scale;
}
// linkpos.c wpn_model: grip g + off[8] -> the BotW model matrix m (3x4 row-major, columns = the weapon's axes)
static void exe_wpn_model(const float g[12], const float off[8], float m[12]) {
    float X[3] = {g[0], g[4], g[8]}, Y[3] = {g[2], g[6], g[10]}, Z[3] = {-g[1], -g[5], -g[9]};
    for (int k = 1; k <= 3; k++) {
        if (off[k] == 0) continue;
        float r = off[k] * 0.0174533f, cs = cosf(r), sn = sinf(r);
        float *p = k == 1 ? X : k == 2 ? Y : Z, *q = k == 1 ? Y : k == 2 ? Z : X;
        float np[3], nq[3];
        for (int i = 0; i < 3; i++) np[i] = cs * p[i] + sn * q[i], nq[i] = -sn * p[i] + cs * q[i];
        memcpy(p, np, sizeof np), memcpy(q, nq, sizeof nq);
    }
    float s = off[4] > 0.05f && off[4] < 4.0f ? off[4] : 1.0f;
    for (int i = 0; i < 3; i++) m[i * 4 + 0] = X[i] * s, m[i * 4 + 1] = Y[i] * s, m[i * 4 + 2] = Z[i] * s;
    for (int i = 0; i < 3; i++) m[i * 4 + 3] = g[3 + 4 * i] - Z[i] * off[0] - s * (X[i] * off[5] + Y[i] * off[6] + Z[i] * off[7]);
}
// sora_draw.c skin_verts for our 1-bone gear actor: v (cm) x rows -> model, then feet + RotY(face) x scale
static void gear_vert(const float rows[12], const float t[3], float face, float scale, const float v[3], float o[3]) {
    float q[3];
    for (int j = 0; j < 3; j++) q[j] = v[0] * rows[j] + v[1] * rows[3 + j] + v[2] * rows[6 + j] + rows[9 + j];
    float s = sinf(face), c = cosf(face);
    o[0] = t[0] + (c * q[0] + s * q[2]) * scale, o[1] = t[1] + q[1] * scale, o[2] = t[2] + (-s * q[0] + c * q[2]) * scale;
}
static float frand(float lo, float hi) { return lo + (hi - lo) * (float)rand() / (float)RAND_MAX; }
static void rot3(float a, float b, float c, float r[9]) {  // Rz(c) Ry(b) Rx(a)
    float ca = cosf(a), sa = sinf(a), cb = cosf(b), sb = sinf(b), cc = cosf(c), sc = sinf(c);
    float x[9] = {1, 0, 0, 0, ca, -sa, 0, sa, ca}, y[9] = {cb, 0, sb, 0, 1, 0, -sb, 0, cb}, z[9] = {cc, -sc, 0, sc, cc, 0, 0, 0, 1}, t[9];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) t[i * 3 + j] = y[i * 3] * x[j] + y[i * 3 + 1] * x[3 + j] + y[i * 3 + 2] * x[6 + j];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) r[i * 3 + j] = z[i * 3] * t[j] + z[i * 3 + 1] * t[3 + j] + z[i * 3 + 2] * t[6 + j];
}

static void test_rows(void) {
    // the identity grip: X = A0, Y = A2, Z = -A1; joint kept; cm / k
    float grip[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 10, 100, 5}, off[8] = {0, 0, 0, 0, 1, 0, 0, 0}, r[12];
    CHECK(ngear_rows(grip, off, 1.0f, r));
    const float want[12] = {1, 0, 0, 0, 0, 1, 0, -1, 0, 10, 100, 5};
    bool same = true;
    for (int i = 0; i < 12; i++) same = same && fabsf(r[i] - want[i]) < 1e-6f;
    CHECK(same);
    // back 0.1 m along the blade at k 2: the origin moves 0.1 x 100 / 2 = 5 cm along +Z (= -A1 = -y)
    off[0] = -0.1f;
    CHECK(ngear_rows(grip, off, 2.0f, r) && fabsf(r[9] - 10) < 1e-5f && fabsf(r[10] - 95) < 1e-5f && fabsf(r[11] - 5) < 1e-5f && fabsf(r[0] - 0.5f) < 1e-6f);
    // nothing to hold it: a squashed bone (a hidden Keyblade), a bad k, a NaN
    float flat[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3};
    CHECK(!ngear_rows(flat, off, 1.0f, r) && !ngear_rows(grip, off, 0.0f, r) && !ngear_rows(grip, off, -1.0f, r));
    float nan_grip[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, NAN, 0, 0};
    CHECK(!ngear_rows(nan_grip, off, 1.0f, r) && !ngear_rows(NULL, off, 1.0f, r));
    // against the exe, 2000 random cases: a scaled / turned grip, any facing, the type offsets, the 3 arms' offsets
    srand(289);
    double worst = 0;
    int bad = 0;
    for (int it = 0; it < 2000; it++) {
        float R[9], sc = frand(0.6f, 1.6f), b[12];
        rot3(frand(-3.1f, 3.1f), frand(-1.5f, 1.5f), frand(-3.1f, 3.1f), R);
        for (int i = 0; i < 9; i++) b[i] = R[i] * sc;  // (KH bones may carry scale: the exe makes the axes unit)
        b[9] = frand(-60, 60), b[10] = frand(0, 180), b[11] = frand(-60, 60);
        float t[3] = {frand(-4000, 4000), frand(-50, 400), frand(-4000, 4000)}, face = frand(-6.3f, 6.3f), k = frand(0.8f, 1.3f), scale = 0.01f * k;
        float o[8] = {-(float)(10 + 5 * (it % 3)) / 100.0f, 180, (float)(it % 7) * 15 - 45, (float)(it % 5) * 30 - 60, 1, 0, 0, 0};
        if (it % 4 == 3) o[3] -= 90, o[4] = 0.7f, o[5] = 0.25f, o[6] = -0.02f, o[7] = 0.03f, o[0] = 0;  // (an arm)
        float g[12], m[12], rows[12];
        exe_grip_world(b, t, face, scale, g);
        exe_wpn_model(g, o, m);
        if (!ngear_rows(b, o, k, rows)) { bad++; continue; }
        for (int vtx = 0; vtx < 4; vtx++) {
            float vm[3] = {frand(-0.3f, 0.3f), frand(-0.1f, 0.1f), frand(-0.5f, 2.2f)}, vcm[3] = {vm[0] * 100, vm[1] * 100, vm[2] * 100}, e[3], q[3];
            for (int i = 0; i < 3; i++) e[i] = m[i * 4] * vm[0] + m[i * 4 + 1] * vm[1] + m[i * 4 + 2] * vm[2] + m[i * 4 + 3];
            gear_vert(rows, t, face, scale, vcm, q);
            for (int i = 0; i < 3; i++) {
                double d = fabs((double)e[i] - (double)q[i]);
                if (d > worst) worst = d;
            }
        }
    }
    printf("  rows vs the exe's placement: 2000 grips x 4 vertices, worst %.6f m (refused %d)\n", worst, bad);
    CHECK(bad == 0 && worst < 2e-3);  // (float, world coordinates up to 4 km)
}

// ================================================================ part 2: the glue on stubs
static void slot_use(int k, const char *name, uint64_t joined) {
    memset(&NET.slot[k], 0, sizeof NET.slot[k]);
    NET.slot[k].used = true, NET.slot[k].joined_ms = joined;
    snprintf(NET.slot[k].p.name, sizeof NET.slot[k].p.name, "%s", name);
}
static int peer_packet(uint8_t player, uint16_t seq, const char *melee, bool hand, uint8_t *b) {
    NetGearItem it[NET_GEAR_N];
    int n = ngear_local(it, melee, hand);
    return net_pack_gear(player, seq, it, n, b, NET_GEAR_PKT);
}

static void test_glue(void) {
    memset(&NET, 0, sizeof NET), memset(&NETG, 0, sizeof NETG);
    NET.on = true, NET.me = 5;
    g_ndest = 0, g_nsent = 0;
    uint8_t b[NET_GEAR_PKT];
    // key 0 ("set netgear 0"; the default before s290): stored only, nothing sent, a friend's packet dropped and counted, nothing to draw
    log_clear();
    CHECK(net_gear_live_set("netgear", 0) && LOGGED("[net] gear off (set netgear 0"));
    CHECK(g_net_gear == 0 && g_net_gear_log == 0 && g_net_gear_max == 8);
    slot_use(0, "Kairi", 5000);
    g_ndest = 1;
    net_gear_local("Weapon_Sword_001", true);
    for (int i = 0; i < 200; i++) g_ms += 16, net_gear_tick(g_ms);
    CHECK(g_nsent == 0);
    int n = peer_packet(7, 1, "Weapon_Lsword_001", true, b);
    net_gear_recv(0, b, n, g_ms);
    CHECK(NETG.st.rx_off == 1 && NETG.st.rx == 0 && net_gear_hand(0, g_ms) == NULL);
    // key 1: one line; the same line again (a live rewrite) = no line
    log_clear();
    CHECK(net_gear_live_set("netgear", 1) && g_net_gear == 1 && LOGGED("[net] gear on"));
    int nl = g_nlog;
    CHECK(net_gear_live_set("netgear", 1) && g_nlog == nl);
    CHECK(net_gear_live_set("netgearlog", 1) && g_net_gear_log == 1 && LOGGED("gear log on"));
    CHECK(!net_gear_live_set("netgearx", 1) && !net_gear_live_set("net", 1));
    CHECK(net_gear_live_set("netgearmax", 99) && g_net_gear_max == 32 && net_gear_live_set("netgearmax", 8) && g_net_gear_max == 8);
    // the first tick sends at once (one peer), then nothing for frames while nothing changes, then the 2 s refresh
    log_clear();
    g_ms += 16, net_gear_tick(g_ms);
    CHECK(g_nsent == 1 && g_last_nd == 1 && g_last[3] == NET_GEAR && g_last[7] == 1 && LOGGED("[net] gear: sent melee Weapon_Sword_001 (in hand) (UDP)"));
    {
        NetGearItem it[NET_GEAR_N];
        int ni = 0;
        CHECK(net_unpack_gear(g_last, g_last_n, NULL, NULL, it, &ni) && ni == 1 && !strcmp(it[0].name, "Weapon_Sword_001") && it[0].flags == NGF_HAND);
    }
    uint64_t t0 = g_ms;
    log_clear();
    while (g_ms < t0 + NET_GEAR_MS - 16) g_ms += 16, net_gear_local("Weapon_Sword_001", true), net_gear_tick(g_ms);
    CHECK(g_nsent == 1);
    g_ms = t0 + NET_GEAR_MS, net_gear_tick(g_ms);
    CHECK(g_nsent == 2 && g_nlog == 0);  // (a refresh: no log line)
    // put away -> a change, sent once the 250 ms gap is over; one line
    g_ms += 100, net_gear_local("Weapon_Sword_001", false), net_gear_tick(g_ms);
    CHECK(g_nsent == 2);
    g_ms += 160, net_gear_tick(g_ms);
    CHECK(g_nsent == 3 && LOGGED("sent melee Weapon_Sword_001 (put away)") && g_nlog == 1);
    // nothing equipped (the Keyblade): n = 0 goes out too, so his puppet stops drawing it
    g_ms += 300, net_gear_local(NULL, false), net_gear_tick(g_ms);
    CHECK(g_nsent == 4 && g_last_n == NET_HDR + 1 && g_last[7] == 0 && LOGGED("sent nothing"));
    // a 2nd peer joins: sent at once (after the gap)
    slot_use(1, "Riku", g_ms);
    g_ndest = 2;
    g_ms += 300, net_gear_tick(g_ms);
    CHECK(g_nsent == 5 && g_last_nd == 2);
    // alone: nothing sent (no dests)
    g_ndest = 0;
    g_ms += 5000, net_gear_tick(g_ms);
    CHECK(g_nsent == 5);
    g_ndest = 2;

    // the receive: his weapon in hand; a late (older) packet dropped; a change logged once
    log_clear();
    n = peer_packet(7, 100, "Weapon_Lsword_001", true, b);
    net_gear_recv(0, b, n, g_ms);
    CHECK(!strcmp(net_gear_hand(0, g_ms), "Weapon_Lsword_001") && LOGGED("received from peer 1 \"Kairi\": melee Weapon_Lsword_001 (in hand)"));
    nl = g_nlog;
    net_gear_recv(0, b, n, g_ms);  // (the same seq again)
    CHECK(NETG.st.rx_dup == 1 && g_nlog == nl);
    n = peer_packet(7, 99, "Weapon_Sword_002", true, b);
    net_gear_recv(0, b, n, g_ms);
    CHECK(NETG.st.rx_dup == 2 && !strcmp(net_gear_hand(0, g_ms), "Weapon_Lsword_001"));
    n = peer_packet(7, 101, "Weapon_Lsword_001", true, b);  // (a refresh: newer, the same list: no line)
    net_gear_recv(0, b, n, g_ms);
    CHECK(g_nlog == nl && NETG.st.rx == 2);
    n = peer_packet(7, 102, "Weapon_Lsword_001", false, b);  // (put away: not in his hand any more)
    net_gear_recv(0, b, n, g_ms);
    CHECK(net_gear_hand(0, g_ms) == NULL && g_nlog == nl + 1);
    n = peer_packet(7, 103, "Weapon_Lsword_001", true, b);
    net_gear_recv(0, b, n, g_ms);
    // a bad packet: counted, the record kept
    uint8_t c[NET_GEAR_PKT];
    memcpy(c, b, (size_t)n), c[11] = '/';
    net_gear_recv(0, c, n, g_ms);
    CHECK(NETG.st.rx_bad == 1 && !strcmp(net_gear_hand(0, g_ms), "Weapon_Lsword_001"));
    // quiet for 5 periods: forgotten (drawn as before); heard again: back
    CHECK(net_gear_hand(0, g_ms + 5 * NET_GEAR_MS) != NULL && net_gear_hand(0, g_ms + 5 * NET_GEAR_MS + 1) == NULL);
    // the slot is someone else now (left + another joined): his record is not the new player's
    slot_use(0, "Sora2", g_ms + 10);
    CHECK(net_gear_hand(0, g_ms) == NULL);
    n = peer_packet(9, 5, "Weapon_Spear_001", true, b);  // (a low seq: the new player's own counter)
    net_gear_recv(0, b, n, g_ms);
    CHECK(!strcmp(net_gear_hand(0, g_ms), "Weapon_Spear_001"));
    // a free slot: nothing; key 0 again: nothing for anyone, one line, the records dropped
    NET.slot[2].used = false;
    CHECK(net_gear_hand(2, g_ms) == NULL && net_gear_hand(-1, g_ms) == NULL && net_gear_hand(NET_MAX_PEERS, g_ms) == NULL);
    log_clear();
    CHECK(net_gear_live_set("netgear", 0) && LOGGED("[net] gear off") && net_gear_hand(0, g_ms) == NULL);
    int sent = g_nsent;
    g_ms += 10000, net_gear_local("Weapon_Sword_001", true), net_gear_tick(g_ms);
    CHECK(g_nsent == sent);
    CHECK(net_gear_live_set("netgear", 1));
    CHECK(net_gear_hand(0, g_ms) == NULL);  // (taken anew after a 0 -> 1)
    g_ms += 16, net_gear_tick(g_ms);
    CHECK(g_nsent == sent + 1);  // (on again: sent at once)

    // a Melty room: nothing while the room is not up; then the 10 s refresh
    NET.melty = true, g_room_up = false;
    g_ms += 20000, net_gear_tick(g_ms);
    CHECK(g_nsent == sent + 1);
    g_room_up = true, g_ndest = 0;  // (in a room net_dests gives nothing; the slots say who is there)
    net_gear_tick(g_ms);
    CHECK(g_nsent == sent + 2);
    g_ms += NET_GEAR_MS, net_gear_tick(g_ms);
    CHECK(g_nsent == sent + 2);
    g_ms += NET_GEAR_MELTY_MS - NET_GEAR_MS, net_gear_tick(g_ms);
    CHECK(g_nsent == sent + 3);
    CHECK(net_gear_hand(1, g_ms) == NULL);
    n = peer_packet(8, 1, "Weapon_Sword_070", true, b);
    net_gear_recv(1, b, n, g_ms);
    CHECK(net_gear_hand(1, g_ms + 5 * NET_GEAR_MS + 1) != NULL);  // (a room's period is 10 s: kept 50 s)
    NET.melty = false, g_ndest = 2;

    // loopback: our list -> pack -> unpack -> slot 0, nothing sent
    memset(&NET, 0, sizeof NET);
    NET.on = true, NET.me = 5, g_net_loop = true;
    net_gear_reset(g_ms);
    sent = g_nsent;
    slot_use(0, "", g_ms);
    net_gear_local("Weapon_Sword_013", true);
    g_ms += 16, net_gear_tick(g_ms);
    CHECK(g_nsent == sent && !strcmp(net_gear_hand(0, g_ms), "Weapon_Sword_013") && LOGGED("(loopback)"));
    g_net_loop = false;

    // the diag line
    log_clear();
    net_gear_diag();
    CHECK(LOGGED("[net] gear on: ours melee Weapon_Sword_013 (in hand)") && LOGGED("slot 1"));
    net_gear_live_set("netgear", 0), net_gear_live_set("netgearlog", 0);
}

// (s290 flip) the compiled default is ON (the user: "Flip the passes ON"); the log key stays off
static void test_default_on(void) {
    CHECK(g_net_gear == 1 && g_net_gear_log == 0 && g_net_gear_max == 8);
}

int main(void) {
    test_default_on();
    test_wire();
    test_local_same_hand();
    test_due_fresh();
    test_rows();
    test_glue();
    printf("netgear_test: %d checks, %d failed -> %s\n", runs, fails, fails ? "FAIL" : "PASS");
    return fails != 0;
}
