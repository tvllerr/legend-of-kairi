// host test for runtime/sora_net_shots_core.h + sora_net_shots.h + NET_SHOTS / NET_RUNES in sora_net_proto.h (s289 shots: a
// friend's arrows, Remote Bombs and thrown weapon fly in your world; his rune state for the objs helper). Part 1, the pure core:
// the wire (sizes, kinds, names, floats, exact lengths, round trips), the rotation and the draw rows against the exe's skinning
// (sora_draw.c skin_verts, copied below), the PR's walk and AS lookup on fake guest memory, which shots are his (the PR's checks +
// moved / away / still / never moved / bombs / booms / the thrown weapon), the arrow on his bow (s289: the nocked arrow is the
// one that flies: followed on the string, sent once it flies, fast or slow; parents), the walk's cadence, a friend's tracks
// (lerp, extrapolation, end, quiet, booms once), the send decision. Part 2, the glue on stubs (no game, no socket, no exe): keys (default 0 = nothing sent, nothing
// kept), the send rates (<= 1 NET_SHOTS + 1 NET_RUNES per 50 ms), empties, alone / room down = nothing, loopback, late / bad
// packets, netshots_friend_holds, the log lines.
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/netshots_test.c -o build_h/netshots_test.exe
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
static char g_log[65536];
static int g_nlog;
static void test_log(const char *f, ...) {
    va_list a;
    va_start(a, f);
    size_t n = strlen(g_log);
    if (n < sizeof g_log - 1) vsnprintf(g_log + n, sizeof g_log - n, f, a);
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
static int g_ndest;
static int net_dests(NetAddr *d, bool hello) {
    (void)hello;
    for (int i = 0; i < g_ndest; i++) d[i].ip = 0x0100007fu + (uint32_t)i, d[i].port = 5000;
    return g_ndest;
}
static int g_nsent, g_nsent_type[32], g_last_n;
static uint8_t g_last[NET_PKT_MAX], g_last_type[32][NET_PKT_MAX];
static int g_last_type_n[32];
static void net_send_all(const NetAddr *d, int nd, const uint8_t *b, int n) {
    (void)d, (void)nd;
    g_nsent++, g_last_n = n, g_nsent_type[b[3] & 31]++;
    memcpy(g_last, b, (size_t)n), memcpy(g_last_type[b[3] & 31], b, (size_t)n), g_last_type_n[b[3] & 31] = n;
}
#include "sora_net_shots.h"

static float frand(void) { return (float)rand() / (float)RAND_MAX * 2.0f - 1.0f; }
static bool near(float a, float b, float e) { return fabsf(a - b) <= e; }

// ================================================================ part 1: the pure core
static NetShot shot(uint16_t id, uint8_t kind, float x, float y, float z, const char *name) {
    NetShot s;
    memset(&s, 0, sizeof s);
    s.id = id, s.kind = kind, s.pos[0] = x, s.pos[1] = y, s.pos[2] = z, s.q[3] = 1;
    if (name) snprintf(s.name, sizeof s.name, "%s", name);
    return s;
}

static void test_wire(void) {
    CHECK(NET_SHOTS == 15 && NET_RUNES == 16 && NET_VERSION == 12 && NET_GEAR == 12);  // (new types, no version bump)
    CHECK(NET_SHOTS_PKT == 521 && NET_SHOTS_PKT <= NET_PKT_MAX && NET_RUNES_PKT == 117 && NET_SHOTS_PKT <= 1024);  // (Melty PACKET_MAX 1024)
    uint8_t b[NET_SHOTS_PKT + 16];
    NetShot in[NET_SHOTS_N], out[NET_SHOTS_N];
    NetBoom bin[NET_SHOTS_BOOMS], bout[NET_SHOTS_BOOMS];
    int n = 0, nb = 0, type = 0;
    uint8_t pl = 0;
    uint16_t sq = 0;
    char name[NET_NAME_MAX + 1];
    // nothing: hdr + 2 counts
    int len = net_pack_shots(9, 0xBEEF, NULL, 0, NULL, 0, b, (int)sizeof b);
    CHECK(len == NET_HDR + 2 && b[0] == 'S' && b[1] == 'N' && b[2] == NET_VERSION && b[3] == NET_SHOTS && b[4] == 9 && b[7] == 0 && b[8] == 0);
    CHECK(net_unpack_shots(b, len, &pl, &sq, out, &n, bout, &nb) && pl == 9 && sq == 0xBEEF && n == 0 && nb == 0);
    CHECK(net_unpack(b, len, &type, &pl, &sq, NULL, name) && type == NET_SHOTS);
    // a full one: 8 shots (2 thrown with the longest names), 4 booms = the largest packet
    for (int i = 0; i < NET_SHOTS_N; i++) {
        in[i] = shot((uint16_t)(100 + i), (uint8_t)(i < 2 ? NSK_THROWN : i), 10.5f * i, -3.25f, 700.0f + i, i < 2 ? "Weapon_Lsword_0123456789abcdefg" : NULL);
        float q[4] = {frand(), frand(), frand(), frand()};
        memcpy(in[i].q, q, sizeof q);
        in[i].flags = (uint8_t)(i & 1);
    }
    CHECK(strlen(in[0].name) == NET_GEAR_NAME_MAX);
    for (int i = 0; i < NET_SHOTS_BOOMS; i++) memset(&bin[i], 0, sizeof bin[i]), bin[i].seq = (uint16_t)(7 + i), bin[i].kind = (uint8_t)(NSK_REMOTE_BOMB + i), bin[i].pos[1] = 2.5f * i;
    len = net_pack_shots(3, 77, in, NET_SHOTS_N, bin, NET_SHOTS_BOOMS, b, (int)sizeof b);
    CHECK(len == NET_HDR + 2 + 8 * 24 + 4 * 16 + 2 * 32 && len <= NET_SHOTS_PKT);
    CHECK(net_unpack_shots(b, len, &pl, &sq, out, &n, bout, &nb) && pl == 3 && sq == 77 && n == 8 && nb == 4);
    bool same = true;
    for (int i = 0; i < n; i++) {
        float l = sqrtf(in[i].q[0] * in[i].q[0] + in[i].q[1] * in[i].q[1] + in[i].q[2] * in[i].q[2] + in[i].q[3] * in[i].q[3]);
        float d = 0;  // (q and -q are the same turn)
        for (int c = 0; c < 4; c++) d += out[i].q[c] * in[i].q[c] / l;
        same = same && out[i].id == in[i].id && out[i].kind == in[i].kind && out[i].flags == in[i].flags && fabsf(fabsf(d) - 1) < 1e-4f &&
               out[i].pos[0] == in[i].pos[0] && out[i].pos[1] == in[i].pos[1] && out[i].pos[2] == in[i].pos[2] && !strcmp(out[i].name, in[i].name);
    }
    for (int i = 0; i < nb; i++) same = same && bout[i].seq == bin[i].seq && bout[i].kind == bin[i].kind && bout[i].pos[1] == bin[i].pos[1];
    CHECK(same);
    // too small a buffer / too many / bad entries: refused when packing
    CHECK(net_pack_shots(3, 1, in, NET_SHOTS_N, bin, NET_SHOTS_BOOMS, b, len - 1) < 0);
    CHECK(net_pack_shots(3, 1, in, NET_SHOTS_N + 1, NULL, 0, b, (int)sizeof b) < 0 && net_pack_shots(3, 1, NULL, 0, bin, NET_SHOTS_BOOMS + 1, b, (int)sizeof b) < 0);
    NetShot bad[2] = {shot(1, NSK_NORMAL_ARROW, 0, 0, 0, NULL), shot(1, NSK_FIRE_ARROW, 0, 0, 0, NULL)};
    CHECK(net_pack_shots(3, 1, bad, 2, NULL, 0, b, (int)sizeof b) < 0);  // (an id twice)
    bad[1].id = 0;
    CHECK(net_pack_shots(3, 1, bad, 2, NULL, 0, b, (int)sizeof b) < 0);  // (id 0)
    bad[1] = shot(2, NSK_N, 0, 0, 0, NULL);
    CHECK(net_pack_shots(3, 1, bad, 2, NULL, 0, b, (int)sizeof b) < 0);  // (a kind past the table)
    bad[1] = shot(2, NSK_THROWN, 0, 0, 0, "Weapon/../x");
    CHECK(net_pack_shots(3, 1, bad, 2, NULL, 0, b, (int)sizeof b) < 0);  // (a name with letters the wire never takes)
    bad[1] = shot(2, NSK_THROWN, 0, 0, 0, "");
    CHECK(net_pack_shots(3, 1, bad, 2, NULL, 0, b, (int)sizeof b) < 0);  // (a thrown weapon without its name)
    bad[1] = shot(2, NSK_BOMB_ARROW, NAN, 0, 0, NULL);
    CHECK(net_pack_shots(3, 1, bad, 2, NULL, 0, b, (int)sizeof b) < 0);  // (not finite)
    bad[1] = shot(2, NSK_BOMB_ARROW, 0, 0, 0, NULL), bad[1].q[3] = 0;
    CHECK(net_pack_shots(3, 1, bad, 2, NULL, 0, b, (int)sizeof b) < 0);  // (no rotation)
    NetBoom bb = {.seq = 0, .kind = NSK_REMOTE_BOMB};
    CHECK(net_pack_shots(3, 1, NULL, 0, &bb, 1, b, (int)sizeof b) < 0);  // (seq 0)
    bb.seq = 5, bb.kind = NSK_FIRE_ARROW;
    CHECK(net_pack_shots(3, 1, NULL, 0, &bb, 1, b, (int)sizeof b) < 0);  // (only bombs go off)
    // a received packet that lies: every byte flip that breaks a rule, cut short, one byte more
    len = net_pack_shots(3, 77, in, 3, bin, 2, b, (int)sizeof b);
    CHECK(len > 0 && net_unpack_shots(b, len, &pl, &sq, out, &n, bout, &nb) && n == 3 && nb == 2 && !strcmp(out[0].name, in[0].name));
    CHECK(!net_unpack_shots(b, len - 1, &pl, &sq, out, &n, bout, &nb) && !net_unpack_shots(b, len + 1, &pl, &sq, out, &n, bout, &nb));
    uint8_t c[sizeof b];
    struct { int at; uint8_t v; } flips[] = {
        {NET_HDR, 9},                       // more shots than the max
        {NET_HDR + 1, 5},                   // more booms than the max
        {NET_HDR + 2, 0},                   // id 0 (with its high byte, below)
        {NET_HDR + 2 + 24 + 2, NSK_N},      // a kind past the table
        {NET_HDR + 2 + 24 + 4 + 3, 0x7f},   // x = NaN (0x7fxxxxxx with a mantissa)
    };
    for (size_t f = 0; f < sizeof flips / sizeof flips[0]; f++) {
        memcpy(c, b, (size_t)len);
        c[flips[f].at] = flips[f].v;
        if (flips[f].at == NET_HDR + 2) c[NET_HDR + 3] = 0;
        if (flips[f].at == NET_HDR + 2 + 24 + 4 + 3) c[NET_HDR + 2 + 24 + 4 + 2] = 0xff;
        CHECK(!net_unpack_shots(c, len, &pl, &sq, out, &n, bout, &nb));
    }
    memcpy(c, b, (size_t)len), c[NET_HDR + 2 + 24 + 0] = c[NET_HDR + 2], c[NET_HDR + 2 + 24 + 1] = c[NET_HDR + 3];
    CHECK(!net_unpack_shots(c, len, &pl, &sq, out, &n, bout, &nb));  // (two entries with one id)
    memcpy(c, b, (size_t)len), memset(c + NET_HDR + 2 + 16, 0, 8);
    CHECK(!net_unpack_shots(c, len, &pl, &sq, out, &n, bout, &nb));  // (rotation all 0)
    memcpy(c, b, (size_t)len), c[NET_HDR + 2 + 3 * 24 + 2] = NSK_NORMAL_ARROW;
    CHECK(!net_unpack_shots(c, len, &pl, &sq, out, &n, bout, &nb));  // (a boom of an arrow)
    memcpy(c, b, (size_t)len), c[NET_HDR + 2 + 3 * 24 + 3] = 1;
    CHECK(!net_unpack_shots(c, len, &pl, &sq, out, &n, bout, &nb));  // (the boom's pad byte)
    int nat = NET_HDR + 2 + 3 * 24 + 2 * 16;  // (shot 0 is thrown: its name follows)
    memcpy(c, b, (size_t)len), c[nat + 1] = '/';
    CHECK(!net_unpack_shots(c, len, &pl, &sq, out, &n, bout, &nb));
    memcpy(c, b, (size_t)len), c[nat] = 0;
    CHECK(!net_unpack_shots(c, len, &pl, &sq, out, &n, bout, &nb));
    memcpy(c, b, (size_t)len), c[3] = NET_RUNES;
    CHECK(!net_unpack_shots(c, len, &pl, &sq, out, &n, bout, &nb));  // (not this type)

    // NET_RUNES: nothing / Magnesis / Stasis / both + 3 pillars
    NetRunes r, ro;
    memset(&r, 0, sizeof r);
    len = net_pack_runes(4, 9, &r, b, (int)sizeof b);
    CHECK(len == NET_HDR + 2 && net_unpack_runes(b, len, &pl, &sq, &ro) && pl == 4 && sq == 9 && ro.flags == 0 && ro.nice == 0);
    CHECK(net_unpack(b, len, &type, &pl, &sq, NULL, name) && type == NET_RUNES);
    r.flags = NRF_MAG | NRF_STOP, r.nice = 3;
    r.mag.hashid = 0xdeadbeef, r.mag.pos[0] = 1, r.mag.q[3] = 1;
    r.stop.hashid = 0x1234, r.stop.pos[2] = -5, r.stop.q[0] = 1;
    for (int i = 0; i < 3; i++) r.ice[i].pos[1] = 10.0f + i, r.ice[i].q[3] = 1;
    len = net_pack_runes(4, 10, &r, b, (int)sizeof b);
    CHECK(len == NET_RUNES_PKT && net_unpack_runes(b, len, &pl, &sq, &ro) && ro.flags == r.flags && ro.nice == 3 && ro.mag.hashid == 0xdeadbeef &&
          ro.stop.hashid == 0x1234 && ro.mag.pos[0] == 1 && ro.stop.pos[2] == -5 && ro.ice[2].pos[1] == 12 && near(ro.stop.q[0], 1, 1e-4f));
    CHECK(!net_unpack_runes(b, len - 1, &pl, &sq, &ro) && !net_unpack_runes(b, len + 1, &pl, &sq, &ro));
    memcpy(c, b, (size_t)len), c[NET_HDR] = 4;
    CHECK(!net_unpack_runes(c, len, &pl, &sq, &ro));  // (an unknown flag)
    memcpy(c, b, (size_t)len), c[NET_HDR + 1] = 4;
    CHECK(!net_unpack_runes(c, len, &pl, &sq, &ro));  // (more pillars than the max)
    r.flags = NRF_STOP, r.nice = 1;
    len = net_pack_runes(4, 11, &r, b, (int)sizeof b);
    CHECK(len == NET_HDR + 2 + 24 + 20 && net_unpack_runes(b, len, &pl, &sq, &ro) && ro.flags == NRF_STOP && ro.stop.hashid == 0x1234 && ro.nice == 1);
    r.flags = 8;
    CHECK(net_pack_runes(4, 1, &r, b, (int)sizeof b) < 0);
    CHECK(nrune_holds(&ro, 0x1234) == NRF_STOP && nrune_holds(&ro, 0xdeadbeef) == 0 && nrune_holds(&ro, 0) == 0 && nrune_holds(NULL, 0x1234) == 0);
}

// ---- rotations and the draw rows: the exe's skinning (sora_draw.c skin_verts, one bone, face 0) puts a vertex where BotW does ----
static void mtx_of(const float q[4], const float t[3], float m[12]) {
    float R[3][3];
    nshot_r_from_q(q, R);
    for (int j = 0; j < 3; j++) m[j * 4 + 0] = R[j][0], m[j * 4 + 1] = R[j][1], m[j * 4 + 2] = R[j][2], m[j * 4 + 3] = t[j];
}
static void exe_skin(const float rows[12], const float p_cm[3], const float feet[3], float face, float scale, float o[3]) {
    float c = cosf(face), s = sinf(face), q[3] = {0, 0, 0}, p[4] = {p_cm[0], p_cm[1], p_cm[2], 1};
    for (int r = 0; r < 4; r++)  // (row vectors: q += p[r] * row r)
        for (int j = 0; j < 3; j++) q[j] += p[r] * rows[r * 3 + j];
    o[0] = feet[0] + (c * q[0] + s * q[2]) * scale;
    o[1] = feet[1] + q[1] * scale;
    o[2] = feet[2] + (-s * q[0] + c * q[2]) * scale;
}
static void test_rot(void) {
    srand(289);
    bool ok_q = true, ok_place = true;
    for (int it = 0; it < 2000; it++) {
        float q[4] = {frand(), frand(), frand(), frand()}, l = 0;
        if (it < 6) {  // (half turns about x / y / z and their mixes: the matrix->quat branches past the trace)
            static const float h[6][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0.7071f, 0.7071f, 0, 0}, {0, 0.7071f, 0.7071f, 0}, {0.7071f, 0, 0.7071f, 0}};
            memcpy(q, h[it], sizeof q);
        }
        for (int i = 0; i < 4; i++) l += q[i] * q[i];
        l = sqrtf(l);
        for (int i = 0; i < 4; i++) q[i] /= l;
        float t[3] = {frand() * 3000, frand() * 300, frand() * 3000}, m[12], q2[4];
        mtx_of(q, t, m);
        if (it % 3 == 0)  // (a scaled actor matrix: the turn is the same)
            for (int j = 0; j < 3; j++) m[j * 4 + 0] *= 1.5f, m[j * 4 + 1] *= 0.8f, m[j * 4 + 2] *= 2.0f;
        nshot_q_from_m(m, q2);
        float d = q[0] * q2[0] + q[1] * q2[1] + q[2] * q2[2] + q[3] * q2[3];
        ok_q = ok_q && fabsf(fabsf(d) - 1) < 1e-4f;
        // a model vertex (cm) drawn by the exe with nshot_rows at the actor's place = BotW's R * v(m) + t
        float rows[12], v[3] = {frand() * 120, frand() * 60, frand() * 90}, o[3], R[3][3];
        nshot_rows(q2, rows);
        exe_skin(rows, v, t, 0, 0.01f, o);
        nshot_r_from_q(q, R);
        for (int j = 0; j < 3; j++) {
            float w = t[j] + (R[j][0] * v[0] + R[j][1] * v[1] + R[j][2] * v[2]) * 0.01f;
            ok_place = ok_place && near(o[j], w, 2e-3f + fabsf(w) * 1e-6f);
        }
    }
    CHECK(ok_q);
    CHECK(ok_place);
    // nlerp: the ends, the middle, the short way round
    float a[4] = {0, 0, 0, 1}, b[4] = {0, 0.7071068f, 0, 0.7071068f}, nb[4] = {0, -0.7071068f, 0, -0.7071068f}, o[4];
    nshot_nlerp(a, b, 0, o);
    CHECK(near(o[3], 1, 1e-6f));
    nshot_nlerp(a, b, 1, o);
    CHECK(near(o[1], 0.7071068f, 1e-5f));
    nshot_nlerp(a, nb, 0.5f, o);
    CHECK(near(o[1], 0.3826834f, 1e-4f) && near(o[3], 0.9238795f, 1e-4f));  // (22.5 deg about y, not the long way)
}

// ---- fake guest memory: the walk, names, links, the AS lookup, bodies (the PR's reads, here through rd) ----
static _Alignas(16) uint8_t g_arena[1 << 20];
static size_t g_used;
static bool fake_rd(uint64_t a, size_t n) { return a >= (uintptr_t)g_arena && a + n <= (uintptr_t)g_arena + g_used && a + n >= a; }
static uint64_t falloc(size_t n) {
    size_t at = (g_used + 15) & ~(size_t)15;
    g_used = at + n;
    memset(g_arena + at, 0, n);
    return (uintptr_t)(g_arena + at);
}
static void w64(uint64_t a, uint64_t v) { memcpy((void *)(uintptr_t)a, &v, 8); }
static void w32(uint64_t a, uint32_t v) { memcpy((void *)(uintptr_t)a, &v, 4); }
static void w8(uint64_t a, uint8_t v) { *(uint8_t *)(uintptr_t)a = v; }
static uint64_t fstr(const char *s) {
    uint64_t a = falloc(strlen(s) + 1);
    memcpy((void *)(uintptr_t)a, s, strlen(s) + 1);
    return a;
}
#define VT_UNIT 0x7001d00000ull
#define N_UNITS 40
static uint64_t g_unit[N_UNITS], g_act[N_UNITS];
static void fake_world(void) {
    g_used = 0;
    uint64_t head = falloc(0x58);
    w64(head, 0x1234);  // (the list's head: no unit)
    for (int i = 0; i < N_UNITS; i++) g_unit[i] = falloc(0x58), g_act[i] = falloc(0x720);
    for (int i = 0; i < N_UNITS; i++) {
        uint64_t u = g_unit[i], a = g_act[i];
        w64(u, VT_UNIT);
        w64(u + 0x10, (i ? g_unit[i - 1] : head) + 0x10);
        w64(u + 0x18, (i + 1 < N_UNITS ? g_unit[i + 1] : head) + 0x10);
        w32(u + 0x40, 1000u + (uint32_t)i), w64(u + 0x48, a);
        w32(a + 0x60, 1000u + (uint32_t)i), w64(a + 0x68, u), w8(a + 0x64, 1);
        char nm[32];
        snprintf(nm, sizeof nm, "Obj_Thing%02d", i);
        w64(a + 0x10, fstr(nm));
    }
}
static void test_walk(void) {
    fake_world();
    uint64_t out[64];
    int n = nshot_walk(fake_rd, g_unit[10], out, 64);
    // (the PR's walk: from unit 10 forward (+0x18) to the end, then back (+0x10) from unit 10 to the first; the forward run's
    // last one, the newest actor, kept: the PR's n-- dropped it)
    CHECK(n == N_UNITS);
    bool all = true;
    for (int i = 0; i < N_UNITS; i++) {
        bool f = false;
        for (int j = 0; j < n; j++) f = f || out[j] == g_act[i];
        all = all && f;
    }
    CHECK(all);
    CHECK(nshot_walk(fake_rd, g_unit[10], out, 5) == 5 && out[0] == g_act[11] && out[4] == g_act[15]);  // (the max)
    w32(g_act[12] + 0x60, 77);  // (an actor whose id is not its unit's: skipped)
    CHECK(nshot_walk(fake_rd, g_unit[10], out, 64) == N_UNITS - 1);
    w32(g_act[12] + 0x60, 1012);
    w64(g_unit[5] + 0x10, 0);  // (a broken link: the walk stops there, after unit 5's own actor)
    CHECK(nshot_walk(fake_rd, g_unit[10], out, 64) == N_UNITS - 5);
    CHECK(nshot_walk(fake_rd, 0, out, 64) == 0 && nshot_walk(fake_rd, (uintptr_t)g_arena + sizeof g_arena - 4, out, 64) == 0);
    // names, links, alive
    char nm[48];
    nshot_name(fake_rd, g_act[3], nm);
    CHECK(!strcmp(nm, "Obj_Thing03"));
    w64(g_act[4] + 0x10, fstr("Bad Name"));
    nshot_name(fake_rd, g_act[4], nm);
    CHECK(nm[0] == 0);
    w64(g_act[4] + 0x10, 0x10);
    nshot_name(fake_rd, g_act[4], nm);
    CHECK(nm[0] == 0);
    char longn[60];
    memset(longn, 'A', 50), longn[50] = 0;
    w64(g_act[4] + 0x10, fstr(longn));
    nshot_name(fake_rd, g_act[4], nm);
    CHECK(nm[0] == 0);  // (no name ends within 47 letters)
    CHECK(nshot_kind_of("NormalArrow") == NSK_NORMAL_ARROW && nshot_kind_of("RemoteBombCube2") == NSK_REMOTE_BOMB_CUBE2 && nshot_kind_of("BombArrow_A") == NSK_BOMB_ARROW &&
          nshot_kind_of("Weapon_Sword_001") < 0 && nshot_kind_of("") < 0 && nshot_kind_of(NULL) < 0 && nshot_kind_of("NormalArrowX") < 0 && nshot_kind_of("(thrown)") < 0);
    uint64_t link = falloc(16);
    w64(link, g_unit[7]), w32(link + 8, 1007);
    CHECK(nshot_linked(fake_rd, link) == g_act[7]);
    w32(link + 8, 1008);
    CHECK(nshot_linked(fake_rd, link) == 0);
    w32(link + 8, 1007), w8(g_act[7] + 0x64, 3);  // (being deleted)
    CHECK(nshot_linked(fake_rd, link) == 0);
    NsWatch w;
    w8(g_act[7] + 0x64, 1);
    CHECK(nshot_watch(fake_rd, g_act[7], &w) && w.unit == g_unit[7] && w.id == 1007 && nshot_alive(fake_rd, g_act[7], w));
    w32(g_unit[7] + 0x40, 5);  // (its unit holds another actor now)
    CHECK(!nshot_alive(fake_rd, g_act[7], w));
    // bodies (PR has_body: 8-aligned, its vtable in the image)
    uint64_t body = falloc(16);
    w64(body, 0x7001234560ull), w64(g_act[8] + 0x190, body);
    CHECK(nshot_has_body(fake_rd, g_act[8]));
    w64(body, 0x5000);
    CHECK(!nshot_has_body(fake_rd, g_act[8]));
    w64(g_act[8] + 0x190, body + 4);
    CHECK(!nshot_has_body(fake_rd, g_act[8]));
    // the AS lookup (ASList's, read-only): mgr +0xb8 slots, +0xc0 the table (0x50 each: +0x20 banks, +0x28 the banks, 0x98 each)
    uint64_t mgr = falloc(0xd0), tab = falloc(3 * 0x50), banks1 = falloc(2 * 0x98), banks0 = falloc(0x98), as1 = falloc(0x20), as0 = falloc(0x20);
    w32(mgr + 0xb8, 3), w64(mgr + 0xc0, tab);
    w32(tab + 0x20, 1), w64(tab + 0x28, banks0);
    w32(tab + 0x50 + 0x20, 2), w64(tab + 0x50 + 0x28, banks1);
    w32(tab + 0xa0 + 0x20, 0);
    w64(banks0, as0), w64(banks0 + 0x18, 1), w64(as0 + 0x10, fstr("Explode"));
    w64(banks1 + 0x98, as1), w64(banks1 + 0x98 + 0x18, 1), w64(as1 + 0x10, fstr("Player_BowCharge"));
    w64(g_act[9] + 0x560, mgr);
    CHECK(!strcmp(nshot_as_name(fake_rd, g_act[9], 0, 0), "Explode") && !strcmp(nshot_as_name(fake_rd, g_act[9], 1, 1), "Player_BowCharge"));
    CHECK(!strcmp(nshot_as_name(fake_rd, g_act[9], 1, 0), "") && !strcmp(nshot_as_name(fake_rd, g_act[9], 1, 2), "") && !strcmp(nshot_as_name(fake_rd, g_act[9], 2, 0), "") &&
          !strcmp(nshot_as_name(fake_rd, g_act[9], 3, 0), "") && !strcmp(nshot_as_name(fake_rd, g_act[9], -1, 0), ""));
    CHECK(nshot_bow_as(fake_rd, g_act[9]));
    w64(banks1 + 0x98 + 0x18, 0);  // (nothing plays in that bank: the default AS)
    CHECK(!nshot_bow_as(fake_rd, g_act[9]));
    CHECK(!strcmp(nshot_as_name(fake_rd, g_act[10], 0, 0), "") && !nshot_bow_as(fake_rd, g_act[10]));  // (no AS manager)
    // parents (BaseProc +0x80; one being deleted, +0x64 state 3, is none): the arrow on his bow, in his hand, an archer's
    uint64_t pl = g_act[20], bow = g_act[21], arrow = g_act[22], other = g_act[23];
    w64(bow + 0x80, pl), w64(arrow + 0x80, bow);
    CHECK(nshot_parent(fake_rd, arrow) == bow && nshot_parent(fake_rd, bow) == pl && nshot_parent(fake_rd, pl) == 0);
    CHECK(nshot_on_him(fake_rd, arrow, pl, bow) && nshot_on_him(fake_rd, arrow, pl, 0));  // (his bow; or a child of a child of his)
    CHECK(!nshot_on_him(fake_rd, arrow, other, 0) && !nshot_on_him(fake_rd, arrow, 0, bow));
    w64(bow + 0x80, 0);
    CHECK(nshot_on_him(fake_rd, arrow, pl, bow) && !nshot_on_him(fake_rd, arrow, pl, 0));  // (the bow known by his link only)
    w64(arrow + 0x80, pl);
    CHECK(nshot_on_him(fake_rd, arrow, pl, 0));  // (in his hand)
    w64(arrow + 0x80, other);
    CHECK(!nshot_on_him(fake_rd, arrow, pl, bow));  // (an archer's)
    w64(arrow + 0x80, bow), w8(bow + 0x64, 3);
    CHECK(nshot_parent(fake_rd, arrow) == 0 && !nshot_on_him(fake_rd, arrow, pl, bow));  // (its bow being deleted)
    w8(bow + 0x64, 1), w64(arrow + 0x80, 0x10);
    CHECK(nshot_parent(fake_rd, arrow) == 0 && !nshot_on_him(fake_rd, arrow, pl, bow));  // (unreadable)
    w64(arrow + 0x80, 0);
    CHECK(nshot_parent(fake_rd, arrow) == 0 && !nshot_on_him(fake_rd, arrow, pl, bow));
}

// ---- which shots are his ----
static void mtx_at(float m[12], float x, float y, float z) {
    memset(m, 0, 12 * sizeof(float));
    m[0] = m[5] = m[10] = 1, m[3] = x, m[7] = y, m[11] = z;
}
static void test_local(void) {
    NsLocal L;
    nsl_init(&L, 500);
    float link[3] = {100, 10, 200}, m[12];
    uint64_t t = 1000;
    NsWatch w = {0x5000, 1};
    NetShot s[NSHOT_MAX];
    NetBoom b[NSHOT_BOOMS];
    bool mv = false;
    // the bowing window: a Bow AS now or in the last second
    CHECK(!nsl_bowing(&L, t, false) && nsl_bowing(&L, t, true) && nsl_bowing(&L, t + 999, false) && !nsl_bowing(&L, t + 1000, false));
    // an arrow first seen 3 m away while bowing: his; not sent before it moved and left his hands
    nsl_bowing(&L, t, true);
    mtx_at(m, 101, 11.5f, 200);
    CHECK(nsl_offer(&L, t, 0xa1, w, NSK_FIRE_ARROW, m, link, true, false, NULL) == NSO_TAKEN && nsl_offer(&L, t, 0xa1, w, NSK_FIRE_ARROW, m, link, true, false, NULL) == NSO_KNOWN);
    CHECK(nsl_list(&L, t, s, &mv) == 0);
    t += 33; CHECK(nsl_update(&L, 0, t, true, m, false, link, true, false) && nsl_list(&L, t, s, &mv) == 0);  // (on his string, still)
    mtx_at(m, 100.8f, 11.0f, 200.5f);  // (1.4 m from his feet)
    t += 33; CHECK(nsl_update(&L, 0, t, true, m, false, link, true, false) && nsl_list(&L, t, s, &mv) == 0);  // (moved, still in his hands)
    mtx_at(m, 103.5f, 11.5f, 201);
    t += 33; CHECK(nsl_update(&L, 0, t, true, m, false, link, true, false) && nsl_list(&L, t, s, &mv) == 1 && mv && s[0].kind == NSK_FIRE_ARROW && s[0].id == 500 &&
                   s[0].pos[0] == 103.5f && !(s[0].flags & NSF_STILL));
    // it sticks: NSF_STILL after half a second, gone (and remembered) after 3 s; offered again = not his
    for (int i = 0; i < 20; i++) t += 33, nsl_update(&L, 0, t, true, m, false, link, false, false);
    CHECK(nsl_list(&L, t, s, &mv) == 1 && (s[0].flags & NSF_STILL) && !mv);
    for (int i = 0; i < 100 && L.tr[0].actor; i++) t += 33, nsl_update(&L, 0, t, true, m, false, link, false, false);
    CHECK(!L.tr[0].actor && L.st.still == 1 && nsl_list(&L, t, s, &mv) == 0);
    nsl_bowing(&L, t, true);
    CHECK(nsl_offer(&L, t, 0xa1, w, NSK_FIRE_ARROW, m, link, true, false, NULL) == NSO_NOT_HIS);
    // an archer's arrow: first seen 6 m away while bowing = not his, also once it comes near; first seen near while NOT bowing = not his
    mtx_at(m, 106, 10, 200);
    NsWatch w2 = {0x5100, 2}, w3 = {0x5200, 3};
    CHECK(nsl_offer(&L, t, 0xa2, w2, NSK_NORMAL_ARROW, m, link, true, false, NULL) == NSO_NOT_HIS);
    mtx_at(m, 100.5f, 10, 200);
    CHECK(nsl_offer(&L, t, 0xa2, w2, NSK_NORMAL_ARROW, m, link, true, false, NULL) == NSO_NOT_HIS);
    CHECK(nsl_offer(&L, t, 0xa3, w3, NSK_NORMAL_ARROW, m, link, false, false, NULL) == NSO_NOT_HIS && nsl_offer(&L, t, 0xa3, w3, NSK_NORMAL_ARROW, m, link, true, false, NULL) == NSO_NOT_HIS);
    // the same actor with a new id (the game reused it): a new arrow, judged again
    NsWatch w3b = {0x5200, 33};
    CHECK(nsl_offer(&L, t, 0xa3, w3b, NSK_NORMAL_ARROW, m, link, true, false, NULL) == NSO_TAKEN);
    // one that never moves: dropped and remembered once his bow is away
    int k3 = nsl_find(&L, 0xa3, 33);
    CHECK(k3 >= 0);
    t += 33; CHECK(nsl_update(&L, k3, t, true, m, false, link, true, false));
    t += 33; CHECK(!nsl_update(&L, k3, t, true, m, false, link, false, false) && L.st.unmoved == 1);
    CHECK(nsl_offer(&L, t, 0xa3, w3b, NSK_NORMAL_ARROW, m, link, true, false, NULL) == NSO_NOT_HIS);
    // a Remote Bomb: his at once, sent from the first look (held over his head), its Explode = one boom, no longer a shot
    NsWatch wb = {0x6000, 9};
    mtx_at(m, 100, 12, 200);
    CHECK(nsl_offer(&L, t, 0xb1, wb, NSK_REMOTE_BOMB, m, link, false, false, NULL) == NSO_TAKEN);
    int kb = nsl_find(&L, 0xb1, 9);
    CHECK(nsl_list(&L, t, s, &mv) == 1 && s[0].kind == NSK_REMOTE_BOMB);
    for (int i = 0; i < 200; i++) t += 33, nsl_update(&L, kb, t, true, m, false, link, false, false);  // (a bomb lying still stays)
    CHECK(L.tr[kb].actor && nsl_list(&L, t, s, &mv) == 1 && (s[0].flags & NSF_STILL));
    CHECK(nsl_booms(&L, t, b) == 0);
    t += 33; CHECK(nsl_update(&L, kb, t, true, m, true, link, false, false) && L.st.booms == 1 && nsl_list(&L, t, s, &mv) == 0);
    CHECK(nsl_booms(&L, t, b) == 1 && b[0].kind == NSK_REMOTE_BOMB && b[0].seq != 0 && b[0].pos[1] == 12);
    uint16_t seq1 = b[0].seq;
    t += 33, nsl_update(&L, kb, t, true, m, true, link, false, false);
    CHECK(L.st.booms == 1 && nsl_booms(&L, t, b) == 1);  // (Explode again: still one boom)
    t += 33; CHECK(!nsl_update(&L, kb, t, false, m, false, link, false, false) && L.st.put_away == 0 && L.st.gone == 1);
    CHECK(nsl_booms(&L, t + NSHOT_BOOM_MS - 200, b) == 1 && nsl_booms(&L, t + NSHOT_BOOM_MS, b) == 0);
    // a cube put away (gone without Explode): no boom
    NsWatch wc = {0x6100, 10};
    CHECK(nsl_offer(&L, t, 0xb2, wc, NSK_REMOTE_BOMB_CUBE, m, link, false, false, NULL) == NSO_TAKEN);
    int kc = nsl_find(&L, 0xb2, 10);
    t += 33; CHECK(!nsl_update(&L, kc, t, false, m, false, link, false, false) && L.st.put_away == 1 && L.st.booms == 1);
    // two booms: both listed, newest first, seqs apart
    NsWatch wd = {0x6200, 11}, we = {0x6300, 12};
    nsl_offer(&L, t, 0xb3, wd, NSK_REMOTE_BOMB2, m, link, false, false, NULL), nsl_offer(&L, t, 0xb4, we, NSK_REMOTE_BOMB_CUBE2, m, link, false, false, NULL);
    int kd = nsl_find(&L, 0xb3, 11), ke = nsl_find(&L, 0xb4, 12);
    t += 33, nsl_update(&L, kd, t, true, m, true, link, false, false), nsl_update(&L, ke, t, true, m, true, link, false, false);
    CHECK(nsl_booms(&L, t, b) == 3 && b[0].kind == NSK_REMOTE_BOMB_CUBE2 && b[1].kind == NSK_REMOTE_BOMB2 && b[0].seq != b[1].seq && b[1].seq != seq1);
    nsl_drop(&L, kd, false), nsl_drop(&L, ke, false);
    // his thrown weapon: followed, sent once it moved, ours again = dropped; let go and never moved = dropped after 3 s
    NsWatch wt = {0x7000, 20};
    mtx_at(m, 100.5f, 11, 200);
    CHECK(nsl_offer(&L, t, 0xc1, wt, NSK_THROWN, m, link, false, false, NULL) == NSO_BAD);
    CHECK(nsl_offer(&L, t, 0xc1, wt, NSK_THROWN, m, link, false, false, "Weapon_Sword_070") == NSO_TAKEN);
    int kt = nsl_find(&L, 0xc1, 20);
    CHECK(nsl_list(&L, t, s, &mv) == 0);
    mtx_at(m, 101.5f, 11.2f, 200);
    t += 33, nsl_update(&L, kt, t, true, m, false, link, false, false);
    CHECK(nsl_list(&L, t, s, &mv) == 1 && s[0].kind == NSK_THROWN && !strcmp(s[0].name, "Weapon_Sword_070") && nsl_flying(&L, 0xc1) == kt && nsl_flying(&L, 0xc2) < 0);
    nsl_drop(&L, kt, false);
    CHECK(nsl_list(&L, t, s, &mv) == 0 && nsl_flying(&L, 0xc1) < 0);
    CHECK(nsl_offer(&L, t, 0xc1, wt, NSK_THROWN, m, link, false, false, "Weapon_Sword_070") == NSO_TAKEN);
    kt = nsl_find(&L, 0xc1, 20);
    for (int i = 0; i < 95 && L.tr[kt].actor; i++) t += 33, nsl_update(&L, kt, t, true, m, false, link, false, false);
    CHECK(!L.tr[kt].actor);
    // full: 8 followed; a 9th takes the place of the one lying still the longest, never a bomb or one in flight
    NsLocal F;
    nsl_init(&F, 65534);
    for (int i = 0; i < NSHOT_MAX; i++) {
        NsWatch wi = {0x8000 + (uint64_t)i * 0x100, 100u + (uint32_t)i};
        mtx_at(m, 101, 11, 200);
        CHECK(nsl_offer(&F, t, 0xd0 + (uint64_t)i, wi, i < 2 ? NSK_REMOTE_BOMB : NSK_NORMAL_ARROW, m, link, true, false, NULL) == NSO_TAKEN);
    }
    CHECK(F.tr[1].id == 65535 && F.tr[2].id == 1);  // (ids skip 0 when they wrap)
    NsWatch w9 = {0x9900, 999};
    CHECK(nsl_offer(&F, t, 0xe0, w9, NSK_NORMAL_ARROW, m, link, true, false, NULL) == NSO_FULL);  // (none lies still yet)
    CHECK(F.why == NSW_FULL && F.st.full == 1);
    t += 33;
    for (int i = 2; i < NSHOT_MAX; i++) {  // (6 arrows fly, then stick one after another)
        mtx_at(m, 104 + i, 11, 200);
        nsl_update(&F, i, t, true, m, false, link, true, false);
    }
    CHECK(F.st.flew == 6);
    for (int i = 2; i < NSHOT_MAX; i++) t += 10, mtx_at(m, 104 + i, 11, 200), nsl_update(&F, i, t, true, m, false, link, true, false);  // (each still from a later time)
    mtx_at(m, 101, 11, 200);
    CHECK(nsl_offer(&F, t, 0xe0, w9, NSK_NORMAL_ARROW, m, link, true, false, NULL) == NSO_TAKEN && F.st.evicted == 1 && nsl_find(&F, 0xd2, 102) < 0 &&
          nsl_find(&F, 0xd0, 100) >= 0 && nsl_find(&F, 0xd1, 101) >= 0 && F.evicted_id == 1);
    CHECK(nsl_rejected(&F, 0xd2, 102));  // (the evicted arrow flew and stuck: never taken again)
    // full again: the one on his bow never gives way (even lying still the longest); one that only moved in his hands gives way,
    // not remembered
    NsLocal G;
    nsl_init(&G, 10);
    mtx_at(m, 100.5f, 11.9f, 200.6f);
    NsWatch wh = {0xa000, 300};
    CHECK(nsl_offer(&G, t, 0xf0, wh, NSK_NORMAL_ARROW, m, link, true, true, NULL) == NSO_TAKEN && G.why == NSW_ON_BOW);
    for (int i = 1; i < NSHOT_MAX; i++) {
        NsWatch wi = {0xa000 + (uint64_t)i * 0x100, 300u + (uint32_t)i};
        mtx_at(m, 101, 11, 200);
        CHECK(nsl_offer(&G, t, 0xf0 + (uint64_t)i, wi, NSK_NORMAL_ARROW, m, link, true, false, NULL) == NSO_TAKEN && G.why == NSW_NEAR);
    }
    t += 33;
    mtx_at(m, 100.5f, 11.9f, 200.6f), nsl_update(&G, 0, t, true, m, false, link, true, true);  // (on his bow, still from now: the oldest)
    for (int i = 1; i < NSHOT_MAX; i++) mtx_at(m, 101.2f, 11, 200), nsl_update(&G, i, t, true, m, false, link, true, false);  // (20 cm: his hands)
    for (int i = 1; i < NSHOT_MAX; i++) t += 10, mtx_at(m, 101.2f, 11, 200), nsl_update(&G, i, t, true, m, false, link, true, false);
    NsWatch wn = {0xb000, 400};
    CHECK(G.st.flew == 0 && nsl_offer(&G, t, 0xff, wn, NSK_NORMAL_ARROW, m, link, true, false, NULL) == NSO_TAKEN && G.evicted_id == 11 &&
          nsl_find(&G, 0xf0, 300) >= 0 && nsl_find(&G, 0xf1, 301) < 0 && !nsl_rejected(&G, 0xf1, 301));
}

// ---- the arrow on his bow (s289 lab: one sent at 2.01 m while still on his string; the next ones waited 3 s there, were dropped
// as lying still, remembered as not his, and flew unsent: taken 6, lay still 5, gone 1). Link's feet 100 10 200 unless he moves;
// on the string at full draw: +0.5 +1.9 +0.6 (2.05 m) ----
static void test_bow(void) {
    NsLocal L;
    nsl_init(&L, 700);
    float feet[3] = {100, 10, 200}, m[12];
    uint64_t t = 5000;
    NetShot s[NSHOT_MAX];
    bool mv = false;
    // 1. nocked: on his bow 10 s (still 5 s, a 2 cm sway, still 5 s more): never let go, never sent; he lets go: still on the bow
    // for a look (the game's cut comes later) while it moves 1.5 m, then cut, fast and far: it flies, sent from then on
    NsWatch w1 = {0x5000, 1};
    mtx_at(m, 100.5f, 11.9f, 200.6f);
    CHECK(nsl_offer(&L, t, 0xa1, w1, NSK_ICE_ARROW, m, feet, true, true, NULL) == NSO_TAKEN && L.why == NSW_ON_BOW && L.st.on_bow == 1);
    bool kept = true, quiet = true;
    for (int i = 0; i < 300; i++) {
        t += 33;
        mtx_at(m, i < 150 ? 100.5f : 100.52f, 11.9f, 200.6f);
        kept = kept && nsl_update(&L, 0, t, true, m, false, feet, true, true);
        quiet = quiet && nsl_list(&L, t, s, &mv) == 0;
    }
    CHECK(kept && quiet && L.tr[0].actor && L.tr[0].moved && L.tr[0].held && !L.tr[0].flew && L.st.still == 0 && L.st.unmoved == 0);
    t += 33, mtx_at(m, 102.0f, 11.9f, 200.6f);  // (45 m/s but 2.8 m from his feet, on the bow)
    CHECK(nsl_update(&L, 0, t, true, m, false, feet, true, true) && !L.tr[0].flew && nsl_list(&L, t, s, &mv) == 0);
    t += 33, mtx_at(m, 104.0f, 11.9f, 200.6f);  // (cut: 60 m/s, 4.5 m from his feet)
    CHECK(nsl_update(&L, 0, t, true, m, false, feet, true, false) && L.tr[0].flew && L.st.flew == 1 && L.tr[0].speed > 50);
    CHECK(nsl_list(&L, t, s, &mv) == 1 && mv && s[0].kind == NSK_ICE_ARROW && s[0].id == 700 && s[0].pos[0] == 104.0f);
    for (int i = 0; i < 100 && L.tr[0].actor; i++) t += 33, nsl_update(&L, 0, t, true, m, false, feet, true, false);  // (it sticks)
    CHECK(!L.tr[0].actor && L.end == NSE_STILL && L.st.still == 1 && nsl_rejected(&L, 0xa1, 1));
    // 2. the lab's shot 1: one on his string NOT seen on his bow (no bow link): taken near his feet, moving with his hands at
    // 2.01 m (dy 1.9): never sent (the old rule sent it: moved, and over 2 m away); fast and far: sent
    NsWatch w2 = {0x5100, 2};
    mtx_at(m, 100.4f, 11.9f, 200.5f);
    CHECK(nsl_offer(&L, t, 0xa2, w2, NSK_ICE_ARROW, m, feet, true, false, NULL) == NSO_TAKEN && L.why == NSW_NEAR);
    int k = nsl_find(&L, 0xa2, 2);
    for (int i = 0; i < 30; i++)
        t += 33, mtx_at(m, 100.4f + 0.01f * (float)(i % 5), 11.9f + 0.003f * (float)i, 200.5f), nsl_update(&L, k, t, true, m, false, feet, true, false);
    CHECK(k >= 0 && L.tr[k].moved && L.tr[k].away && !L.tr[k].flew && !nsl_sent(&L.tr[k]) && nsl_list(&L, t, s, &mv) == 0);
    t += 33, mtx_at(m, 103.0f, 12.0f, 200.5f);
    CHECK(nsl_update(&L, k, t, true, m, false, feet, true, false) && L.tr[k].flew && nsl_list(&L, t, s, &mv) == 1);
    nsl_drop(&L, k, false);
    // 3. bullet time: let go slowly (9 m/s): flies once it is off his bow 2 looks and 0.25 m farther from his feet (3 looks here)
    NsWatch w3 = {0x5200, 3};
    mtx_at(m, 100.5f, 11.9f, 200.6f);
    CHECK(nsl_offer(&L, t, 0xa3, w3, NSK_FIRE_ARROW, m, feet, true, true, NULL) == NSO_TAKEN);
    k = nsl_find(&L, 0xa3, 3);
    t += 33;
    CHECK(nsl_update(&L, k, t, true, m, false, feet, true, true) && L.tr[k].off == 0);
    float x = 100.5f;
    for (int i = 1; i <= 2; i++) {
        x += 0.3f, t += 33, mtx_at(m, x, 11.9f, 200.6f);
        CHECK(nsl_update(&L, k, t, true, m, false, feet, true, false) && !L.tr[k].flew && L.tr[k].off == i && !nsl_sent(&L.tr[k]));
    }
    x += 0.3f, t += 33, mtx_at(m, x, 11.9f, 200.6f);
    CHECK(nsl_update(&L, k, t, true, m, false, feet, true, false) && L.tr[k].flew && L.tr[k].speed < NSHOT_FLY_MPS && nsl_sent(&L.tr[k]));
    nsl_drop(&L, k, false);
    // 4. our bow link lost for 3 looks at a time while he walks (6 m/s; the arrow goes along at 2 m): never a shot
    NsWatch w4 = {0x5300, 4};
    mtx_at(m, 100.5f, 11.9f, 200.6f);
    CHECK(nsl_offer(&L, t, 0xa4, w4, NSK_NORMAL_ARROW, m, feet, true, true, NULL) == NSO_TAKEN);
    k = nsl_find(&L, 0xa4, 4);
    kept = true;
    for (int i = 0; i < 40; i++) {
        feet[0] += 0.2f, t += 33, mtx_at(m, feet[0] + 0.5f, 11.9f, 200.6f);
        kept = kept && nsl_update(&L, k, t, true, m, false, feet, true, i % 4 == 0);
    }
    CHECK(kept && !L.tr[k].flew && !nsl_sent(&L.tr[k]) && L.tr[k].off == 3);
    nsl_drop(&L, k, false), feet[0] = 100;
    // 5. at a gallop (15 m/s): the one on his bow goes along (never a shot); one lying on the ground he rides past while aiming:
    // never moves, never sent, let go (remembered) once his bow is away; the one on his bow, his bow unused 3 s: let go, not
    // remembered, not taken again while his bow is unused, taken once he uses it
    NsWatch w5 = {0x5400, 5}, w6 = {0x5500, 6};
    float g[12];
    mtx_at(m, 100.5f, 11.9f, 200.6f), mtx_at(g, 103, 10.05f, 200);
    CHECK(nsl_offer(&L, t, 0xa5, w5, NSK_NORMAL_ARROW, m, feet, true, true, NULL) == NSO_TAKEN);
    CHECK(nsl_offer(&L, t, 0xa6, w6, NSK_NORMAL_ARROW, g, feet, true, false, NULL) == NSO_TAKEN && L.why == NSW_NEAR);
    int k5 = nsl_find(&L, 0xa5, 5), k6 = nsl_find(&L, 0xa6, 6);
    for (int i = 0; i < 30; i++) {
        feet[0] += 0.5f, t += 33, mtx_at(m, feet[0] + 0.5f, 11.9f, 200.6f);
        nsl_update(&L, k5, t, true, m, false, feet, true, true), nsl_update(&L, k6, t, true, g, false, feet, true, false);
    }
    CHECK(L.tr[k5].actor && L.tr[k5].speed > 14 && !L.tr[k5].flew && !nsl_sent(&L.tr[k5]));
    CHECK(L.tr[k6].actor && !L.tr[k6].moved && L.tr[k6].dist > 10 && !L.tr[k6].flew && !nsl_sent(&L.tr[k6]));
    t += 33;
    CHECK(!nsl_update(&L, k6, t, true, g, false, feet, false, false) && L.end == NSE_UNMOVED && nsl_rejected(&L, 0xa6, 6));
    CHECK(nsl_update(&L, k5, t, true, m, false, feet, false, true));  // (his bow away, the arrow still on it: kept a while)
    for (int i = 0; i < 100 && L.tr[k5].actor; i++) t += 33, nsl_update(&L, k5, t, true, m, false, feet, false, true);
    CHECK(!L.tr[k5].actor && L.end == NSE_IDLE && L.st.idle == 1 && !nsl_rejected(&L, 0xa5, 5));
    CHECK(nsl_offer(&L, t, 0xa5, w5, NSK_NORMAL_ARROW, m, feet, false, true, NULL) == NSO_NOT_HIS && L.why == NSW_IDLE_BOW && !nsl_rejected(&L, 0xa5, 5) &&
          nsl_find(&L, 0xa5, 5) < 0);
    CHECK(nsl_offer(&L, t, 0xa5, w5, NSK_NORMAL_ARROW, m, feet, true, true, NULL) == NSO_TAKEN && L.why == NSW_ON_BOW);
    nsl_drop(&L, nsl_find(&L, 0xa5, 5), false), feet[0] = 100;
    // 6. remembered as not his (the lab: after 3 s on his string), then seen on his bow while he uses it: his again, taken
    mtx_at(m, 100.5f, 11.9f, 200.6f);
    CHECK(nsl_rejected(&L, 0xa6, 6) && nsl_offer(&L, t, 0xa6, w6, NSK_NORMAL_ARROW, m, feet, true, false, NULL) == NSO_NOT_HIS && L.why == NSW_REJECTED);
    CHECK(nsl_offer(&L, t, 0xa6, w6, NSK_NORMAL_ARROW, m, feet, true, true, NULL) == NSO_TAKEN && L.why == NSW_ON_BOW && !nsl_rejected(&L, 0xa6, 6));
    nsl_drop(&L, nsl_find(&L, 0xa6, 6), false);
    // 7. one that only moved in his hands (no bow link), then lay still 3 s: let go, not remembered (it may still fly later)
    NsWatch w7 = {0x5600, 7};
    CHECK(nsl_offer(&L, t, 0xa7, w7, NSK_NORMAL_ARROW, m, feet, true, false, NULL) == NSO_TAKEN);
    k = nsl_find(&L, 0xa7, 7);
    t += 33, mtx_at(m, 100.6f, 11.9f, 200.6f), nsl_update(&L, k, t, true, m, false, feet, true, false);
    for (int i = 0; i < 100 && L.tr[k].actor; i++) t += 33, nsl_update(&L, k, t, true, m, false, feet, true, false);
    CHECK(!L.tr[k].actor && L.end == NSE_STILL && !nsl_rejected(&L, 0xa7, 7));
    // 8. jumps: 40 m in one look (load, warp) is never a flight, nor a move at the same time stamp; a real look after: it flies
    NsWatch w8 = {0x5700, 8};
    mtx_at(m, 101, 11, 200);
    CHECK(nsl_offer(&L, t, 0xa8, w8, NSK_NORMAL_ARROW, m, feet, true, false, NULL) == NSO_TAKEN);
    k = nsl_find(&L, 0xa8, 8);
    t += 33, mtx_at(m, 141, 11, 200);
    CHECK(nsl_update(&L, k, t, true, m, false, feet, true, false) && !L.tr[k].flew && L.tr[k].speed > 1000);
    mtx_at(m, 144, 11, 200);
    CHECK(nsl_update(&L, k, t, true, m, false, feet, true, false) && !L.tr[k].flew && L.tr[k].speed == 0);
    t += 33, mtx_at(m, 146, 11, 200);
    CHECK(nsl_update(&L, k, t, true, m, false, feet, true, false) && L.tr[k].flew);
    nsl_drop(&L, k, false);
    // 9. why (the trace's words): every reason has its words
    NsLocal W;
    nsl_init(&W, 1);
    NsWatch wa = {0x6000, 50}, wb = {0x6100, 51}, wc = {0x6200, 52}, wd = {0x6300, 53};
    mtx_at(m, 106, 10, 200);
    CHECK(nsl_offer(&W, t, 0xc1, wa, NSK_NORMAL_ARROW, m, feet, true, false, NULL) == NSO_NOT_HIS && W.why == NSW_TOO_FAR);
    CHECK(nsl_offer(&W, t, 0xc1, wa, NSK_NORMAL_ARROW, m, feet, true, false, NULL) == NSO_NOT_HIS && W.why == NSW_REJECTED);
    mtx_at(m, 101, 10, 200);
    CHECK(nsl_offer(&W, t, 0xc2, wb, NSK_NORMAL_ARROW, m, feet, false, false, NULL) == NSO_NOT_HIS && W.why == NSW_NOT_BOWING);
    CHECK(nsl_offer(&W, t, 0xc3, wc, NSK_REMOTE_BOMB, m, feet, false, false, NULL) == NSO_TAKEN && W.why == NSW_BOMB);
    CHECK(nsl_offer(&W, t, 0xc4, wd, NSK_THROWN, m, feet, false, false, "Weapon_Sword_001") == NSO_TAKEN && W.why == NSW_THROWN);
    CHECK(nsl_offer(&W, t, 0xc4, wd, NSK_THROWN, m, feet, false, false, "Weapon_Sword_001") == NSO_KNOWN && W.why == NSW_NONE);
    bool words = true;
    for (int i = 1; i < NSW_N; i++) words = words && NSW_TXT[i] && NSW_TXT[i][0];
    for (int i = 1; i < NSE_N; i++) words = words && NSE_TXT[i] && NSE_TXT[i][0];
    CHECK(words && !strcmp(NSW_TXT[NSW_ON_BOW], "taken: on our bow") && !strcmp(NSE_TXT[NSE_STILL], "lay still"));
}

// ---- the walk's cadence (s289 lab: one walk of 3297 us among walks of 360 us had stopped the bowing walks for the session) ----
static void test_walk_level(void) {
    CHECK(nshot_walk_level(0, 360, 3000, 12000) == 0 && nshot_walk_level(0, 3100, 3000, 12000) == 1 && nshot_walk_level(0, 13000, 3000, 12000) == 2);
    CHECK(nshot_walk_level(1, 2000, 3000, 12000) == 1 && nshot_walk_level(1, 1400, 3000, 12000) == 0 && nshot_walk_level(1, 12500, 3000, 12000) == 2);
    CHECK(nshot_walk_level(2, 7000, 3000, 12000) == 2 && nshot_walk_level(2, 5000, 3000, 12000) == 1 && nshot_walk_level(2, 1000, 3000, 12000) == 0);
    float avg = 0;
    int lv = 0, most = 0;
    for (int i = 0; i < 300; i++) {
        avg += ((i == 150 ? 3297.0f : 360.0f) - avg) / 8, lv = nshot_walk_level(lv, avg, 3000, 12000);
        if (lv > most) most = lv;
    }
    CHECK(most == 0 && lv == 0);
    for (int i = 0; i < 60; i++) avg += (5000 - avg) / 8, lv = nshot_walk_level(lv, avg, 3000, 12000);  // (slow for a while: every 6th frame)
    CHECK(lv == 1);
    for (int i = 0; i < 60; i++) avg += (2000 - avg) / 8, lv = nshot_walk_level(lv, avg, 3000, 12000);  // (better, not under half: stays)
    CHECK(lv == 1);
    for (int i = 0; i < 60; i++) avg += (800 - avg) / 8, lv = nshot_walk_level(lv, avg, 3000, 12000);  // (fast again: every frame while bowing)
    CHECK(lv == 0);
    avg = 0, lv = 0;
    avg += (50000 - avg) / 8, lv = nshot_walk_level(lv, avg, 3000, 12000);  // (one walk of 50 ms: at once)
    CHECK(lv == 1);
}

// ---- a friend's tracks ----
static void test_remote(void) {
    NsPeer P;
    memset(&P, 0, sizeof P);
    uint64_t t = 50000;
    NetShot s[2];
    float p[3], q[4];
    s[0] = shot(7, NSK_NORMAL_ARROW, 0, 10, 0, NULL);
    nsp_take(&P, t, s, 1, NULL, 0);
    NstTrack *tr = nsp_track(&P, 7);
    CHECK(tr && P.st.new_tracks == 1 && !strcmp(tr->name, "NormalArrow"));
    CHECK(!nsp_sample(tr, t - 1, p, q) && nsp_sample(tr, t, p, q) && p[0] == 0);  // (nothing before its first sample)
    s[0].pos[0] = 2;
    nsp_take(&P, t + 50, s, 1, NULL, 0);
    s[0].pos[0] = 4;
    nsp_take(&P, t + 100, s, 1, NULL, 0);
    CHECK(nsp_sample(tr, t + 25, p, q) && near(p[0], 1, 1e-5f) && nsp_sample(tr, t + 75, p, q) && near(p[0], 3, 1e-5f));
    CHECK(nsp_sample(tr, t + 150, p, q) && near(p[0], 6, 1e-4f));    // (50 ms past his newest: on at its speed)
    CHECK(nsp_sample(tr, t + 1000, p, q) && near(p[0], 8, 1e-4f));   // (then held after NSHOT_EXTRA_MS)
    // still: no extrapolation
    s[0].flags = NSF_STILL;
    nsp_take(&P, t + 150, s, 1, NULL, 0);
    CHECK(nsp_sample(tr, t + 400, p, q) && near(p[0], 4, 1e-5f));
    // his next packet without it: ended; shown until its last sample has played, then freed
    nsp_take(&P, t + 200, NULL, 0, NULL, 0);
    CHECK(tr->ended && P.st.ended == 1 && nsp_sample(tr, t + 140, p, q) && !nsp_sample(tr, t + 151, p, q));
    CHECK(nsp_reap(&P, t + 200, t + 80) == 1 && nsp_reap(&P, t + 300, t + 180) == 0 && !tr->used);
    // the same id again after it ended: a new shot (a fresh track)
    s[0] = shot(7, NSK_ICE_ARROW, 50, 10, 0, NULL);
    nsp_take(&P, t + 300, s, 1, NULL, 0);
    tr = nsp_track(&P, 7);
    CHECK(tr && !tr->ended && tr->kind == NSK_ICE_ARROW && tr->ns == 1 && P.st.new_tracks == 2);
    // quiet: ended after NSHOT_QUIET_MS without a packet
    CHECK(nsp_reap(&P, t + 300 + NSHOT_QUIET_MS, t + 300) == 1 && !tr->ended);
    CHECK(nsp_reap(&P, t + 301 + NSHOT_QUIET_MS, t + 300) == 1 && tr->ended && nsp_reap(&P, t + 1500, t + 1400) == 0);
    // booms: once each (the repeats dropped), played NET_INTERP_MS after they came (at = now - 120)
    NetBoom b[2] = {{.seq = 40, .kind = NSK_REMOTE_BOMB, .pos = {1, 2, 3}}, {.seq = 41, .kind = NSK_REMOTE_BOMB_CUBE, .pos = {4, 5, 6}}}, o;
    nsp_take(&P, t + 2000, NULL, 0, b, 1);
    nsp_take(&P, t + 2050, NULL, 0, b, 2);
    nsp_take(&P, t + 2100, NULL, 0, b, 2);
    CHECK(P.st.booms == 2 && P.st.boom_dups == 3);
    CHECK(!nsp_boom_due(&P, t + 2000 - 1, &o));
    CHECK(nsp_boom_due(&P, t + 2000, &o) && o.seq == 40 && o.pos[2] == 3 && !nsp_boom_due(&P, t + 2049, &o));
    CHECK(nsp_boom_due(&P, t + 2050, &o) && o.seq == 41 && !nsp_boom_due(&P, t + 9999, &o));
    // thrown: his weapon name kept for its model
    s[0] = shot(9, NSK_THROWN, 0, 0, 0, "Weapon_Spear_001");
    nsp_take(&P, t + 3000, s, 1, NULL, 0);
    CHECK(nsp_track(&P, 9) && !strcmp(nsp_track(&P, 9)->name, "Weapon_Spear_001"));
    // more than NST_TRACKS at once: the rest counted full
    NsPeer Q;
    memset(&Q, 0, sizeof Q);
    for (int i = 0; i < NST_TRACKS + 3; i++) {
        NetShot one = shot((uint16_t)(1 + i), NSK_NORMAL_ARROW, (float)i, 0, 0, NULL);
        nsp_take(&Q, t + (uint64_t)i, &one, 1, NULL, 0);  // (each packet lists only the newest: the older ones end and give way)
    }
    CHECK(Q.st.full == 0 && Q.st.new_tracks == NST_TRACKS + 3);
}

static void test_due(void) {
    CHECK(!nshot_due(false, false, 0, 1000, 0));
    CHECK(nshot_due(true, true, 0, 1000, 0) && nshot_due(true, true, 0, 1050, 1000) && !nshot_due(true, true, 0, 1049, 1000));
    CHECK(!nshot_due(true, false, 0, 1200, 1000) && nshot_due(true, false, 0, 1250, 1000));
    CHECK(nshot_due(false, false, 2, 1050, 1000) && !nshot_due(false, false, 2, 1020, 1000));
    CHECK(nshot_due(true, true, 0, 900, 1000));  // (a clock that went back)
}

// ================================================================ part 2: the glue on stubs
static void slot_use(int k, const char *name, uint64_t joined) {
    memset(&NET.slot[k], 0, sizeof NET.slot[k]);
    NET.slot[k].used = true, NET.slot[k].joined_ms = joined;
    snprintf(NET.slot[k].p.name, sizeof NET.slot[k].p.name, "%s", name);
}
static void test_glue(void) {
    memset(&NET, 0, sizeof NET), memset(&NETS, 0, sizeof NETS);
    NET.on = true, NET.me = 5;
    g_ndest = 0, g_nsent = 0;
    uint8_t b[NET_SHOTS_PKT];
    NetShot s[2] = {shot(3, NSK_NORMAL_ARROW, 1, 2, 3, NULL), shot(4, NSK_THROWN, 4, 5, 6, "Weapon_Sword_001")};
    NetBoom bm = {.seq = 9, .kind = NSK_REMOTE_BOMB, .pos = {7, 8, 9}};
    // keys 0 ("set netshots 0" / "set netrunes 0"; the default before s290): nothing sent, a friend's packet dropped and counted,
    // nobody holds anything
    log_clear();
    CHECK(net_shots_live_set("netshots", 0) && LOGGED("[net] shots off (set netshots 0") && net_shots_live_set("netrunes", 0) &&
          LOGGED("[net] runes off (set netrunes 0"));
    CHECK(g_net_shots == 0 && g_net_runes == 0 && g_net_shots_log == 0 && g_net_shots_trace == 0 && g_net_shots_max == 6 && g_net_shots_boom == 200);
    slot_use(0, "Kairi", 5000);
    g_ndest = 1;
    for (int i = 0; i < 100; i++) g_ms += 16, net_shots_local(s, 2, &bm, 1, true, g_ms), net_shots_tick(g_ms);
    CHECK(g_nsent == 0);
    int n = net_pack_shots(7, 1, s, 2, &bm, 1, b, (int)sizeof b);
    net_shots_recv(0, NET_SHOTS, b, n, g_ms);
    CHECK(NETS.st.rx_off == 1 && NETS.st.rx == 0 && !NETS.peer[0].p.tr[0].used);
    NetRunes r;
    memset(&r, 0, sizeof r);
    r.flags = NRF_MAG, r.mag.hashid = 0xabc, r.mag.q[3] = 1;
    n = net_pack_runes(7, 1, &r, b, (int)sizeof b);
    net_shots_recv(0, NET_RUNES, b, n, g_ms);
    CHECK(NETS.st.rrx_off == 1 && netshots_friend_holds(0xabc) == 0);
    // keys on: one line each; a live rewrite (the same value) = no line
    log_clear();
    CHECK(net_shots_live_set("netshots", 1) && g_net_shots == 1 && LOGGED("[net] shots on"));
    int nl = g_nlog;
    CHECK(net_shots_live_set("netshots", 1) && g_nlog == nl);
    CHECK(net_shots_live_set("netshotslog", 1) && LOGGED("shots log on") && net_shots_live_set("netrunes", 1) && LOGGED("[net] runes on"));
    CHECK(net_shots_live_set("netshotsmax", 99) && g_net_shots_max == 16 && net_shots_live_set("netshotsmax", 6) && g_net_shots_max == 6);
    CHECK(net_shots_live_set("netshotsboom", -5) && g_net_shots_boom == 0 && LOGGED("no boom effect") && net_shots_live_set("netshotsboom", 200) && g_net_shots_boom == 200);
    CHECK(!net_shots_live_set("netshotsx", 1) && !net_shots_live_set("netgear", 1) && !net_shots_live_set("net", 1));
    // the lab trace: its own key (13 letters: live keys over 15 are never read), one line each way
    nl = g_nlog;
    CHECK(strlen("netshotstrace") <= 15 && net_shots_live_set("netshotstrace", 1) && g_net_shots_trace == 1 && LOGGED("[net] shots trace on") && g_nlog == nl + 1);
    nl = g_nlog;
    CHECK(net_shots_live_set("netshotstrace", 1) && g_nlog == nl && net_shots_live_set("netshotstrace", 0) && g_net_shots_trace == 0 &&
          LOGGED("[net] shots trace off") && g_nlog == nl + 1);
    // moving shots: never two of a kind within 50 ms (so at most one NET_SHOTS + one NET_RUNES in any 50 ms room tick = 2 of the
    // 16 lanes); on 16 ms frames that is one per 4th frame (64 ms, ~16 Hz): 31 or 32 in 2 s
    g_nsent = 0, memset(g_nsent_type, 0, sizeof g_nsent_type);
    uint64_t t0 = g_ms, last = 0, rlast = 0, gap = 9999, rgap = 9999;
    for (int i = 0; i < 125; i++) {
        g_ms += 16;
        net_shots_local(s, 2, NULL, 0, true, g_ms);
        net_runes_local(&r, g_ms);
        int before = g_nsent_type[NET_SHOTS], rbefore = g_nsent_type[NET_RUNES];
        net_shots_tick(g_ms);
        CHECK(g_nsent_type[NET_SHOTS] - before <= 1 && g_nsent_type[NET_RUNES] - rbefore <= 1);
        if (g_nsent_type[NET_SHOTS] != before) {
            if (last && g_ms - last < gap) gap = g_ms - last;
            last = g_ms;
        }
        if (g_nsent_type[NET_RUNES] != rbefore) {
            if (rlast && g_ms - rlast < rgap) rgap = g_ms - rlast;
            rlast = g_ms;
        }
    }
    CHECK(gap >= NSHOT_MS && rgap >= NRUNE_MS && gap <= 64 && rgap <= 64);
    CHECK(g_nsent_type[NET_SHOTS] >= 31 && g_nsent_type[NET_SHOTS] <= 32 && g_nsent_type[NET_RUNES] >= 31 && g_nsent_type[NET_RUNES] <= 32);
    CHECK(g_ms - t0 == 2000 && g_last_type[NET_SHOTS][3] == NET_SHOTS && g_last_type[NET_SHOTS][NET_HDR] == 2 && g_last_type[NET_RUNES][NET_HDR] == NRF_MAG);
    {  // (30 fps frames: every 2nd one, 66 ms)
        int a = g_nsent_type[NET_SHOTS];
        uint64_t l2 = 0, g2 = 9999;
        for (int i = 0; i < 60; i++) {
            g_ms += 33;
            net_shots_local(s, 2, NULL, 0, true, g_ms);
            int before = g_nsent_type[NET_SHOTS];
            net_shots_tick(g_ms);
            if (g_nsent_type[NET_SHOTS] != before) {
                if (l2 && g_ms - l2 < g2) g2 = g_ms - l2;
                l2 = g_ms;
            }
        }
        CHECK(g2 >= NSHOT_MS && g_nsent_type[NET_SHOTS] - a >= 29 && g_nsent_type[NET_SHOTS] - a <= 31);
    }
    // all still: 4 Hz
    int sent = g_nsent_type[NET_SHOTS];
    for (int i = 0; i < 125; i++) g_ms += 16, net_shots_local(s, 2, NULL, 0, false, g_ms), net_shots_tick(g_ms);
    CHECK(g_nsent_type[NET_SHOTS] - sent >= 7 && g_nsent_type[NET_SHOTS] - sent <= 9);
    // a boom due: 20 Hz again, and its line once
    log_clear(), sent = g_nsent_type[NET_SHOTS];
    for (int i = 0; i < 31; i++) g_ms += 16, net_shots_local(s, 2, &bm, 1, false, g_ms), net_shots_tick(g_ms);
    CHECK(g_nsent_type[NET_SHOTS] - sent >= 7 && LOGGED("our RemoteBomb went off at 7.0 8.0 9.0: boom #9 sent (UDP)"));
    char *first = strstr(g_log, "boom #9 sent");
    CHECK(first && !strstr(first + 1, "boom #9 sent"));
    // nothing any more: NSHOT_EMPTY empty ones, then quiet
    sent = g_nsent_type[NET_SHOTS];
    for (int i = 0; i < 200; i++) g_ms += 16, net_shots_local(NULL, 0, NULL, 0, false, g_ms), net_shots_tick(g_ms);
    CHECK(g_nsent_type[NET_SHOTS] - sent == NSHOT_EMPTY && g_last_type[NET_SHOTS][NET_HDR] == 0 && g_last_type[NET_SHOTS][NET_HDR + 1] == 0);
    // the game side stopped filling (a load, Sora off): the old list is not sent after NSHOT_LOCAL_MS
    net_shots_local(s, 2, NULL, 0, true, g_ms);
    sent = g_nsent_type[NET_SHOTS];
    for (int i = 0; i < 100; i++) g_ms += 16, net_shots_tick(g_ms);
    CHECK(g_nsent_type[NET_SHOTS] - sent <= NSHOT_LOCAL_MS / NSHOT_MS + 1 + NSHOT_EMPTY && g_last_type[NET_SHOTS][NET_HDR] == 0);
    // alone: nothing
    g_ndest = 0;
    sent = g_nsent;
    for (int i = 0; i < 50; i++) g_ms += 16, net_shots_local(s, 2, NULL, 0, true, g_ms), net_runes_local(&r, g_ms), net_shots_tick(g_ms);
    CHECK(g_nsent == sent);
    // a Melty room: nothing while it is down; up: the slots say who is there
    NET.melty = true, g_room_up = false;
    for (int i = 0; i < 20; i++) g_ms += 16, net_shots_local(s, 2, NULL, 0, true, g_ms), net_shots_tick(g_ms);
    CHECK(g_nsent == sent);
    g_room_up = true;
    for (int i = 0; i < 20; i++) g_ms += 16, net_shots_local(s, 2, NULL, 0, true, g_ms), net_shots_tick(g_ms);
    CHECK(g_nsent > sent);
    NET.melty = false, g_ndest = 1;
    // the receive: fresh ones onto slot 0's tracks, a late one dropped, a bad one counted
    log_clear();
    n = net_pack_shots(7, 100, s, 2, &bm, 1, b, (int)sizeof b);
    net_shots_recv(0, NET_SHOTS, b, n, g_ms);
    CHECK(NETS.st.rx == 1 && nsp_track(&NETS.peer[0].p, 3) && nsp_track(&NETS.peer[0].p, 4) && NETS.peer[0].p.st.booms == 1);
    CHECK(LOGGED("[net] shots: peer 1's NormalArrow #3") && LOGGED("peer 1's Weapon_Sword_001 #4") && LOGGED("peer 1's bomb went off"));
    n = net_pack_shots(7, 99, s, 2, NULL, 0, b, (int)sizeof b);
    net_shots_recv(0, NET_SHOTS, b, n, g_ms);
    CHECK(NETS.st.rx_dup == 1);
    b[NET_HDR] = 9;
    net_shots_recv(0, NET_SHOTS, b, n, g_ms);
    CHECK(NETS.st.rx_bad == 1);
    // the slot is someone else now: his record starts empty
    slot_use(0, "Riku", 9000);
    n = net_pack_shots(8, 5, NULL, 0, NULL, 0, b, (int)sizeof b);
    net_shots_recv(0, NET_SHOTS, b, n, g_ms);
    CHECK(NETS.st.rx == 2 && !nsp_track(&NETS.peer[0].p, 3));
    // runes: what he holds, for the objs helper; forgotten when quiet; netrunes 0 = nobody
    r.flags = NRF_MAG | NRF_STOP, r.stop.hashid = 0x77, r.stop.q[3] = 1;
    n = net_pack_runes(8, 50, &r, b, (int)sizeof b);
    net_shots_recv(0, NET_RUNES, b, n, g_ms);
    CHECK(netshots_friend_holds(0xabc) == 1 && netshots_friend_holds(0x77) == 1 && netshots_friend_holds(0x78) == 0 && netshots_friend_holds(0) == 0);
    g_ms += NRUNE_FORGET_MS + 1;
    CHECK(netshots_friend_holds(0xabc) == 0);
    n = net_pack_runes(8, 51, &r, b, (int)sizeof b);
    net_shots_recv(0, NET_RUNES, b, n, g_ms);
    CHECK(netshots_friend_holds(0xabc) == 1);
    net_shots_live_set("netrunes", 0);
    CHECK(netshots_friend_holds(0xabc) == 0);
    net_shots_live_set("netrunes", 1);
    // loopback: our shots -> pack -> unpack -> slot 0, nothing sent
    memset(&NET, 0, sizeof NET);
    NET.on = true, NET.me = 5, g_net_loop = true;
    net_shots_reset(g_ms);
    sent = g_nsent;
    slot_use(0, "", g_ms);
    g_ms += 16, net_shots_local(s, 2, NULL, 0, true, g_ms), net_shots_tick(g_ms);
    CHECK(g_nsent == sent && nsp_track(&NETS.peer[0].p, 3) && nsp_track(&NETS.peer[0].p, 4));
    g_net_loop = false;
    // the diag line
    log_clear();
    net_shots_diag();
    CHECK(LOGGED("[net] shots on, runes on") && LOGGED("slot 1"));
    net_shots_live_set("netshots", 0), net_shots_live_set("netshotslog", 0), net_shots_live_set("netrunes", 0);
    CHECK(g_net_shots == 0 && NETS.nloc == 0 && !NETS.peer[0].p.tr[0].used);
}

// ================================================================ s290 arrow: netshotsstuck (a stuck arrow stays stuck for friends)
// the lab (s289q): copy2's IceArrow hit 4.13 m from his feet; its watch failed at the hit while it stayed stuck there, and the walk
// found the same actor (address + id) again. Here: the re-watch on fake guest memory, the id match, the walk's units, the hold,
// the lost grace + the walk finding it, the game's removal, the sender's cap, the receiver's hold / cap / late end, key 0 = s289.
static void test_stuck_rewatch(void) {
    fake_world();
    uint64_t a = g_act[20], old = g_unit[20], nu = g_unit[30];
    NsWatch w;
    CHECK(nshot_watch(fake_rd, a, &w) && w.unit == old && w.id == 1020 && nshot_alive(fake_rd, a, w) && nshot_same(fake_rd, a, 1020));
    CHECK(!nshot_same(fake_rd, a, 1021) && !nshot_same(fake_rd, 0, 1020));
    NsWatch w0 = w;
    CHECK(nshot_rewatch(fake_rd, a, &w) && w.unit == old);  // (its unit still holds it: the same watch)
    // the game moves it to unit 30 (a stuck arrow on a new proc unit [GUESS]): the old watch fails, the re-watch follows it
    w32(old + 0x40, 7777), w64(old + 0x48, g_act[5]);  // (the old unit reused by another actor)
    w32(nu + 0x40, 1020), w64(nu + 0x48, a), w64(a + 0x68, nu);
    CHECK(!nshot_alive(fake_rd, a, w0) && nshot_rewatch(fake_rd, a, &w) && w.unit == nu && w.id == 1020 && nshot_alive(fake_rd, a, w));
    // never: another id at +0x60 (the memory is a new actor now), being deleted (state 3), a new unit that does not hold it
    // (its +0x40 / +0x48), no unit, an unreadable one
    w = w0;
    w32(a + 0x60, 1099);
    CHECK(!nshot_rewatch(fake_rd, a, &w) && w.unit == old && !nshot_same(fake_rd, a, 1020));
    w32(a + 0x60, 1020), w8(a + 0x64, 3);
    CHECK(!nshot_rewatch(fake_rd, a, &w) && w.unit == old && !nshot_same(fake_rd, a, 1020));
    w8(a + 0x64, 1), w32(nu + 0x40, 1021);
    CHECK(!nshot_rewatch(fake_rd, a, &w) && w.unit == old && nshot_same(fake_rd, a, 1020));  // (lost: the walk may find it)
    w32(nu + 0x40, 1020), w64(nu + 0x48, g_act[6]);
    CHECK(!nshot_rewatch(fake_rd, a, &w) && w.unit == old);
    w64(nu + 0x48, a), w64(a + 0x68, 0);
    CHECK(!nshot_rewatch(fake_rd, a, &w) && w.unit == old);
    w64(a + 0x68, (uintptr_t)g_arena + sizeof g_arena - 8);
    CHECK(!nshot_rewatch(fake_rd, a, &w) && w.unit == old);
    // the walk's units: each actor with the unit it came through (nsl_found's input); nshot_walk = the same actors
    fake_world();
    uint64_t acts[64], units[64], acts2[64];
    int n = nshot_walk_u(fake_rd, g_unit[10], acts, units, 64), n2 = nshot_walk(fake_rd, g_unit[10], acts2, 64);
    bool same = n == n2 && n == N_UNITS;
    for (int i = 0; i < n && same; i++) same = acts[i] == acts2[i] && *(uint64_t *)(uintptr_t)(units[i] + 0x48) == acts[i];
    CHECK(same);
    // the lab's case if its +0x68 lags: the walk finds it on a unit its +0x68 does not name: the unit it came through
    w32(g_unit[25] + 0x40, 7000), w64(g_unit[25] + 0x48, g_act[1]);  // (unit 25 holds another actor now)
    w32(g_unit[3] + 0x40, 1025), w64(g_unit[3] + 0x48, g_act[25]);   // (actor 25 is on unit 3 now; its +0x68 still names unit 25)
    w32(g_act[3] + 0x60, 6003);
    n = nshot_walk_u(fake_rd, g_unit[10], acts, units, 64);
    int at = -1;
    for (int i = 0; i < n; i++)
        if (acts[i] == g_act[25]) at = i;
    CHECK(at >= 0 && units[at] == g_unit[3] && *(uint64_t *)(uintptr_t)(g_act[25] + 0x68) == g_unit[25]);
    NsWatch wl = {g_unit[25], 1025};
    CHECK(!nshot_alive(fake_rd, g_act[25], wl) && !nshot_rewatch(fake_rd, g_act[25], &wl) && nshot_same(fake_rd, g_act[25], 1025));
    wl.unit = units[at];
    CHECK(nshot_alive(fake_rd, g_act[25], wl));  // (what nsl_found sets: the next look holds)
}

// his shot arrow: on his bow, off it, flies; it hits 4 m out (the lab: 4.13 m) and lies still from then on. -> its track
static int stuck_shot(NsLocal *L, uint64_t *t, float feet[3], float m[12], uint64_t actor, NsWatch w) {
    mtx_at(m, feet[0] + 0.5f, feet[1] + 1.9f, feet[2] + 0.6f);
    nsl_offer(L, *t, actor, w, NSK_ICE_ARROW, m, feet, true, true, NULL);
    int k = nsl_find(L, actor, w.id);
    for (int i = 0; i < 10; i++) *t += 33, nsl_update(L, k, *t, true, m, false, feet, true, true);
    *t += 33, mtx_at(m, feet[0] + 2.0f, feet[1] + 1.9f, feet[2] + 0.6f), nsl_update(L, k, *t, true, m, false, feet, true, true);
    *t += 33, mtx_at(m, feet[0] + 4.0f, feet[1] + 1.0f, feet[2] + 0.6f), nsl_update(L, k, *t, true, m, false, feet, true, false);
    return k;
}
static void test_stuck_local(void) {
    float feet[3] = {-1113.0f, 237.3f, 1887.0f}, m[12];
    NetShot s[NSHOT_MAX];
    bool mv = false;
    // 1. key 0 (nsl_init: stuck_ms 0) = s289: dropped 3 s after it lies, remembered; no capped / stuck_gone counted
    {
        NsLocal L;
        nsl_init(&L, 900);
        uint64_t t = 10000;
        NsWatch w = {0x6000, 11};
        int k = stuck_shot(&L, &t, feet, m, 0xb1, w);
        CHECK(k >= 0 && L.tr[k].flew && L.stuck_ms == 0);
        int looks = 0;
        while (L.tr[k].actor && looks < 1000) t += 33, looks++, nsl_update(&L, k, t, true, m, false, feet, false, false);
        CHECK(!L.tr[k].actor && L.end == NSE_STILL && L.st.still == 1 && L.st.capped == 0 && looks * 33 >= NSHOT_STILL_MS && looks * 33 < NSHOT_STILL_MS + 100);
        CHECK(nsl_rejected(&L, 0xb1, 11));
    }
    // 2. netshotsstuck 65: kept + listed (NSF_STILL) while it lies, 20 s; then the game removes it: gone, counted stuck_gone, its
    // still time said; not remembered (its memory may hold a new actor: a new id anyway)
    {
        NsLocal L;
        nsl_init(&L, 900);
        L.stuck_ms = NSHOT_STUCK_S * 1000;
        uint64_t t = 10000;
        NsWatch w = {0x6000, 12};
        int k = stuck_shot(&L, &t, feet, m, 0xb2, w);
        bool kept = true, listed = true;
        for (int i = 0; i < 600; i++) {  // (20 s at 30 looks a second)
            t += 33;
            kept = kept && nsl_update(&L, k, t, true, m, false, feet, false, false);
            listed = listed && nsl_list(&L, t, s, &mv) == 1 && s[0].id == L.tr[k].id;
        }
        CHECK(kept && listed && (s[0].flags & NSF_STILL) && !mv && L.st.still == 0);
        uint64_t still = t - L.tr[k].still_since;
        CHECK(!nsl_update(&L, k, t + 33, false, m, false, feet, false, false) && L.end == NSE_GONE && L.st.stuck_gone == 1 &&
              L.end_still_ms == (uint32_t)(still + 33) && !nsl_rejected(&L, 0xb2, 12) && nsl_list(&L, t + 33, s, &mv) == 0);
    }
    // 3. the cap: never removed (a stale read [GUESS]): let go at exactly stuck_ms still (NSE_CAP), remembered (the walk never takes
    // it again); not before
    {
        NsLocal L;
        nsl_init(&L, 900);
        L.stuck_ms = 10 * 1000;
        uint64_t t = 10000;
        NsWatch w = {0x6000, 13};
        int k = stuck_shot(&L, &t, feet, m, 0xb3, w);
        t += 33, nsl_update(&L, k, t, true, m, false, feet, false, false);  // (the first still look)
        uint64_t since = L.tr[k].still_since;
        CHECK(since == t);
        bool kept = true;
        while (t + 33 < since + 10000) t += 33, kept = kept && nsl_update(&L, k, t, true, m, false, feet, false, false);
        CHECK(kept && L.tr[k].actor);
        t = since + 10000;
        CHECK(!nsl_update(&L, k, t, true, m, false, feet, false, false) && L.end == NSE_CAP && L.st.capped == 1 && L.end_still_ms == 10000 && nsl_rejected(&L, 0xb3, 13));
        nsl_bowing(&L, t, true);
        CHECK(nsl_offer(&L, t, 0xb3, w, NSK_ICE_ARROW, m, feet, true, false, NULL) == NSO_NOT_HIS);
        // the hold is for arrows that flew only: one that only waited in his hands and lies (dropped) goes after 3 s, as before
        NsWatch w2 = {0x6100, 14};
        mtx_at(m, feet[0] + 0.5f, feet[1] + 1.0f, feet[2]);
        CHECK(nsl_offer(&L, t, 0xb4, w2, NSK_NORMAL_ARROW, m, feet, true, false, NULL) == NSO_TAKEN);
        int k2 = nsl_find(&L, 0xb4, 14);
        t += 33, mtx_at(m, feet[0] + 0.6f, feet[1] + 0.2f, feet[2]), nsl_update(&L, k2, t, true, m, false, feet, true, false);
        int looks = 0;
        while (L.tr[k2].actor && looks < 1000) t += 33, looks++, nsl_update(&L, k2, t, true, m, false, feet, true, false);
        CHECK(!L.tr[k2].flew && L.end == NSE_STILL && looks * 33 < NSHOT_STILL_MS + 100);
        // and his thrown weapon: unchanged (still 3 s: it goes, as s289)
        NsWatch w3 = {0x6200, 15};
        mtx_at(m, feet[0] + 1, feet[1], feet[2]);
        CHECK(nsl_offer(&L, t, 0xb5, w3, NSK_THROWN, m, feet, false, false, "Weapon_Sword_001") == NSO_TAKEN);
        int k3 = nsl_find(&L, 0xb5, 15);
        t += 33, mtx_at(m, feet[0] + 3, feet[1], feet[2]), nsl_update(&L, k3, t, true, m, false, feet, false, false);
        looks = 0;
        while (L.tr[k3].actor && looks < 1000) t += 33, looks++, nsl_update(&L, k3, t, true, m, false, feet, false, false);
        CHECK(L.end == NSE_STILL && looks * 33 < NSHOT_STILL_MS + 100);
    }
    // 4. lost: its watch + re-watch failed, its id still there: kept NSHOT_LOST_MS at its last pose and sent; the walk finds it
    // (the id must match) and it is watched on that unit; not found: gone after the grace
    {
        NsLocal L;
        nsl_init(&L, 900);
        L.stuck_ms = NSHOT_STUCK_S * 1000;
        uint64_t t = 10000;
        NsWatch w = {0x6000, 16};
        int k = stuck_shot(&L, &t, feet, m, 0xb6, w);
        float px = L.tr[k].m[3];
        t += 33;
        CHECK(nsl_lose(&L, k, t) && L.tr[k].lost_since == t && L.st.lost == 1 && nsl_list(&L, t, s, &mv) == 1 && s[0].pos[0] == px);
        CHECK(nsl_lose(&L, k, t + NSHOT_LOST_MS - 1) && L.st.lost == 1);
        CHECK(nsl_found(&L, 0xb6, 17, 0x7000) < 0 && L.tr[k].w.unit == 0x6000);  // (another id: not it)
        CHECK(nsl_found(&L, 0xb7, 16, 0x7000) < 0 && nsl_found(&L, 0xb6, 16, 0) < 0);
        CHECK(nsl_found(&L, 0xb6, 16, 0x7000) == k && L.tr[k].w.unit == 0x7000 && L.tr[k].w.id == 16 && L.tr[k].lost_since == t && L.st.found == 1);
        CHECK(nsl_found(&L, 0xb6, 16, 0x7000) < 0 && L.st.found == 1);  // (the same unit again: nothing new)
        // found but its next look fails too (an unreadable matrix): the lost time is not started again: gone when it is over
        {
            NsLocal L2 = L;
            CHECK(nsl_lose(&L2, k, t + NSHOT_LOST_MS - 1) && !nsl_lose(&L2, k, t + NSHOT_LOST_MS) && L2.end == NSE_GONE);
        }
        t += NSHOT_LOST_MS;
        CHECK(nsl_update(&L, k, t, true, m, false, feet, false, false) && !L.tr[k].lost_since);  // (the look holds: lost no more)
        CHECK(nsl_found(&L, 0xb6, 16, 0x7100) < 0 && L.tr[k].w.unit == 0x7000);  // (only a lost one is re-watched)
        // lost again, not found: gone once the grace is over, as nsl_update's not alive (not remembered)
        CHECK(nsl_lose(&L, k, t + 33) && !nsl_lose(&L, k, t + 33 + NSHOT_LOST_MS) && L.end == NSE_GONE && !L.tr[k].actor && L.st.gone == 1 &&
              L.st.stuck_gone == 1 && !nsl_rejected(&L, 0xb6, 16));
        CHECK(!nsl_lose(&L, k, t + 5000));  // (an empty track)
        // the walk offering a lost one before it re-watches: still known (its id), never "not ours"
        k = stuck_shot(&L, &t, feet, m, 0xb8, (NsWatch){0x6300, 18});
        CHECK(nsl_lose(&L, k, t + 33));
        nsl_bowing(&L, t + 33, true);
        mtx_at(m, feet[0] + 4.13f, feet[1], feet[2]);
        CHECK(nsl_offer(&L, t + 33, 0xb8, (NsWatch){0x6400, 18}, NSK_ICE_ARROW, m, feet, true, false, NULL) == NSO_KNOWN);
    }
}

static void test_stuck_remote(void) {
    // his arrow flies, sticks (NSF_STILL) and his game keeps it 50 s (a packet every 250 ms): held here at its place all the while,
    // no cap (65 + 5 s); then a packet without it (the late end): ended, shown until its last place has played, then freed
    NsPeer P;
    memset(&P, 0, sizeof P);
    uint64_t t = 200000;
    float p[3], q[4];
    NetShot s = shot(21, NSK_ICE_ARROW, 0, 10, 0, NULL);
    for (int i = 0; i < 10; i++) s.pos[0] = (float)i * 5, nsp_take(&P, t, &s, 1, NULL, 0), t += 66;
    NstTrack *tr = nsp_track(&P, 21);
    CHECK(tr && !tr->still_at);
    s.flags = NSF_STILL;
    uint64_t t_still = t;
    bool held = true;
    uint16_t ids[4];
    int capped = 0;
    for (int i = 0; i < 200; i++, t += 250) {
        nsp_take(&P, t, &s, 1, NULL, 0);
        capped += nsp_cap(&P, t, (uint64_t)NSHOT_STUCK_S * 1000 + NSHOT_STUCK_SLACK_MS, ids, 4);
        nsp_reap(&P, t, t - NET_INTERP_MS);
        held = held && tr->used && !tr->ended && nsp_sample(tr, t - NET_INTERP_MS, p, q) && (i == 0 || near(p[0], 45, 1e-4f));  // (i 0: its last flight)
    }
    CHECK(held && capped == 0 && tr->still_at == t_still && P.st.ended == 0);
    nsp_take(&P, t, NULL, 0, NULL, 0);
    CHECK(tr->ended && P.st.ended == 1 && nsp_reap(&P, t, t - NET_INTERP_MS) == 0 && !tr->used);
    // moving again (stuck in a Bokoblin that walks): its still time starts again
    memset(&P, 0, sizeof P);
    s = shot(22, NSK_FIRE_ARROW, 0, 0, 0, NULL), s.flags = NSF_STILL;
    nsp_take(&P, t, &s, 1, NULL, 0);
    tr = nsp_track(&P, 22);
    CHECK(tr->still_at == t);
    s.flags = 0, nsp_take(&P, t + 250, &s, 1, NULL, 0);
    CHECK(tr->still_at == 0);
    s.flags = NSF_STILL, nsp_take(&P, t + 500, &s, 1, NULL, 0);
    CHECK(tr->still_at == t + 500);
    // the receiver's cap: his packets keep listing it still past cap_ms (a stale read there): ended here at exactly cap_ms, freed,
    // and never shown again while he lists it; cap 0 = none; thrown weapons / bombs never capped
    uint64_t t0 = t + 500, cap = 70000;
    bool early = false;
    for (uint64_t u = t0 + 250; u < t0 + cap; u += 250) {
        nsp_take(&P, u, &s, 1, NULL, 0);
        early = early || nsp_cap(&P, u, 0, ids, 4) != 0 || nsp_cap(&P, u, cap, ids, 4) != 0;
    }
    CHECK(!early && !tr->ended && nsp_cap(&P, t0 + cap, cap, ids, 4) == 1 && ids[0] == 22 && tr->ended && P.st.capped == 1 && P.capped_id[0] == 22);
    CHECK(nsp_reap(&P, t0 + cap, t0 + cap - NET_INTERP_MS) == 0 && !tr->used);
    uint32_t nt = P.st.new_tracks;
    for (int i = 1; i <= 8; i++) nsp_take(&P, t0 + cap + 250u * (uint64_t)i, &s, 1, NULL, 0);
    CHECK(!nsp_track(&P, 22) && P.st.new_tracks == nt);  // (hidden for good)
    NetShot o = shot(23, NSK_ICE_ARROW, 5, 0, 0, NULL);  // (his next arrow: shown as ever)
    nsp_take(&P, t0 + cap + 3000, &o, 1, NULL, 0);
    CHECK(nsp_track(&P, 23) && P.st.new_tracks == nt + 1);
    NsPeer Q;
    memset(&Q, 0, sizeof Q);
    NetShot wpn = shot(30, NSK_THROWN, 0, 0, 0, "Weapon_Sword_001"), bomb = shot(31, NSK_REMOTE_BOMB, 0, 0, 0, NULL);
    wpn.flags = bomb.flags = NSF_STILL;
    NetShot two[2] = {wpn, bomb};
    nsp_take(&Q, 1000, two, 2, NULL, 0);
    nsp_take(&Q, 1000 + cap, two, 2, NULL, 0);
    CHECK(nsp_cap(&Q, 1000 + cap + 1, cap, ids, 4) == 0 && Q.st.capped == 0);
}

static void test_stuck_glue(void) {
    memset(&NET, 0, sizeof NET), memset(&NETS, 0, sizeof NETS);
    NET.on = true, NET.me = 5;
    slot_use(1, "Riku", 7000);
    uint8_t b[NET_SHOTS_PKT];
    // the key: default 600 since s291 (was 65; with netshots 1), 0..600, one line per change
    CHECK(g_net_shots_stuck == NSHOT_STUCK_S && NSHOT_STUCK_S == 600 && strlen("netshotsstuck") <= 15);
    log_clear();
    CHECK(net_shots_live_set("netshotsstuck", 600) && g_nlog == 0);  // (the default again: no line)
    CHECK(net_shots_live_set("netshotsstuck", 65) && g_net_shots_stuck == 65 && g_nlog == 1);
    log_clear();
    CHECK(net_shots_live_set("netshotsstuck", 9999) && g_net_shots_stuck == 600 && LOGGED("at most 600 s still") && g_nlog == 1);
    CHECK(net_shots_live_set("netshotsstuck", -3) && g_net_shots_stuck == 0 && LOGGED("netshotsstuck 0: a stuck arrow goes at its hit"));
    net_shots_live_set("netshotsstuck", 65), net_shots_live_set("netshots", 1), net_shots_live_set("netshotslog", 1);
    // his arrow flies, lies still: one "lies still here" line; his game removes it after 20 s: "gone after 20.0 s lying still"
    uint16_t seq = 100;
    NetShot s = shot(40, NSK_ICE_ARROW, -1108.9f, 237.3f, 1887.0f, NULL);
    log_clear();
    for (int i = 0; i < 5; i++) {
        g_ms += 66, s.pos[0] += 1;
        int n = net_pack_shots(2, ++seq, &s, 1, NULL, 0, b, (int)sizeof b);
        net_shots_take(1, b, n, g_ms);
    }
    CHECK(LOGGED("peer 2's IceArrow #40 at") && !LOGGED("lies still"));
    s.flags = NSF_STILL;
    uint64_t t_still = g_ms + 250;
    for (int i = 0; i < 80; i++) {
        g_ms += 250;
        int n = net_pack_shots(2, ++seq, &s, 1, NULL, 0, b, (int)sizeof b);
        net_shots_take(1, b, n, g_ms);
        net_shots_reap(1, g_ms, g_ms - NET_INTERP_MS);
    }
    CHECK(LOGGED("peer 2's IceArrow #40 lies still here at -1103.9 237.3 1887.0: held until his game removes it"));
    char *one = strstr(g_log, "lies still here");
    CHECK(one && !strstr(one + 1, "lies still here") && nsp_track(&NETS.peer[1].p, 40) && !nsp_track(&NETS.peer[1].p, 40)->ended);
    // his game removes it: the next packet does not list it: ended, one line with its still time
    g_ms += 250;
    int n = net_pack_shots(2, ++seq, NULL, 0, NULL, 0, b, (int)sizeof b);
    net_shots_take(1, b, n, g_ms);
    char want[120];
    snprintf(want, sizeof want, "peer 2's IceArrow #40 gone after %.1f s lying still: his game removed it", (double)(g_ms - t_still) / 1000.0);
    CHECK(LOGGED(want) && (g_ms - t_still) == 20000);
    // a packet that came late (an older seq) still listing it: dropped, it stays gone
    n = net_pack_shots(2, (uint16_t)(seq - 3), &s, 1, NULL, 0, b, (int)sizeof b);
    uint32_t dup = NETS.st.rx_dup;
    net_shots_take(1, b, n, g_ms + 10);
    CHECK(NETS.st.rx_dup == dup + 1 && nsp_track(&NETS.peer[1].p, 40) && nsp_track(&NETS.peer[1].p, 40)->ended);
    CHECK(net_shots_reap(1, g_ms + 200, g_ms + 200 - NET_INTERP_MS) == 0 && !nsp_track(&NETS.peer[1].p, 40));
    // his packets stop with one lying still (he left / crashed): 1 s quiet, said once, hidden
    s.id = 41;
    log_clear();
    for (int i = 0; i < 4; i++) {
        g_ms += 250;
        n = net_pack_shots(2, ++seq, &s, 1, NULL, 0, b, (int)sizeof b);
        net_shots_take(1, b, n, g_ms);
    }
    uint64_t last = g_ms;
    for (int i = 0; i < 60; i++) g_ms += 33, net_shots_reap(1, g_ms, g_ms - NET_INTERP_MS);
    CHECK(LOGGED("peer 2's IceArrow #41: his packets stopped (1000 ms): hidden here") && !nsp_track(&NETS.peer[1].p, 41) && g_ms - last > NSHOT_QUIET_MS);
    // the cap: listed still for 70 s (65 + 5): hidden, one line; never back while listed
    s.id = 42;
    log_clear();
    for (int i = 0; i < 300; i++) {
        g_ms += 250;
        n = net_pack_shots(2, ++seq, &s, 1, NULL, 0, b, (int)sizeof b);
        net_shots_take(1, b, n, g_ms);
        net_shots_reap(1, g_ms, g_ms - NET_INTERP_MS);
    }
    CHECK(LOGGED("peer 2's IceArrow #42 still 70 s with no end from him: hidden here (netshotsstuck 65 + 5 s)") && !nsp_track(&NETS.peer[1].p, 42) &&
          NETS.peer[1].p.st.capped == 1 && !LOGGED("#42 gone after"));
    // netshotsstuck 0 = s289: no cap (nsp_reap only), no new lines, even 90 s still
    net_shots_live_set("netshotsstuck", 0);
    s.id = 43;
    log_clear();
    for (int i = 0; i < 360; i++) {
        g_ms += 250;
        n = net_pack_shots(2, ++seq, &s, 1, NULL, 0, b, (int)sizeof b);
        net_shots_take(1, b, n, g_ms);
        net_shots_reap(1, g_ms, g_ms - NET_INTERP_MS);
    }
    CHECK(nsp_track(&NETS.peer[1].p, 43) && !nsp_track(&NETS.peer[1].p, 43)->ended && NETS.peer[1].p.st.capped == 1 && !LOGGED("lies still") && !LOGGED("hidden"));
    log_clear();
    net_shots_diag();
    CHECK(!LOGGED("lie still here now"));
    net_shots_live_set("netshotsstuck", 65);
    log_clear();
    net_shots_diag();
    CHECK(LOGGED("slot 2: 1 arrow(s) of his lie still here now; hidden by the cap 1 (netshotsstuck 65 s)"));
    net_shots_live_set("netshots", 0), net_shots_live_set("netshotslog", 0);
}

// (s292 coopfix) netshots 0 -> 1 (lab s291 h): the tracks of before the key went 0 are dropped at each change (nsl_restart), so a
// stale one never comes back (#38486 was sent again 21 m from where it stuck); ids / boom seqs go on; the glue calls the hook
static int g_restart_calls;
static void restart_hook(void) { g_restart_calls++; }
static void test_restart(void) {
    float feet[3] = {-1553.0f, 186.7f, 1307.0f}, m[12];
    NetShot s[NSHOT_MAX];
    NetBoom bo[NSHOT_BOOMS];
    bool mv = false;
    NsLocal L;
    nsl_init(&L, 38480);
    L.stuck_ms = NSHOT_STUCK_S * 1000;
    uint64_t t = 50000;
    NsWatch wa = {0x6100, 21}, wb = {0x6200, 22}, wc = {0x6300, 23}, wd = {0x6400, 24};
    int ka = stuck_shot(&L, &t, feet, m, 0xa1, wa);  // (an arrow that flew and lies stuck: kept, sent)
    for (int i = 0; i < 30; i++) t += 33, nsl_update(&L, ka, t, true, m, false, feet, false, false);
    mtx_at(m, feet[0] + 1, feet[1], feet[2]);
    CHECK(nsl_offer(&L, t, 0xb1, wb, NSK_REMOTE_BOMB, m, feet, false, false, NULL) == NSO_TAKEN);
    int kb = nsl_find(&L, 0xb1, 22);
    CHECK(nsl_update(&L, kb, t + 33, true, m, true, feet, false, false));  // (it went off: a boom due)
    CHECK(nsl_offer(&L, t, 0xc1, wc, NSK_THROWN, m, feet, false, false, "Weapon_Sword_001") == NSO_TAKEN);
    uint16_t next = L.next_id, bseq = L.boom_seq;
    CHECK(ka >= 0 && kb >= 0 && L.tr[ka].flew && nsl_list(&L, t + 33, s, &mv) >= 1 && nsl_booms(&L, t + 33, bo) == 1);
    // the change: every track goes, the arrow remembered (never taken again), the bomb / the weapon not
    CHECK(nsl_restart(&L) == 3);
    bool none = true;
    for (int i = 0; i < NSHOT_MAX; i++) none = none && !L.tr[i].actor;
    CHECK(none && nsl_list(&L, t + 66, s, &mv) == 0 && !mv && nsl_booms(&L, t + 66, bo) == 0);
    CHECK(nsl_rejected(&L, 0xa1, 21) && !nsl_rejected(&L, 0xb1, 22) && !nsl_rejected(&L, 0xc1, 23));
    CHECK(L.next_id == next && L.boom_seq == bseq && L.stuck_ms == NSHOT_STUCK_S * 1000);
    // the old stuck arrow offered again by the walk, near our feet while we use a bow: not ours (remembered); the bomb still out:
    // taken again under a NEW id from where it is now; a new arrow on our bow: taken, the next id
    mtx_at(m, feet[0] + 0.5f, feet[1] + 0.2f, feet[2]);
    CHECK(nsl_offer(&L, t + 99, 0xa1, wa, NSK_NORMAL_ARROW, m, feet, true, false, NULL) == NSO_NOT_HIS && L.why == NSW_REJECTED);
    mtx_at(m, feet[0] + 21, feet[1], feet[2]);
    CHECK(nsl_offer(&L, t + 99, 0xb1, wb, NSK_REMOTE_BOMB, m, feet, false, false, NULL) == NSO_TAKEN);
    kb = nsl_find(&L, 0xb1, 22);
    CHECK(kb >= 0 && L.tr[kb].id == next && near(L.tr[kb].m[3], feet[0] + 21, 1e-4f));
    CHECK(nsl_offer(&L, t + 99, 0xd1, wd, NSK_NORMAL_ARROW, m, feet, true, true, NULL) == NSO_TAKEN && L.tr[nsl_find(&L, 0xd1, 24)].id == (uint16_t)(next + 1));
    CHECK(nsl_restart(&L) == 2 && nsl_restart(&L) == 0);  // (nothing left: 0, no harm)
    // the glue: each change of netshots calls the game side's hook once; the same value again does not
    void (*was)(void) = g_nsg_restart;
    g_nsg_restart = restart_hook, g_restart_calls = 0;
    int keep = g_net_shots;
    net_shots_live_set("netshots", 1);
    int same = g_restart_calls;
    net_shots_live_set("netshots", 0);
    net_shots_live_set("netshots", 0);
    net_shots_live_set("netshots", 1);
    CHECK(same == (keep ? 0 : 1) && g_restart_calls == same + 2 && g_net_shots == 1);
    g_nsg_restart = NULL;
    net_shots_live_set("netshots", 0), net_shots_live_set("netshots", 1);  // (no game side yet: no call, no crash)
    g_nsg_restart = was;
    net_shots_live_set("netshots", keep);
}

// (s290 flip) both compiled defaults are ON (the user: "Flip the passes ON"); the log / trace keys stay off
static void test_default_on(void) {
    CHECK(g_net_shots == 1);
    CHECK(g_net_runes == 1);
    CHECK(g_net_shots_log == 0 && g_net_shots_trace == 0 && g_net_shots_max == 6 && g_net_shots_boom == 200);
}

int main(void) {
    test_default_on();
    test_wire();
    test_rot();
    test_walk();
    test_local();
    test_bow();
    test_remote();
    test_due();
    test_walk_level();
    test_glue();
    test_stuck_rewatch();
    test_stuck_local();
    test_stuck_remote();
    test_stuck_glue();
    test_restart();
    printf("netshots_test: %d checks, %d failed -> %s\n", runs, fails, fails ? "FAIL" : "PASS");
    return fails != 0;
}
