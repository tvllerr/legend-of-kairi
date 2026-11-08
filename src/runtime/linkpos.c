// Recon hook (Sora-in-BotW): Link's world position once per game frame, appended to a text file.
//
//   SAVAGEWIND_LINK_LOG=<path>       turns it on; each line is "frame x y z" (frame: game frames since boot, as
//                                    counted by ksys::VFR::updateInterval; x y z: uking::act::PlayerInfo::mPlayerPos)
//   SAVAGEWIND_LINK_LOG_EVERY=<n>    one line every n game frames (default 1: every frame)
//   SAVAGEWIND_LINK_LOG_FLUSH=<n>    flush the file every n lines (default 60: about once a second at 60 fps)
//
// Called from rt_vfr_update (runtime/hle/framepace.c), which the game's own main thread calls from inside
// ksys::VFR::updateInterval every frame (tools/patches.py _vfr_ratio, 0x70014092f4 in 1.8.2): the read happens on the
// game thread, between two frames' updates, so the three floats are never half-written. The paused game (pause
// menu, map) updates VFR twice a frame; both updates log the same position.
//
// Where the position comes from (1.8.2, main module at 0x7000000000): [0x7001cb2658] is the PlayerInfo singleton
// pointer (sead SINGLETON_DISPOSER sInstance; HEAD's tools/game_sites.py finds the same address by pattern as site
// 'player_info'). zeldaret/botw src/Game/Actor/Player/actPlayerInfo.h (1.5.0 layout, 0x3b0 bytes): vtable 0x0,
// disposer 0x8-0x28, seven empty Info bases 0x28-0x60, PlayerBase *mPlayerActor 0x60, BaseProcLink mPlayerLink 0x68,
// mHorseLink 0x78, sead::Vector3f mPlayerPos 0x88, mPlayerPosForPostCalc 0x94, mMaxHeartValue 0xa0,
// mStaminaCurrentMax 0xa4 - the same offsets Savage Wind's own clock probe uses (framepace.c clock_probe).
//
// Probe (step 2: Link's velocity, input and model in guest memory), same frame, same thread:
//   SAVAGEWIND_LINK_PROBE=<prefix>         <prefix>.txt: one line per game frame with the ksys::act::Actor fields of
//                                          the player actor that zeldaret/botw names (1.5.0 layout, actActor.h):
//                                          mMtx translation (0x398 + 0xc/0x1c/0x2c), mVelocity 0x400, mModel 0x4e0
//                                          and its flag bytes +0xa0..0xa3, the opacity-ish floats 0x4e8..0x4fc,
//                                          mActorFlags2 0x518, mActorFlags 0x520; <prefix>.bin: per frame a header
//                                          (u64 game frame, u64 presented frame, u64 actor, u64 bytes) and then that
//                                          many raw bytes of the actor, for offline diffing against scripted input.
//   SAVAGEWIND_LINK_PROBE_BYTES=<n>        bytes of the actor dumped per frame (default 0x4000; 0 = text only)
//   SAVAGEWIND_LINK_POKE=<from>-<to>:<off>=<f32>[+<off>=<f32>...][;...]
//                                          writes those floats into the player actor (actor + off) every game frame
//                                          while the presented frame is in [from, to), for visibility experiments;
//                                          an offset written c<off> goes into the CharacterController, i<off> into
//                                          the phys::InstanceSet (see below) instead of the actor; b<off> into
//                                          [controller+0x20], r<off> into [InstanceSet+0xf8]; <off>~=<f32> adds
//                                          the value every frame instead of setting it
//   Physics (step 2b: the servo has to go through physics, mVelocity is output only):
//   SAVAGEWIND_LINK_PROBE_PHYS=<0|1>       (default 1 when probing) <prefix>.is.bin / <prefix>.cc.bin: per frame the
//                                          same header as <prefix>.bin (base = the struct's address) and the raw bytes
//                                          of the phys::InstanceSet at [actor+0x578] (0x198, zeldaret physInstanceSet.h)
//                                          and of the CharacterController at [InstanceSet+0x80] (layout unknown: a
//                                          stub in zeldaret; the +0x80 offset itself is unverified on 1.8.2), and
//                                          <prefix>.b.bin / .r.bin: 0x100 bytes of B = [controller+0x20] and 0x300 of
//                                          R = [InstanceSet+0xf8] (addresses logged once, and again if they change)
//   SAVAGEWIND_LINK_PROBE_CC_BYTES=<n>     bytes of the controller dumped per frame (default 0x800)
//   SAVAGEWIND_LINK_FIND=<presented>       at that presented frame, scan all committed guest memory for float triples
//                                          equal (+-0.02) to Link's position (every copy: actor, controller, the Havok
//                                          body/motion...), list them in <prefix>.hits.txt, and from then on dump
//                                          0x60 bytes before to 0x60 after each hit every frame to <prefix>.hits.bin
//                                          (header u64 frame, present, hits, 0xc0; then per hit u64 addr + 0xc0 bytes).
//                                          Pokes take h<k>:<off> for hit k (+off, may be negative) once found.
//
// Servo (step 2d: move Link by a delta; this is what Sora's movement will drive):
//   linkpos_move(dx, dy, dz)               adds a world-space delta (units) that the next rt_vfr_update applies to
//                                          Link's Havok body: the motion state at object = [controller+0x410], head =
//                                          object+0x1a0; head+0x0 (transform translation), +0x10 (centerOfMass0) and
//                                          +0x20 (centerOfMass1, the one the solver integrates from) all get the delta,
//                                          so the next physics step starts from the moved body and still collides
//                                          (ground, walls). Velocity head+0x90 is NOT an input (servo2: overwritten
//                                          every step). y is held by ground contact while he stands on something.
//                                          Pending deltas are dropped while paused (VFR updates twice a frame then).
//   SAVAGEWIND_LINK_SERVO=<from>-<to>:<dx>,<dy>,<dz>[;...]
//                                          test driver: linkpos_move(dx, dy, dz) every game frame while the presented
//                                          frame is in [from, to)
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sora_win.h"  // (<windows.h> on Windows; Linux shims: Deck port)
#include <pthread.h>  // pthread_gethandle (watch: the guest threads' Windows handles)
#ifdef _WIN32
#include <psapi.h>  // QueryWorkingSetEx (K32 in kernel32 with PSAPI_VERSION 2)
#endif

#include "gamelib.h"
#include "hle.h"
#include "sw_api_core.h"  // (s175 idea 1)
#include "game_sites.h"  // (g_game_version)
#include "sora_sites.h"

// The running game version's addresses (sora_sites.h; linkpos_init picks the row). All zeros = not a version we know.
static SoraSites g_ss;
const SoraSites *sora_sites(void) { return &g_ss; }
#define SS (&g_ss)

#define PLAYER_INFO_PTR (SS->player_info)
#define PLAYER_POS 0x88
#define PLAYER_ACTOR 0x60

// ksys::act::Actor (zeldaret/botw actActor.h, 1.5.0; to be confirmed on 1.8.2 by the probe)
#define ACTOR_MTX 0x398
#define ACTOR_VELOCITY 0x400
#define ACTOR_MODEL 0x4e0
#define ACTOR_OPACITY 0x4e8
#define ACTOR_FLAGS2 0x518
#define ACTOR_FLAGS 0x520
#define MODEL_FLAGS 0xa0  // gsys::Model bool _a0 (true), _a1 (false), _a2 (true), _a3 (false)
#define ACTOR_PHYSICS 0x578  // phys::InstanceSet *mPhysics
#define INSTSET_SIZE 0x198
#define INSTSET_CC 0x80  // CharacterController * (from the 0x198-byte layout; unverified on 1.8.2)

static FILE *g_log;
static int g_every, g_flush;
static uint64_t g_frame, g_lines;
static uint64_t g_last_pinfo;

static int env_int(const char *name, int def) {
    const char *e = getenv(name);
    return e && *e ? atoi(e) : def;
}

static bool in_guest(uint64_t a) { return a >= GUEST_LO && a + 16 <= GUEST_HI; }

// How many of the n bytes at a can be read (committed, readable pages), so the dump never faults past the actor's heap.
// (s81 fps hunt) A cache of the guest's 64 KB commit chunks seen committed: VirtualQuery on the 12.5 GB guest
// reservation measures the whole committed run (ms per call), and the enemy-velocity override asked several times per
// enemy per frame (+~20 ms frames). Guest chunks stay committed (only plat_reset_guest decommits; a read of a
// reserved page commits it, platform/windows.c lazy_commit), so a stale bit is still safe to read.
#define RD_CHUNK 12  // (s182: 4 KB pages, was 64 KB chunks: the guest stack TOP page 0x723ffff000 is committed but the rest of its
                     // 64 KB chunk is not -> never cached -> every DLL guest call (swa_invoke checks its stack scratch) paid one
                     // VirtualQuery of the 3 GB run above it = 45 ms; copied enemy swings ran at 13-20 fps. Bitmap = 400 KB.)
static volatile LONG64 g_rd_bits[((GUEST_HI - GUEST_LO) >> RD_CHUNK) / 64];
static bool rd_cached(uint64_t a, size_t n) {
    if (a < GUEST_LO || a + n > GUEST_HI || !n) return false;
    for (uint64_t c = (a - GUEST_LO) >> RD_CHUNK, e = (a + n - 1 - GUEST_LO) >> RD_CHUNK; c <= e; c++)
        if (!(g_rd_bits[c / 64] >> (c % 64) & 1)) return false;
    return true;
}
static void rd_mark(uint64_t lo, uint64_t hi) {  // (the whole pages inside [lo, hi); whole 64-bit words at once for big runs)
    if (lo < GUEST_LO) lo = GUEST_LO;
    if (hi > GUEST_HI) hi = GUEST_HI;
    if (hi <= lo) return;
    uint64_t c = (lo - GUEST_LO + (1ull << RD_CHUNK) - 1) >> RD_CHUNK, e = (hi - GUEST_LO) >> RD_CHUNK;  // pages [c, e)
    while (c < e && c % 64) { InterlockedOr64(&g_rd_bits[c / 64], 1ll << (c % 64)); c++; }
    for (; c + 64 <= e; c += 64)
        if (g_rd_bits[c / 64] != -1ll) InterlockedExchange64(&g_rd_bits[c / 64], -1ll);  // (bits are only ever set: a full word is safe)
    for (; c < e; c++) InterlockedOr64(&g_rd_bits[c / 64], 1ll << (c % 64));
}
static size_t readable(uint64_t a, size_t n) {
    if (rd_cached(a, n)) return n;
    size_t ok = 0;
    while (ok < n) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery((const void *)(uintptr_t)(a + ok), &mbi, sizeof mbi) || mbi.State != MEM_COMMIT ||
            (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) || !(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE |
            PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
            break;
        uint64_t end = (uint64_t)(uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)) rd_mark((uint64_t)(uintptr_t)mbi.BaseAddress, end);
        ok = end - a >= n ? n : (size_t)(end - a);
    }
    return ok;
}

static FILE *g_ptxt, *g_pbin, *g_isbin, *g_ccbin, *g_bbin, *g_rbin, *g_obin;
static size_t g_pbytes, g_ccbytes;

// B = [controller+0x20] (Havok side: pos +0x90, vel/s +0xa0) and R = [InstanceSet+0xf8] (pos +0x254, vel/s x,z
// +0x26c/+0x274), found by SAVAGEWIND_LINK_FIND in find6. Resolved every frame, checked with readable() only when the
// pointer changes (VirtualQuery is ~2 ms here), logged when it changes, dumped to <prefix>.b.bin / .r.bin.
#define B_HOLDER 0x20
#define R_HOLDER 0xf8
#define B_BYTES 0x100
#define R_BYTES 0x300
static uint64_t g_b, g_r;  // 0 = not resolved / not readable

#define MAX_HITS 512
#define HIT_SPAN 0x60
static FILE *g_hbin;
static char g_hprefix[480];
static uint64_t g_find_at, g_hits[MAX_HITS];
static bool g_hit_ok[MAX_HITS];  // the whole +-HIT_SPAN window was readable at find time
static int g_nhits = -1;  // -1: not searched yet

// Calls fn on every 4 KB page of committed, writable guest memory that is already in the working set. Only resident
// pages: reading a committed page nobody touched yet makes Windows back it (find2/find3 read ~3.2 GB that way and the
// game crawled at ~1.5 fps afterwards). Link's state is in use every frame, so it is always resident.
static size_t for_each_page(void (*fn)(uint64_t pg, void *ctx), void *ctx) {
    size_t scanned = 0;
    uint64_t a = GUEST_LO;
    while (a < GUEST_HI) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery((const void *)(uintptr_t)a, &mbi, sizeof mbi)) break;
        uint64_t base = (uint64_t)(uintptr_t)mbi.BaseAddress, end = base + mbi.RegionSize;
        if (mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE)) &&
            (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE))) {
            PSAPI_WORKING_SET_EX_INFORMATION ws[1024];  // (s111 camfast: on the stack, the camera scan's worker thread calls this too)
            for (uint64_t chunk = base; chunk < end; chunk += sizeof ws / sizeof ws[0] * 0x1000) {
                size_t np = 0;
                for (uint64_t pg = chunk; pg < end && np < sizeof ws / sizeof ws[0]; pg += 0x1000)
                    ws[np++].VirtualAddress = (PVOID)(uintptr_t)pg;
                if (!QueryWorkingSetEx(GetCurrentProcess(), ws, (DWORD)(np * sizeof ws[0]))) continue;
                for (size_t q = 0; q < np; q++)
                    if (ws[q].VirtualAttributes.Valid) fn((uint64_t)(uintptr_t)ws[q].VirtualAddress, ctx), scanned += 0x1000;
            }
        }
        a = end;
    }
    return scanned;
}

static void find_page(uint64_t pg, void *ctx) {
    const float *p = ctx, *f = (const float *)(uintptr_t)pg;
    float x = p[0], y = p[1], z = p[2];
    // (x at the page's last 2 floats can't be checked without the next page: skipped, rare)
    for (size_t i = 0; i + 2 < 0x1000 / 4; i++) {  // (written so that NaN never matches)
        float d = f[i] - x;
        if (!(d <= 0.02f && d >= -0.02f)) continue;
        float dy = f[i + 1] - y, dz = f[i + 2] - z;
        if (!(dy <= 0.02f && dy >= -0.02f && dz <= 0.02f && dz >= -0.02f)) continue;
        if (g_nhits < MAX_HITS) g_hits[g_nhits++] = pg + i * 4;
    }
}

// Every float triple within 0.02 of p in committed, writable guest memory (4-byte aligned).
static void find_hits(const float *p) {
    float x = p[0], y = p[1], z = p[2];
    g_nhits = 0;
    size_t scanned = for_each_page(find_page, (void *)p);
    // Drop hits that are slow to read (GPU-visible / uncached mappings: dumping those every frame stalled the game to
    // ~1 fps in run find2) - they are render copies anyway, not the physics state.
    LARGE_INTEGER qf, t0, t1;
    QueryPerformanceFrequency(&qf);
    static uint8_t sink[2 * HIT_SPAN];
    int kept = 0;
    uint64_t slow[MAX_HITS];
    int nslow = 0;
    for (int k = 0; k < g_nhits; k++) {
        uint64_t lo = g_hits[k] - HIT_SPAN;
        if (readable(lo, 2 * HIT_SPAN) < 2 * HIT_SPAN) { g_hit_ok[kept] = false; g_hits[kept++] = g_hits[k]; continue; }
        QueryPerformanceCounter(&t0);
        for (int r = 0; r < 8; r++) memcpy(sink, (const void *)(uintptr_t)lo, sizeof sink);
        QueryPerformanceCounter(&t1);
        double us = (double)(t1.QuadPart - t0.QuadPart) * 1e6 / (double)qf.QuadPart;
        if (us > 4.0) slow[nslow++] = g_hits[k];
        else g_hit_ok[kept] = true, g_hits[kept++] = g_hits[k];
    }
    g_nhits = kept;
    char path[512];
    snprintf(path, sizeof path, "%s.hits.txt", g_hprefix);
    FILE *t = fopen(path, "w");
    if (t) {
        fprintf(t, "# pos %.3f %.3f %.3f, %zu MB scanned, %d hits kept, %d slow to read (dropped)\n", x, y, z,
                scanned >> 20, g_nhits, nslow);
        for (int k = 0; k < g_nhits; k++) fprintf(t, "%d %llx\n", k, (unsigned long long)g_hits[k]);
        for (int k = 0; k < nslow; k++) fprintf(t, "slow %llx\n", (unsigned long long)slow[k]);
        fclose(t);
    }
    snprintf(path, sizeof path, "%s.hits.bin", g_hprefix);
    g_hbin = fopen(path, "ab");
    LOG("[linkpos] find: %d copies of Link's position kept, %d slow ones dropped (%zu MB scanned)\n", g_nhits, nslow,
        scanned >> 20);
}

// The copy that moves Link (bisect2: pushing it pushes him, the game keeps it): a cluster of position copies at
// head, +0x10, +0x20 (and +0xc0) in the Havok heap (0x72456a3430 one boot, 0x724569ca90 the next). g_body = head,
// found among the hits by that shape; pokes take k<off> relative to it. Then every u64 in resident guest memory that
// points into [head-0x400, head+0x10) is listed in <prefix>.ptrs.txt, tagged when it sits inside the actor /
// InstanceSet / controller / B / R, to find a pointer path that does not need the 1 s scan.
// The pointer scan (bisect4.ptrs.txt) found the object start = head - 0x1a0, held by the controller at +0x410 (and
// +0x440): probe_frame resolves g_body = [cc+0x410] + 0x1a0 every frame (validated once per pointer change), so k<off>
// works without SAVAGEWIND_LINK_FIND. Layout (bisect4, looks like hkpMotion's motion state): head +0x00 = transform
// translation, +0x10 = centerOfMass0, +0x20 = centerOfMass1 (the one that moves Link), +0x90 = linear velocity (u/s).
static uint64_t g_body, g_actor, g_is, g_cc;
#define BODY_HOLDER 0x410
#define BODY_HEAD 0x1a0
#define BODY_BYTES 0x200
#define BODY_PTR_BEFORE 0x400
#define MAX_PTRS 256
static uint64_t g_ptr_at[MAX_PTRS], g_ptr_val[MAX_PTRS];
static int g_nptrs;

static bool is_hit(uint64_t a) {
    for (int k = 0; k < g_nhits; k++) if (g_hits[k] == a) return true;
    return false;
}

static void ptr_page(uint64_t pg, void *ctx) {
    (void)ctx;
    const uint64_t *q = (const uint64_t *)(uintptr_t)pg;
    for (size_t i = 0; i < 0x1000 / 8; i++)
        if (q[i] - (g_body - BODY_PTR_BEFORE) < BODY_PTR_BEFORE + 0x10 && g_nptrs < MAX_PTRS)
            g_ptr_at[g_nptrs] = pg + i * 8, g_ptr_val[g_nptrs++] = q[i];
}

static const char *ptr_owner(uint64_t at, uint64_t *off) {
    struct { const char *name; uint64_t base, size; } owners[] = {
        {"actor", g_actor, 0x4000}, {"is", g_is, INSTSET_SIZE}, {"cc", g_cc, 0x800}, {"B", g_b, B_BYTES},
        {"R", g_r, R_BYTES}, {"body", g_body - BODY_PTR_BEFORE, BODY_PTR_BEFORE + 0x200},
    };
    for (size_t i = 0; i < sizeof owners / sizeof owners[0]; i++)
        if (owners[i].base && at - owners[i].base < owners[i].size) { *off = at - owners[i].base; return owners[i].name; }
    return NULL;
}

static void find_body(void) {
    uint64_t via = g_body;  // from the pointer path (probe_frame)
    g_body = 0;
    for (int k = 0; k < g_nhits && !g_body; k++)
        if (is_hit(g_hits[k] + 0x10) && is_hit(g_hits[k] + 0x20)) g_body = g_hits[k];
    if (!g_body) { LOG("[linkpos] find: no body cluster (head, +0x10, +0x20) among the hits\n"); g_body = via; return; }
    LOG("[linkpos] find: body cluster %llx, pointer path [cc+%#x]+%#x gives %llx (%s)\n", (unsigned long long)g_body,
        BODY_HOLDER, BODY_HEAD, (unsigned long long)via, via == g_body ? "same" : "DIFFERENT");
    if (via && via != g_body) g_body = via;  // keep the validated one for pokes
    g_nptrs = 0;
    for_each_page(ptr_page, NULL);
    LOG("[linkpos] find: body cluster at %llx (+0xc0 %s), %d pointers into [-%#x, +0x10)\n",
        (unsigned long long)g_body, is_hit(g_body + 0xc0) ? "hit" : "no hit", g_nptrs, BODY_PTR_BEFORE);
    char path[512];
    snprintf(path, sizeof path, "%s.ptrs.txt", g_hprefix);
    FILE *t = fopen(path, "w");
    if (!t) return;
    fprintf(t, "# body %llx; actor %llx is %llx cc %llx B %llx R %llx\n# holder value value-body owner+off\n",
            (unsigned long long)g_body, (unsigned long long)g_actor, (unsigned long long)g_is,
            (unsigned long long)g_cc, (unsigned long long)g_b, (unsigned long long)g_r);
    for (int i = 0; i < g_nptrs; i++) {
        uint64_t off = 0;
        const char *own = ptr_owner(g_ptr_at[i], &off);
        long long rel = (long long)(g_ptr_val[i] - g_body);
        fprintf(t, "%llx %llx %s%#llx %s+%#llx\n", (unsigned long long)g_ptr_at[i], (unsigned long long)g_ptr_val[i],
                rel < 0 ? "-" : "+", rel < 0 ? -rel : rel, own ? own : "-", (unsigned long long)off);
    }
    fclose(t);
}

static void probe_init(void) {
    const char *prefix = getenv("SAVAGEWIND_LINK_PROBE");
    if (!prefix || !*prefix) return;
    char path[512];
    snprintf(path, sizeof path, "%s.txt", prefix);
    g_ptxt = fopen(path, "a");
    const char *nb = getenv("SAVAGEWIND_LINK_PROBE_BYTES");
    g_pbytes = nb && *nb ? strtoull(nb, NULL, 0) : 0x4000;
    if (g_pbytes) {
        snprintf(path, sizeof path, "%s.bin", prefix);
        g_pbin = fopen(path, "ab");
    }
    if (env_int("SAVAGEWIND_LINK_PROBE_PHYS", 1)) {
        snprintf(path, sizeof path, "%s.is.bin", prefix);
        g_isbin = fopen(path, "ab");
        snprintf(path, sizeof path, "%s.cc.bin", prefix);
        g_ccbin = fopen(path, "ab");
        const char *cb = getenv("SAVAGEWIND_LINK_PROBE_CC_BYTES");
        g_ccbytes = cb && *cb ? strtoull(cb, NULL, 0) : 0x800;
        snprintf(path, sizeof path, "%s.b.bin", prefix);
        g_bbin = fopen(path, "ab");
        snprintf(path, sizeof path, "%s.r.bin", prefix);
        g_rbin = fopen(path, "ab");
        snprintf(path, sizeof path, "%s.body.bin", prefix);  // BODY_BYTES from the body head
        g_obin = fopen(path, "ab");
    }
    snprintf(g_hprefix, sizeof g_hprefix, "%s", prefix);
    const char *fa = getenv("SAVAGEWIND_LINK_FIND");
    g_find_at = fa && *fa ? strtoull(fa, NULL, 0) : 0;
    if (g_ptxt)
        fprintf(g_ptxt, "# frame present actor | pinfo_pos xyz | mtx_t xyz | vel xyz | model a0 a1 a2 a3 | "
                        "4e8 4ec 4f0 4f4 4f8 4fc | flags2 flags | instset cc\n");
    LOG("[linkpos] probe -> %s.txt/.bin (%zu bytes of the actor per frame)\n", prefix, g_pbytes);
}

// SAVAGEWIND_LINK_POKE: applied on the game thread right after the frame's update, so the next draw sees it unless the
// game recomputes the field first (which is itself the answer the experiment is after).
static void poke_frame(uint64_t present, uint64_t actor, uint64_t is, uint64_t cc) {
    static const char *spec;
    static int init, active = -1, entered = -1;
    if (!init) { init = 1; spec = getenv("SAVAGEWIND_LINK_POKE"); }
    if (!spec || !*spec) return;
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", spec);
    int idx = 0, now = -1;
    char *s1 = NULL;
    for (char *win = strtok_r(buf, ";", &s1); win; win = strtok_r(NULL, ";", &s1), idx++) {
        unsigned long long from, to;
        int used = 0;
        if (sscanf(win, "%llu-%llu:%n", &from, &to, &used) < 2 || !used) continue;
        if (present < from || present >= to) continue;
        now = idx;
        char *s2 = NULL;
        for (char *w = strtok_r(win + used, "+", &s2); w; w = strtok_r(NULL, "+", &s2)) {
            char *eq = strchr(w, '=');
            if (!eq) continue;
            uint64_t base = actor, size = 0x4000;
            bool checked = false;  // base already validated (b/r): skip the per-write VirtualQuery
            if (*w == 'c') base = cc, size = g_ccbytes ? g_ccbytes : 0x800, w++;
            else if (*w == 'i') base = is, size = INSTSET_SIZE, w++;
            else if (*w == 'b') base = g_b, size = B_BYTES, checked = true, w++;
            else if (*w == 'r') base = g_r, size = R_BYTES, checked = true, w++;
            else if (*w == 'k') base = g_body, size = BODY_BYTES, checked = true, w++;  // body head (probe_frame)
            else if (*w == 'h') {  // h<k>:<off>, h<k0>-<k1>:<off> (hits k0..k1), h*:<off> (all) - SAVAGEWIND_LINK_FIND hits
                char *colon;
                long k0, k1;
                if (w[1] == '*') k0 = 0, k1 = g_nhits - 1, colon = w + 2;
                else if (w[1] == '@') {  // h@<lo>-<hi>:<off>: the hits at addresses in [lo, hi) (indices shift per boot)
                    uint64_t lo = strtoull(w + 2, &colon, 16), hi = *colon == '-' ? strtoull(colon + 1, &colon, 16) : lo + 1;
                    k0 = 0;
                    while (k0 < g_nhits && g_hits[k0] < lo) k0++;
                    k1 = k0 - 1;
                    while (k1 + 1 < g_nhits && g_hits[k1 + 1] < hi) k1++;
                    if (k1 < k0) { if (entered != idx) LOG("[linkpos] poke %s: no hits\n", w); continue; }
                } else {
                    k0 = k1 = strtol(w + 1, &colon, 10);
                    if (*colon == '-') k1 = strtol(colon + 1, &colon, 10);
                }
                if (k1 >= g_nhits) k1 = g_nhits - 1;  // (a range past the end: up to the last hit)
                if (*colon != ':' || k0 < 0 || k0 > k1) continue;
                long long hoff = strtoll(colon + 1, NULL, 0);
                if (hoff < -HIT_SPAN || hoff + 4 > HIT_SPAN) continue;
                float val = strtof(eq + 1, NULL);
                bool add = eq[-1] == '~';
                int n = 0;
                for (long k = k0; k <= k1; k++) {  // (g_hit_ok: the window was checked at find time, no VirtualQuery)
                    if (!g_hit_ok[k]) continue;
                    float *f = (float *)(uintptr_t)(g_hits[k] + hoff);
                    *f = add ? *f + val : val, n++;
                }
                if (entered != idx) LOG("[linkpos] poke %s: %d hits (%ld..%ld)\n", w, n, k0, k1);
                continue;
            }
            uint64_t off = strtoull(w, NULL, 0);
            float val = strtof(eq + 1, NULL);
            bool add = eq > w && eq[-1] == '~';  // <off>~=<f32>: add every frame (drift test) instead of setting
            if (base && off + 4 <= size && (checked || readable(base + off, 4) == 4)) {
                float *f = (float *)(uintptr_t)(base + off);
                if (entered != idx)  // first frame of the window: where the write goes and what was there
                    LOG("[linkpos] poke %s at %llx: %g -> %g\n", w, (unsigned long long)(base + off), *f,
                        add ? *f + val : val);
                *f = add ? *f + val : val;
            } else if (entered != idx)
                LOG("[linkpos] poke %s: no target (base %llx)\n", w, (unsigned long long)base);
        }
        entered = idx;
    }
    if (now != active) { LOG("[linkpos] poke window %d at presented frame %llu\n", now, (unsigned long long)present); active = now; }
}

static void probe_frame(uint64_t frame, uint64_t pinfo) {
    uint64_t actor = *(uint64_t *)(uintptr_t)(pinfo + PLAYER_ACTOR);
    extern uint64_t vk_frame_counter(void);
    uint64_t present = vk_frame_counter();
    if (!in_guest(actor) || readable(actor, 0x840) < 0x840) return;
    const uint8_t *a = (const uint8_t *)(uintptr_t)actor;
    uint64_t is = *(const uint64_t *)(a + ACTOR_PHYSICS), cc = 0;
    if (!in_guest(is) || readable(is, INSTSET_SIZE) < INSTSET_SIZE) is = 0;
    if (is) cc = *(const uint64_t *)(uintptr_t)(is + INSTSET_CC);
    if (!in_guest(cc) || readable(cc, 0x10) < 0x10) cc = 0;
    {  // B / R: re-validate only when the pointer changes
        static uint64_t last_braw = 1, last_rraw = 1;
        uint64_t braw = cc ? *(const uint64_t *)(uintptr_t)(cc + B_HOLDER) : 0;
        uint64_t rraw = is ? *(const uint64_t *)(uintptr_t)(is + R_HOLDER) : 0;
        if (braw != last_braw) {
            last_braw = braw;
            g_b = in_guest(braw) && readable(braw, B_BYTES) == B_BYTES ? braw : 0;
            LOG("[linkpos] B = [cc %llx +0x20] = %llx%s (presented %llu)\n", (unsigned long long)cc,
                (unsigned long long)braw, g_b ? "" : " (not readable)", (unsigned long long)present);
        }
        static uint64_t last_oraw = 1;
        uint64_t oraw = cc ? *(const uint64_t *)(uintptr_t)(cc + BODY_HOLDER) : 0;
        if (oraw != last_oraw) {
            last_oraw = oraw;
            g_body = in_guest(oraw) && readable(oraw, BODY_HEAD + BODY_BYTES) == BODY_HEAD + BODY_BYTES ? oraw + BODY_HEAD : 0;
            LOG("[linkpos] body = [cc +%#x] = %llx + %#x = %llx%s (presented %llu)\n", BODY_HOLDER,
                (unsigned long long)oraw, BODY_HEAD, (unsigned long long)g_body, g_body ? "" : " (not readable)",
                (unsigned long long)present);
        }
        if (rraw != last_rraw) {
            last_rraw = rraw;
            g_r = in_guest(rraw) && readable(rraw, R_BYTES) == R_BYTES ? rraw : 0;
            LOG("[linkpos] R = [is %llx +0xf8] = %llx%s (presented %llu)\n", (unsigned long long)is,
                (unsigned long long)rraw, g_r ? "" : " (not readable)", (unsigned long long)present);
        }
    }
    if (g_find_at && g_nhits < 0 && present >= g_find_at) {
        LARGE_INTEGER qf, t0, t1;
        QueryPerformanceFrequency(&qf);
        QueryPerformanceCounter(&t0);
        g_actor = actor, g_is = is, g_cc = cc;
        find_hits((const float *)(uintptr_t)(pinfo + PLAYER_POS));
        find_body();
        QueryPerformanceCounter(&t1);
        LOG("[linkpos] find took %.1f ms\n", (double)(t1.QuadPart - t0.QuadPart) * 1e3 / (double)qf.QuadPart);
    }
    poke_frame(present, actor, is, cc);
    const float *pp = (const float *)(uintptr_t)(pinfo + PLAYER_POS);
    const float *m = (const float *)(a + ACTOR_MTX), *v = (const float *)(a + ACTOR_VELOCITY);
    const float *op = (const float *)(a + ACTOR_OPACITY);
    uint64_t model = *(const uint64_t *)(a + ACTOR_MODEL);
    uint8_t mf[4] = {0xff, 0xff, 0xff, 0xff};
    if (in_guest(model) && readable(model, 0xa4) == 0xa4) memcpy(mf, (const void *)(uintptr_t)(model + MODEL_FLAGS), 4);
    if (g_ptxt) {
        fprintf(g_ptxt, "%llu %llu %llx | %.3f %.3f %.3f | %.3f %.3f %.3f | %.4f %.4f %.4f | %llx %u %u %u %u | "
                        "%g %g %g %g %g %g | %#x %#llx | %llx %llx\n",
                (unsigned long long)frame, (unsigned long long)present, (unsigned long long)actor, pp[0], pp[1],
                pp[2], m[3], m[7], m[11], v[0], v[1], v[2], (unsigned long long)model, mf[0], mf[1], mf[2], mf[3],
                op[0], op[1], op[2], op[3], op[4], op[5], *(const uint32_t *)(a + ACTOR_FLAGS2),
                (unsigned long long)*(const uint64_t *)(a + ACTOR_FLAGS), (unsigned long long)is,
                (unsigned long long)cc);
        if (frame % 60 == 0) fflush(g_ptxt);
    }
    if (g_pbin) {
        uint64_t hdr[4] = {frame, present, actor, readable(actor, g_pbytes)};
        fwrite(hdr, sizeof hdr, 1, g_pbin);
        fwrite(a, 1, (size_t)hdr[3], g_pbin);
        if (frame % 60 == 0) fflush(g_pbin);
    }
    if (g_isbin && is) {
        uint64_t hdr[4] = {frame, present, is, INSTSET_SIZE};
        fwrite(hdr, sizeof hdr, 1, g_isbin);
        fwrite((const void *)(uintptr_t)is, 1, INSTSET_SIZE, g_isbin);
        if (frame % 60 == 0) fflush(g_isbin);
    }
    if (g_ccbin && cc) {
        uint64_t hdr[4] = {frame, present, cc, readable(cc, g_ccbytes)};
        fwrite(hdr, sizeof hdr, 1, g_ccbin);
        fwrite((const void *)(uintptr_t)cc, 1, (size_t)hdr[3], g_ccbin);
        if (frame % 60 == 0) fflush(g_ccbin);
    }
    if (g_bbin && g_b) {
        uint64_t hdr[4] = {frame, present, g_b, B_BYTES};
        fwrite(hdr, sizeof hdr, 1, g_bbin);
        fwrite((const void *)(uintptr_t)g_b, 1, B_BYTES, g_bbin);
        if (frame % 60 == 0) fflush(g_bbin);
    }
    if (g_rbin && g_r) {
        uint64_t hdr[4] = {frame, present, g_r, R_BYTES};
        fwrite(hdr, sizeof hdr, 1, g_rbin);
        fwrite((const void *)(uintptr_t)g_r, 1, R_BYTES, g_rbin);
        if (frame % 60 == 0) fflush(g_rbin);
    }
    if (g_obin && g_body) {
        uint64_t hdr[4] = {frame, present, g_body, BODY_BYTES};
        fwrite(hdr, sizeof hdr, 1, g_obin);
        fwrite((const void *)(uintptr_t)g_body, 1, BODY_BYTES, g_obin);
        if (frame % 60 == 0) fflush(g_obin);
    }
    if (g_hbin && g_nhits > 0) {
        static LARGE_INTEGER qf, t0, t1;
        static double spent_ms;
        static uint64_t dumps;
        if (!qf.QuadPart) QueryPerformanceFrequency(&qf);
        QueryPerformanceCounter(&t0);
        uint64_t hdr[4] = {frame, present, (uint64_t)g_nhits, 2 * HIT_SPAN};
        fwrite(hdr, sizeof hdr, 1, g_hbin);
        // No readable() here: VirtualQuery costs ~2 ms in this process (find5: 241 hits = 545 ms a frame). Each span was
        // checked once in find_hits (g_hit_ok), and guest memory stays mapped for the whole run.
        static uint8_t zero[2 * HIT_SPAN];
        for (int k = 0; k < g_nhits; k++) {
            uint64_t lo = g_hits[k] - HIT_SPAN;
            fwrite(&g_hits[k], 8, 1, g_hbin);
            fwrite(g_hit_ok[k] ? (const void *)(uintptr_t)lo : zero, 1, 2 * HIT_SPAN, g_hbin);
        }
        if (frame % 60 == 0) fflush(g_hbin);
        QueryPerformanceCounter(&t1);
        spent_ms += (double)(t1.QuadPart - t0.QuadPart) * 1e3 / (double)qf.QuadPart;
        if (++dumps % 60 == 0) { LOG("[linkpos] hit dumps: %.2f ms/frame (presented %llu)\n", spent_ms / 60, (unsigned long long)present); spent_ms = 0; }
    }
}

// ------------------------------------------------------------------ servo: move Link by a delta
#define PAUSE_STATE (SS->pause_state)  // (u32: 0 playing, 2 in the pause menu or the map; framepace.c)
#define BODY_TRANSFORM 0x0           // head offsets of the three position copies (servo2 / bisect4)
#define BODY_COM0 0x10
#define BODY_COM1 0x20

static float g_move[3];

void linkpos_move(float dx, float dy, float dz) { g_move[0] += dx, g_move[1] += dy, g_move[2] += dz; }

// Link's body head this frame, or 0. The chain PlayerInfo -> actor -> InstanceSet -> controller -> object is read
// every frame but VirtualQuery'd (~2 ms a call here) only when one of its pointers changes. g_servo_actor / g_servo_cc
// are that body's actor and controller (valid while it returns nonzero).
static uint64_t g_servo_actor, g_servo_cc;

static uint64_t servo_body(void) {
    static uint64_t last[4] = {1, 1, 1, 1}, head;
    uint64_t pinfo = *(uint64_t *)(uintptr_t)PLAYER_INFO_PTR, actor = 0, is = 0, cc = 0, obj = 0;
    g_servo_actor = g_servo_cc = 0;
    if (!in_guest(pinfo)) return 0;
    actor = *(uint64_t *)(uintptr_t)(pinfo + PLAYER_ACTOR);
    if (actor != last[0]) {
        last[0] = actor, last[1] = last[2] = last[3] = 1, head = 0;
        if (!in_guest(actor) || readable(actor + ACTOR_PHYSICS, 8) < 8) last[0] = 1;
    }
    if (last[0] == 1) return 0;
    is = *(uint64_t *)(uintptr_t)(actor + ACTOR_PHYSICS);
    if (is != last[1]) {
        last[1] = is, last[2] = last[3] = 1, head = 0;
        if (!in_guest(is) || readable(is + INSTSET_CC, 8) < 8) last[1] = 1;
    }
    if (last[1] == 1) return 0;
    cc = *(uint64_t *)(uintptr_t)(is + INSTSET_CC);
    if (cc != last[2]) {
        last[2] = cc, last[3] = 1, head = 0;
        if (!in_guest(cc) || readable(cc + BODY_HOLDER, 8) < 8) last[2] = 1;
    }
    if (last[2] == 1) return 0;
    obj = *(uint64_t *)(uintptr_t)(cc + BODY_HOLDER);
    if (obj != last[3]) {
        last[3] = obj;
        head = in_guest(obj) && readable(obj + BODY_HEAD, 0xa0) == 0xa0 ? obj + BODY_HEAD : 0;  // (to the velocity)
        LOG("[linkpos] servo body = [cc %llx +%#x] + %#x = %llx%s\n", (unsigned long long)cc, BODY_HOLDER, BODY_HEAD,
            (unsigned long long)head, head ? "" : " (not readable)");
    }
    if (head) g_servo_actor = actor, g_servo_cc = cc;
    return head;
}

static void servo_frame(void) {
    static const char *spec;
    static int init, active = -1;
    if (!init) { init = 1; spec = getenv("SAVAGEWIND_LINK_SERVO"); }
    if (spec && *spec) {  // test driver
        extern uint64_t vk_frame_counter(void);
        uint64_t present = vk_frame_counter();
        int idx = 0, now = -1;
        static uint64_t once;  // one-frame windows (to = from + 1) fire once at the first frame >= from: presented
                               // frames step by 2 per game frame at 30 fps, so [from, from + 1) can be skipped (hit1)
        for (const char *w = spec; *w && idx < 64; idx++) {
            unsigned long long from, to;
            float d[3];
            if (sscanf(w, "%llu-%llu:%f,%f,%f", &from, &to, &d[0], &d[1], &d[2]) == 5 && present >= from &&
                (to == from + 1 ? !(once >> idx & 1) : present < to))
                now = idx, once |= (to == from + 1 ? 1ull : 0ull) << idx, linkpos_move(d[0], d[1], d[2]);
            const char *semi = strchr(w, ';');
            if (!semi) break;
            w = semi + 1;
        }
        if (now != active) { LOG("[linkpos] servo window %d at presented frame %llu\n", now, (unsigned long long)present); active = now; }
    }
    if (!g_move[0] && !g_move[1] && !g_move[2]) return;
    uint64_t head = *(uint32_t *)(uintptr_t)PAUSE_STATE ? 0 : servo_body();
    if (head) {
        static const int copies[] = {BODY_TRANSFORM, BODY_COM0, BODY_COM1};
        for (int c = 0; c < 3; c++) {
            float *p = (float *)(uintptr_t)(head + copies[c]);
            p[0] += g_move[0], p[1] += g_move[1], p[2] += g_move[2];
        }
    }
    g_move[0] = g_move[1] = g_move[2] = 0;  // (paused or no body: dropped, not saved up for a jump later)
}

// ------------------------------------------------------------------ velocity servo (step 2f)
// The game sets Link's body velocity (head +0x90/+0x94/+0x98, u/s) once a frame in CC_VEL_STEP, through 0x70000f3efc
// (setLinearVelocity?); then the solver (0x700011e510) and the integrator (0x700011e7a0, which also writes CoM1) run
// from it (probe/watch1, SAVAGEWIND_LINK_WATCH). The override calls the original and then replaces x/z with ours (y
// stays the game's: gravity, ground), so Havok moves him with full collision.
//   linkpos_set_velocity(on, vx, vz)       on: from the next physics step on, Link's horizontal velocity is (vx, vz) u/s
//   linkpos_link_state()                   LINK_GROUND / LINK_AIR / LINK_OTHER (climbing, grabbing, loading...): the
//                                          servo only drives him on the ground or in the air
//   SAVAGEWIND_LINK_VEL=<from>-<to>:<vx>,<vz>[,<stop x>][;...]   test driver (presented-frame windows; off outside
//                                          them; <stop x>: off once the body passes that x in vx's direction)
//   SAVAGEWIND_LINK_NO_STEP=1              no step-up (below)
// The step function runs for every character controller (~20 a frame, some after the physics step): only Link's call
// is overridden (velservo4 wrote all of them, and the late ones hid the solver's result): the one whose x0 is Link's
// controller + 0x398 (stepA3 trace: the only call that resets his velocity, once a frame; x0 = the controller itself
// never comes).
// Step-up (Link's own walk over the lip east of the spawn, walkeast1: blocked, then vy 3.5..4.6 for 4 frames, +0.6):
// at the start of Link's step the body still holds the last step's solved velocity; when its part along the target
// direction is below STEP_FREE of the target for 2 steps on the ground, vy is held at >= STEP_VY until he moves
// freely again, or until he has risen STEP_MAX without getting free (a wall: no more kicks until the direction turns
// or he moves freely).
#define CC_VEL_STEP (SS->cc_vel_step)  // 1.8.2 0x70011c0830: [f_70011c0830+0x59c] calls the velocity setter (watch1 stacks)
#define BODY_VEL 0x90
#define CC_STEP_LINK_ARG 0x398  // x0 of Link's call = controller + this
#define STEP_FREE 0.6f
#define STEP_VY 4.0f
#define STEP_MAX 0.8f
#define STEP_MIN_FRAMES 4  // (the walk kicked for 4 frames)
// Link's state word (probe field_timeline over walk/stand/fall/climb runs): 0x18000001 standing, 0x18000000 moving,
// 0x18000002 / 0x08000002 at a ledge (ground bits 0x08000000), 0x80000000 / 0x80080000 in the air, 0x2 grabbing a wall
// and 0x90 climbing (velservo3, walkall1: neither group), 0x1 while the save loads.
#define ACTOR_STATE 0xcec
#define STATE_GROUND 0x08000000u
#define STATE_AIR 0x80000000u
enum { LINK_NONE, LINK_GROUND, LINK_AIR, LINK_OTHER };

static struct {
    int blocked;     // frames in a row without free motion
    int kick;        // frames of the current kick (0: none)
    bool wall;
    float y0, dir[2];
} g_step;

static GuestFn g_vel_step;
static uint64_t g_vel_calls, g_vel_link_calls, g_vel_applied, g_vel_kicks, g_vel_wakes;  // (logged once a second by vel_frame)
static bool g_vel_on, g_no_step, g_vy_on;
static float g_vel[2], g_vy;

void linkpos_set_velocity(bool on, float vx, float vz) { g_vel_on = on, g_vel[0] = vx, g_vel[1] = vz; }

// Step 3: the vertical velocity too (u/s, on top of linkpos_set_velocity, which must be on): Sora's jump / fall curve.
// Written on the same path as the step-up kick (which it replaces while on); off = the game's gravity and ground.
void linkpos_set_velocity_y(bool on, float vy) { g_vy_on = on, g_vy = vy; }

// The body's vertical velocity for one write: ours, else the step-up kick's, else the game's.
static void write_vy(float *v) {
    if (g_vy_on) v[1] = g_vy;
    else if (g_step.kick && v[1] < STEP_VY) v[1] = STEP_VY;
}

static int state_of(uint64_t actor) {
    if (!actor) return LINK_NONE;
    uint32_t s = *(const uint32_t *)(uintptr_t)(actor + ACTOR_STATE);
    // (0: lifting off - stepA6 shows 0 then 0x08000000 for a few frames after each step-up kick; also before a load)
    return s & STATE_GROUND ? LINK_GROUND : s & STATE_AIR || !s ? LINK_AIR : LINK_OTHER;
}

int linkpos_link_state(void) { return servo_body() ? state_of(g_servo_actor) : LINK_NONE; }

// What the game reads from the pad (runtime/hle/input.c fill_state calls the filter): while Link's input is blocked he
// gets no left stick and no action buttons (A B X Y L R ZL ZR, stick click), so he never starts a walk, climb, jump or
// glide of his own; Plus / Minus, the d-pad and the right stick (camera) stay his. On while the servo drives him unless
// linkpos_block_link_input(false); SAVAGEWIND_LINK_VEL_INPUT=1 leaves the scripted input alone in test windows.
void (*g_game_input_filter)(uint64_t *b, int32_t *lx, int32_t *ly, int32_t *rx, int32_t *ry);
static int g_block_input = -1;  // -1: while the servo drives, 0 never, 1 always
static bool g_test_input;  // SAVAGEWIND_LINK_VEL_INPUT: test windows keep the scripted input
static bool g_autocontinue;  // SAVAGEWIND_LINK_AUTOCONTINUE=1: tap A from the title until Link is in the world
static uint64_t g_t0;  // presented frame Link first stood in the world (vel_frame), 0 before

void linkpos_block_link_input(bool on) { g_block_input = on; }

uint64_t linkpos_world_frame(void) { return g_t0; }

// The pad as the player holds it, before the filter (runtime/sora.c reads Sora's input here), on whatever thread polls.
void (*g_link_pad_tap)(uint64_t *b, int32_t *lx, int32_t *ly, int32_t *rx, int32_t *ry);
void linkpos_set_pad_tap(void (*fn)(uint64_t *, int32_t *, int32_t *, int32_t *, int32_t *)) { g_link_pad_tap = fn; }  // (sora_mod.dll)

int linkpos_link_life(void);
static void link_input_filter(uint64_t *b, int32_t *lx, int32_t *ly, int32_t *rx, int32_t *ry) {
    // (Continue on the title and the menu: A 8 frames every 2 s from frame 300; and after a Game Over in a test run -
    // life 0 or no Link - until he stands in the world with life again, so the run goes on from the autosave)
    static bool game_over;
    if (g_autocontinue && g_t0) {
        int life = linkpos_link_life();
        if (!game_over && life <= 0) game_over = true, LOG("[linkpos] Game Over (life %d): auto-Continue\n", life);
        else if (game_over && life > 0 && linkpos_link_state() == LINK_GROUND)
            game_over = false, LOG("[linkpos] back in the world after Continue\n");
    }
    if (g_autocontinue && (!g_t0 || game_over)) {
        extern uint64_t vk_frame_counter(void);
        uint64_t f = vk_frame_counter();
        if (f >= 300 && f % 60 < 8) *b |= 1;
    }
    if (g_link_pad_tap) g_link_pad_tap(b, lx, ly, rx, ry);  // (may add its own scripted input: a test player's pad)
    // the pause menu and the map keep the whole pad (blocking A there would leave the game unusable)
    if (*(volatile uint32_t *)(uintptr_t)PAUSE_STATE) return;
    if (!(g_block_input == 1 || (g_block_input == -1 && g_vel_on && !g_test_input))) return;
    // SAVAGEWIND_LINK_PASS=<mask>: these buttons still reach Link (tests: 8 = Y, his attack, so he draws his weapon)
    static int pass = -1;
    if (pass < 0) pass = (int)strtol(getenv("SAVAGEWIND_LINK_PASS") ? getenv("SAVAGEWIND_LINK_PASS") : "0", NULL, 0);
    *b &= ~(0x3dfull & ~(uint64_t)pass);  // A B X Y, L-stick click, L R ZL ZR (bits 0-3, 4, 6-9; RSTICK 5, Plus 10, Minus 11 kept)
    *lx = *ly = 0;
}


// Once a frame (vel_frame, game thread, between two physics steps: the body holds the last step's solved velocity -
// stepA5's trace shows it bouncing off the lip): decide the kick that every write of the next step applies.
static void step_frame(uint64_t head, int state) {
    float sp = sqrtf(g_vel[0] * g_vel[0] + g_vel[1] * g_vel[1]);
    const float *v = (const float *)(uintptr_t)(head + BODY_VEL), *p = (const float *)(uintptr_t)(head + BODY_COM1);
    if (g_no_step || !g_vel_on || g_vy_on || sp < 0.3f || !(state == LINK_GROUND || state == LINK_AIR)) {
        memset(&g_step, 0, sizeof g_step);
        return;
    }
    float dir[2] = {g_vel[0] / sp, g_vel[1] / sp};
    if (dir[0] * g_step.dir[0] + dir[1] * g_step.dir[1] < 0.87f) g_step.blocked = g_step.kick = 0, g_step.wall = false;
    g_step.dir[0] = dir[0], g_step.dir[1] = dir[1];
    bool free = v[0] * dir[0] + v[2] * dir[1] > STEP_FREE * sp;
    if (g_step.kick) {
        float rise = p[1] - g_step.y0;
        if (free && g_step.kick >= STEP_MIN_FRAMES) {
            LOG("[linkpos] step-up done at %.3f %.3f %.3f (+%.2f in %d frames)\n", p[0], p[1], p[2], rise, g_step.kick);
            g_step.kick = g_step.blocked = 0;
        } else if (rise > STEP_MAX) {
            LOG("[linkpos] step-up gave up at %.3f %.3f %.3f (+%.2f, still blocked: a wall)\n", p[0], p[1], p[2], rise);
            g_step.kick = 0, g_step.wall = true;
        } else g_step.kick++;
        return;
    }
    if (free) { g_step.blocked = 0, g_step.wall = false; return; }
    if (++g_step.blocked >= 2 && !g_step.wall && state == LINK_GROUND) {
        g_step.kick = 1, g_step.y0 = p[1];
        LOG("[linkpos] step-up at %.3f %.3f %.3f (solved v %.2f %.2f %.2f)\n", p[0], p[1], p[2], v[0], v[1], v[2]);
    }
}

// Link's body, actor and controller for vel_step: resolved once a frame by vel_frame on the game thread (the step calls
// run on several threads at once - stepA3 - so they must not run servo_body's caches themselves).
static uint64_t g_vel_head, g_vel_actor, g_vel_cc;

// A standing Link's controller is not stepped at all (stepA4: no call with his x0 while he idles): until his own call
// has come in a frame (g_link_step_frame), the other controllers' calls write his velocity, which wakes him (stepA3:
// his calls start the frame after); once it has, they leave it alone so the solver's result survives to step_frame.
static volatile uint64_t g_vel_frame_id, g_link_step_frame;  // (vel_frame counts frames)

// s55 airhold slice 2 (recon/S55_HELPER_AIRHOLD.md 4): other actors' character controllers (Bokoblins etc. have one:
// Bokoblin.bphysics use_character_controller, form "Standing") go through the same step; an actor given a velocity by
// linkpos_enemy_vel(actor, on, vx, vy, vz) (u/s, all three axes, the game's gravity replaced) gets it written after the
// original, like Link's. Entries are resolved once a frame on the game thread (evel_frame); vel_step only compares.
// A controller not stepped yet this frame is woken by the other calls, as Link's is (stepA4).
// s57 (recon/S57_HELPER_EVEL.md): live, a dying Bokoblin (life 0) rose on the KH2 curve through this override, but
// living standing ones never moved (y within 0.1 u, x/z ~0.1 u of drift). Our write after the step call can lose in
// ways only a living enemy's state triggers (S57 doc 2: the AI's own requests / a transform request applied before our
// write and integrated with the game's velocity, a frozen (InstanceSet::setFixed) or sleeping body, the CC's own
// ground state). Two additions, each with a knob (linkpos_enemy_vel_opts, env SAVAGEWIND_EVEL_REQ / _SNAP):
//  - req: once a frame (inside sora_frame, game thread, the window linkpos_raycast uses) the velocity is also handed to
//    BotW's own ksys::phys::RigidBody::setLinearVelocity(body, &v, eps) (0x70011bf3c0, the call the CC itself makes
//    on its body [cc+8]): the request flags the body, so its step call comes every frame and the game's step applies
//    it and wakes a sleeping island (f_70011c0830: setLinearVelocity on the motion, then activate if inactive).
//  - snap (1, the default): while the curve rises, if at the entry's own step the body is lower than where our last
//    step should have taken it (prev + v * dt, dt = the physics system's step [[0x7001c05ae0] + 0x64]), the three body
//    position copies (servo_frame's) are moved up to it before our velocity is written: the step then integrates from
//    there with full collision. 2 = also while falling, never below the body's start height. 0 = velocity only (s56).
// Diagnostics: `[linkpos] enemy velocity <actor> diag ...` on the first steps and whenever the body's state changes:
// the step call's pending motion flags (RigidBody +0x6c: 0x20 transform, 0x40 velocity), the hkpRigidBody motion
// type (+0x15c: 1-3 dynamic, 4 keyframed, 5 fixed), island active, actor +0x518 bit 0x400 (InstanceSet::setFixed
// from the actor update), body y vs expected, snaps.
#define EVEL_MAX 8
#define RB_SET_LINVEL (SS->rb_set_linear_velocity)  // RigidBody::setLinearVelocity(x0 body, x1 const Vec3f*, s0 eps) (S55 2.2, VERIFIED)
#define PHYS_SYSTEM_PTR (SS->phys_system_got)  // ksys::phys::System* ([sys + 0x64] = the step dt, s: f_71011947c4 / f_71011955d8)
#define PHYS_SYS_DT 0x64
#define RB_FLAGS 0x68         // ksys RigidBody (the step call's x0): u32 flags, u32 pending motion flags at +0x6c
#define RB_MOTION_FLAGS 0x6c
#define HKP_ISLAND 0x128      // hkpRigidBody (= the body object, head - 0x1a0): simulation island; +0x31 u16 bit 10 = active
#define HKP_MOTION_TYPE 0x15c
#define ACTOR_FIXED_BIT 0x400u  // actor +0x518 (ACTOR_FLAGS2): set by f_71014105b8 (Actor -> InstanceSet::setFixed)
#define EVEL_SNAP_EPS 0.1f
static struct {
    uint64_t actor, cc, head, island; float v[3]; bool on, seen; volatile uint64_t step_frame;
    bool have_exp, aok; float exp[3], y0; uint64_t exp_frame;  // (s57 snap: expected CoM1 at the next own step)
    uint32_t key; int ndiag; uint64_t steps, snaps, reqs; float snapped, ymax;
} g_evel[EVEL_MAX];
static SRWLOCK g_evel_lock = SRWLOCK_INIT;
static bool g_in_frame;  // (linkpos_raycast's window, below)
static int g_evel_req = -1, g_evel_snap = -1;  // (-1: from the env at first use)

// Live knobs from sora_mod.dll (GetProcAddress): req 0/1, snap 0/1/2; a negative value keeps the current one.
void linkpos_enemy_vel_opts(int req, int snap) {
    if (req >= 0) g_evel_req = req != 0;
    if (snap >= 0) g_evel_snap = snap;
    LOG("[linkpos] enemy velocity opts: req %d snap %d\n", g_evel_req, g_evel_snap);
}
static void evel_opts_init(void) {
    if (g_evel_req < 0) { const char *e = getenv("SAVAGEWIND_EVEL_REQ"); g_evel_req = !(e && *e == '0'); }
    if (g_evel_snap < 0) { const char *e = getenv("SAVAGEWIND_EVEL_SNAP"); g_evel_snap = e && *e ? atoi(e) : 1; }
}

static float evel_dt(void) {  // the physics step, s (1/30 when it cannot be read)
    uint64_t sys = *(const uint64_t *)(uintptr_t)PHYS_SYSTEM_PTR;
    float dt = in_guest(sys) ? *(const float *)(uintptr_t)(sys + PHYS_SYS_DT) : 0;
    return dt >= 1.0f / 240 && dt <= 0.2f ? dt : 1.0f / 30;
}

// One guest call, game thread only: the game's own velocity request on the controller's body (as sora_airhold.h's
// ah_body_vel does for ragdoll bodies).
static bool evel_request(uint64_t body, const float v[3]) {
    Cpu *c = g_in_frame ? kernel_current_cpu() : NULL;
    if (!c || !dispatch_lookup(RB_SET_LINVEL)) return false;
    Cpu save = *c;
    uint64_t sp = (c->sp - 0x40) & ~0xfull;  // (scratch below the live guest sp)
    float *g = (float *)(uintptr_t)sp;
    g[0] = v[0], g[1] = v[1], g[2] = v[2], g[3] = 0;
    c->sp = sp, c->x[0] = body, c->x[1] = sp;
    float eps = 1.1920929e-7f;
    uint32_t eb;
    memcpy(&eb, &eps, 4);
    c->v[0] = eb;
    guest_call(c, RB_SET_LINVEL);
    *c = save;
    return true;
}

void linkpos_enemy_vel(uint64_t actor, bool on, float vx, float vy, float vz) {
    evel_opts_init();
    AcquireSRWLockExclusive(&g_evel_lock);
    int i, free_i = -1;
    uint64_t body = 0;
    for (i = 0; i < EVEL_MAX && !(g_evel[i].on && g_evel[i].actor == actor); i++)
        if (!g_evel[i].on && free_i < 0) free_i = i;
    if (i == EVEL_MAX && on && free_i >= 0) {
        i = free_i;
        memset(&g_evel[i], 0, sizeof g_evel[i]);
        g_evel[i].actor = actor;
    }
    if (i < EVEL_MAX) {
        g_evel[i].v[0] = vx, g_evel[i].v[1] = vy, g_evel[i].v[2] = vz;
        if (!on && g_evel[i].on)
            LOG("[linkpos] enemy velocity off for %llx (%llu own steps, %llu snaps %.2f u, %llu requests, body rose to %+.2f)\n",
                (unsigned long long)actor, (unsigned long long)g_evel[i].steps, (unsigned long long)g_evel[i].snaps,
                g_evel[i].snapped, (unsigned long long)g_evel[i].reqs, g_evel[i].ymax);
        g_evel[i].on = on;
        if (on && g_evel_req && g_in_frame && g_evel[i].head) body = g_evel[i].cc + CC_STEP_LINK_ARG, g_evel[i].reqs++;
    }
    ReleaseSRWLockExclusive(&g_evel_lock);
    if (body) { float v[3] = {vx, vy, vz}; evel_request(body, v); }  // (outside the lock: guest code)
}

static void evel_frame(void) {  // (vel_frame, game thread: the controller + body head of each entry, or 0)
    evel_opts_init();
    AcquireSRWLockExclusive(&g_evel_lock);
    for (int i = 0; i < EVEL_MAX; i++) {
        if (!g_evel[i].on) { g_evel[i].head = 0; continue; }
        uint64_t a = g_evel[i].actor, is = 0, cc = 0, obj = 0, head = 0;
        if (in_guest(a) && readable(a + ACTOR_PHYSICS, 8) == 8) is = *(const uint64_t *)(uintptr_t)(a + ACTOR_PHYSICS);
        if (in_guest(is) && readable(is + INSTSET_CC, 8) == 8) cc = *(const uint64_t *)(uintptr_t)(is + INSTSET_CC);
        if (in_guest(cc) && readable(cc + BODY_HOLDER, 8) == 8) obj = *(const uint64_t *)(uintptr_t)(cc + BODY_HOLDER);
        if (in_guest(obj) && readable(obj + BODY_HEAD, 0xa0) == 0xa0) head = obj + BODY_HEAD;
        if (head != g_evel[i].head || cc != g_evel[i].cc) {
            uint64_t cc8 = in_guest(cc) ? *(const uint64_t *)(uintptr_t)(cc + 8) : 0;  // (the CC's body getter f_71011961b4)
            LOG("[linkpos] enemy velocity %llx: controller %llx body %llx%s (CC body [cc+8] %llx%s)\n", (unsigned long long)a,
                (unsigned long long)cc, (unsigned long long)head, head ? "" : " (none: no override)", (unsigned long long)cc8,
                cc8 == cc + CC_STEP_LINK_ARG ? " = cc+0x398" : " != cc+0x398: the step match may miss it");
            g_evel[i].have_exp = false, g_evel[i].island = 0, g_evel[i].aok = false;
            if (head) g_evel[i].y0 = ((const float *)(uintptr_t)(head + BODY_COM1))[1];
        }
        g_evel[i].cc = cc, g_evel[i].head = head;
        if (!head) continue;
        // the island pointer (for the active bit in evel_step), checked here where VirtualQuery is affordable
        uint64_t isl = *(const uint64_t *)(uintptr_t)(obj + HKP_ISLAND);
        if (isl != g_evel[i].island) g_evel[i].island = in_guest(isl) && readable(isl + 0x30, 4) == 4 ? isl : 0;
        if (!g_evel[i].aok) g_evel[i].aok = readable(a + ACTOR_FLAGS2, 4) == 4;
        float y = ((const float *)(uintptr_t)(head + BODY_COM1))[1] - g_evel[i].y0;
        if (y > g_evel[i].ymax) g_evel[i].ymax = y;
    }
    ReleaseSRWLockExclusive(&g_evel_lock);
}

static void evel_diag(int i, uint64_t x0, uint32_t pend, const float *cur, float snap) {  // (from the entry's own step)
    uint64_t obj = g_evel[i].head - BODY_HEAD, isl = g_evel[i].island;
    uint32_t mtype = *(const uint8_t *)(uintptr_t)(obj + HKP_MOTION_TYPE);
    int active = isl ? (*(const uint16_t *)(uintptr_t)(isl + 0x31) >> 10) & 1 : -1;
    uint32_t af = g_evel[i].aok ? *(const uint32_t *)(uintptr_t)(g_evel[i].actor + ACTOR_FLAGS2) : 0;
    uint32_t key = (pend & 0x60) | mtype << 8 | (uint32_t)(active + 1) << 16 | (af & ACTOR_FIXED_BIT ? 1u : 0) << 18 |
                   (snap > 0 ? 1u : 0) << 19;
    g_evel[i].steps++;
    if (g_evel[i].ndiag < 4 || key != g_evel[i].key || g_evel[i].steps % 30 == 0) {
        g_evel[i].ndiag++, g_evel[i].key = key;
        LOG("[linkpos] enemy velocity %llx diag step %llu: pending %#x (flags %#x), motion type %u, island %s, actor fixed %d, "
            "body y %.2f (start %+.2f, expected %+.2f), v %.1f %.1f %.1f, snap %.2f (%llu snaps)\n",
            (unsigned long long)g_evel[i].actor, (unsigned long long)g_evel[i].steps, pend,
            *(const uint32_t *)(uintptr_t)(x0 + RB_FLAGS), mtype, active < 0 ? "?" : active ? "active" : "ASLEEP",
            (af & ACTOR_FIXED_BIT) != 0, cur[1], cur[1] - g_evel[i].y0,
            g_evel[i].have_exp ? g_evel[i].exp[1] - g_evel[i].y0 : 0.0f, g_evel[i].v[0], g_evel[i].v[1], g_evel[i].v[2], snap,
            (unsigned long long)g_evel[i].snaps);
    }
}

static bool evel_step(Cpu *c) {  // true: this call was an entry's own step (done here)
    uint64_t x0 = c->x[0];
    for (int i = 0; i < EVEL_MAX; i++) {
        uint64_t head = g_evel[i].on ? g_evel[i].head : 0;
        if (!head || x0 != g_evel[i].cc + CC_STEP_LINK_ARG) continue;
        uint32_t pend = *(const uint32_t *)(uintptr_t)(x0 + RB_MOTION_FLAGS);  // (what the game's step is about to apply)
        g_vel_step(c);
        float *v = (float *)(uintptr_t)(head + BODY_VEL), *cur = (float *)(uintptr_t)(head + BODY_COM1);
        float want[3] = {g_evel[i].v[0], g_evel[i].v[1], g_evel[i].v[2]}, dt = evel_dt(), snap = 0;
        // s57 snap: the body did not get where our last step should have taken it (and the curve still wants it there)
        bool fresh = g_evel[i].have_exp && g_evel[i].exp_frame + 1 == g_vel_frame_id;
        if (g_evel_snap && fresh && (want[1] > 0.5f || g_evel_snap >= 2)) {
            float d[3] = {g_evel[i].exp[0] - cur[0], g_evel[i].exp[1] - cur[1], g_evel[i].exp[2] - cur[2]};
            if (g_evel_snap >= 2 && cur[1] + d[1] < g_evel[i].y0 + 0.02f) d[1] = g_evel[i].y0 + 0.02f - cur[1];
            float len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            float lim = fmaxf(3.0f * sqrtf(want[0] * want[0] + want[1] * want[1] + want[2] * want[2]) * dt, 0.3f);
            if ((want[1] > 0.5f ? d[1] > EVEL_SNAP_EPS : len > EVEL_SNAP_EPS) && len < 4.0f) {
                float k = len > lim ? lim / len : 1.0f;
                static const int copies[] = {BODY_TRANSFORM, BODY_COM0, BODY_COM1};
                for (int n = 0; n < 3; n++) {
                    float *p = (float *)(uintptr_t)(head + copies[n]);
                    p[0] += d[0] * k, p[1] += d[1] * k, p[2] += d[2] * k;
                }
                snap = len * k, g_evel[i].snaps++, g_evel[i].snapped += snap;
            }
        }
        v[0] = want[0], v[1] = want[1], v[2] = want[2];
        for (int n = 0; n < 3; n++) g_evel[i].exp[n] = cur[n] + want[n] * dt;
        g_evel[i].have_exp = true, g_evel[i].exp_frame = g_vel_frame_id;
        g_evel[i].step_frame = g_vel_frame_id;
        if (!g_evel[i].seen) g_evel[i].seen = true, LOG("[linkpos] enemy velocity: %llx's controller step seen (x0 %llx)\n",
                                                        (unsigned long long)g_evel[i].actor, (unsigned long long)x0);
        evel_diag(i, x0, pend, cur, snap);
        return true;
    }
    return false;
}
static void evel_wake(void) {  // (after any other call: entries not stepped yet this frame get their velocity)
    for (int i = 0; i < EVEL_MAX; i++) {
        uint64_t head = g_evel[i].on ? g_evel[i].head : 0;
        if (!head || g_evel[i].step_frame == g_vel_frame_id) continue;
        float *v = (float *)(uintptr_t)(head + BODY_VEL);
        v[0] = g_evel[i].v[0], v[1] = g_evel[i].v[1], v[2] = g_evel[i].v[2];
    }
}

// s88 ride/wind (recon/S88_HELPER_RIDE.md): the servo replaced x/z of the body's velocity after BotW's own step and
// so threw away whatever BotW put there for outside motion (a moving platform's carry, a fan's / updraft's push; with
// Link's pad blocked his own walk is zero, so what the game wrote is the outside part). Link's step call now compares
// the body's velocity before and after the original: GROUND, the game's x/z is the carry; AIR, the change (game - held)
// is the push; an upward change of the held y (> EXT_UP, gravity only lowers it) is an updraft. That "ext" is added on
// top of Sora's velocity for EXT_HOLD frames after its last sample (a standing Link's step is not called every frame).
// GUESS: that BotW writes platform carry / wind into this same velocity; the log lines tell (set ride / set wind 0/1).
#define EXT_HOLD 3
#define EXT_MAX 30.0f
#define EXT_UP 1.0f
#define EXT_GROUND_MAX 10.0f  // (s88 review) u/s: a platform's carry; GUESS (BotW's lifts / Divine Beast parts are slower)
static int g_ride = 1, g_wind = 1;
static float g_ext[3];
static uint64_t g_ext_frame;
void linkpos_set_ride(int ride, int wind) { if (ride >= 0) g_ride = ride != 0; if (wind >= 0) g_wind = wind != 0; }

static void ext_sample(const float pre[3], const float post[3], int state) {
    float e[3] = {0, 0, 0};
    if (memcmp(pre, post, sizeof(float) * 3)) {
        float hx = state == LINK_GROUND ? post[0] : post[0] - pre[0], hz = state == LINK_GROUND ? post[2] : post[2] - pre[2];
        float h = sqrtf(hx * hx + hz * hz), dy = post[1] - pre[1];
        // (s88 review) the ground carry is the game's ABSOLUTE x/z, and the held velocity includes last frame's ext: a
        // controller step that only damps / collision-projects what we wrote (post = a * pre) would read Sora's own walk
        // as carry and compound it frame after frame (a runaway to the cap). So while the servo walks him, a ground
        // sample parallel to the held velocity (within ~18 deg) and not faster than it is that echo, not a platform:
        // dropped (a platform moving exactly along his walk loses its carry while he walks - GUESS acceptable).
        float pl = sqrtf(pre[0] * pre[0] + pre[2] * pre[2]), sv = sqrtf(g_vel[0] * g_vel[0] + g_vel[1] * g_vel[1]);
        bool echo = state == LINK_GROUND && sv > 0.3f && pl > 0.3f && h <= pl * 1.02f &&
                    hx * pre[0] + hz * pre[2] > 0.95f * h * pl;
        float cap = state == LINK_GROUND ? EXT_GROUND_MAX : EXT_MAX;
        if (echo) {
            static uint64_t echo_log;
            if (g_vel_frame_id >= echo_log + 60)
                echo_log = g_vel_frame_id, LOG("[linkpos] ext ground: game v %.2f %.2f along the held %.2f %.2f (own walk echo), not carried\n",
                                               post[0], post[2], pre[0], pre[2]);
        } else if (h > (state == LINK_GROUND ? 0.3f : 1.0f) && (state == LINK_GROUND ? g_ride : g_wind)) {
            float k = h > cap ? cap / h : 1.0f;
            e[0] = hx * k, e[2] = hz * k;
        }
        // (s88 review: the updraft only in the air - on the ground the step resets a solved fall / landing y, which is
        // no updraft, and a stale one would lift the next jump)
        if (state == LINK_AIR && dy > EXT_UP && g_wind) e[1] = dy > EXT_MAX ? EXT_MAX : dy;
    }
    static uint64_t last_log;
    bool any = e[0] != 0 || e[1] != 0 || e[2] != 0;
    if (any) {
        g_ext[0] = e[0], g_ext[1] = e[1], g_ext[2] = e[2], g_ext_frame = g_vel_frame_id;
        if (g_vel_frame_id >= last_log + 30) {
            last_log = g_vel_frame_id;
            LOG("[linkpos] ext %s: game v %.2f %.2f %.2f, held v %.2f %.2f %.2f -> carry %.2f %.2f %.2f\n",
                state == LINK_GROUND ? "ground (ride)" : "air (wind)", post[0], post[1], post[2], pre[0], pre[1], pre[2],
                e[0], e[1], e[2]);
        }
    }
}

// (s88 review) for sora_mod.dll (GetProcAddress): the ext being added now (u/s), 0 = none. The DLL's landing tests
// read an updraft's rise as "the fall was stopped" otherwise.
int linkpos_ext_get(float out[3]) {
    bool on = g_ext_frame && g_vel_frame_id <= g_ext_frame + EXT_HOLD;
    for (int i = 0; i < 3; i++) out[i] = on ? g_ext[i] : 0.0f;
    return on;
}

static void ext_add(float *v) {  // (on top of the servo's x/z, and y when the y servo owns it)
    if (g_ext_frame && g_vel_frame_id <= g_ext_frame + EXT_HOLD) {
        v[0] += g_ext[0], v[2] += g_ext[2];
        if (g_vy_on) v[1] += g_ext[1];
    }
}

static void vel_step(Cpu *c) {
    if (evel_step(c)) return;
    uint64_t head = g_vel_on ? g_vel_head : 0;
    g_vel_calls++;
    int state = head ? state_of(g_vel_actor) : LINK_NONE;
    bool drive = state == LINK_GROUND || state == LINK_AIR;
    if (!head || c->x[0] != g_vel_cc + CC_STEP_LINK_ARG) {
        g_vel_step(c);
        evel_wake();  // (s55 airhold slice 2)
        if (drive && g_link_step_frame != g_vel_frame_id) {
            float *v = (float *)(uintptr_t)(head + BODY_VEL);
            v[0] = g_vel[0], v[2] = g_vel[1];
            write_vy(v);
            ext_add(v);
            g_vel_wakes++;
        }
        return;
    }
    g_link_step_frame = g_vel_frame_id;
    g_vel_link_calls++;
    float *v = (float *)(uintptr_t)(head + BODY_VEL), pre[3] = {v[0], v[1], v[2]};
    g_vel_step(c);
    if (!drive) return;
    ext_sample(pre, v, state);
    v[0] = g_vel[0], v[2] = g_vel[1];
    write_vy(v);
    ext_add(v);
    g_vel_applied++;
}

// Recompiled functions call each other directly (gen.py: a bl into the same text is a C call), so hle_override (by
// guest address, in rt_dispatch) only sees indirect calls. hook_calls also rewrites the direct ones: every call/jmp
// rel32 to `target`'s host code inside the host code of the guest function `caller` goes to `fn` instead, through a
// thunk allocated within +-2 GB of swgame.dll (jmp [rip]; address). fn calls the original itself.
static bool host_range(uint64_t guest, uint64_t *lo, uint64_t *hi);  // (watch section)
static uint64_t enemy_nearest(void);  // (enemies section)

static int hook_calls(uint64_t caller, uint64_t target, HostFn fn) {
    uint64_t lo, hi, to = (uint64_t)(uintptr_t)dispatch_lookup(target);
    if (!to || !host_range(caller, &lo, &hi)) return 0;
    static uint8_t *thunks;
    static int nthunks;
    if (!thunks)  // a page within reach of the DLL's rel32 calls
        for (uint64_t at = (lo - 0x10000) & ~0xffffull; at > lo - 0x7f000000 && !thunks; at -= 0x10000)
            thunks = VirtualAlloc((void *)(uintptr_t)at, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!thunks || nthunks >= 0x1000 / 16) return 0;
    uint8_t *t = thunks + 16 * nthunks++;
    t[0] = 0xff, t[1] = 0x25, memset(t + 2, 0, 4);  // jmp [rip+0]
    uint64_t dst = (uint64_t)(uintptr_t)fn;
    memcpy(t + 6, &dst, 8);
    int n = 0;
    for (uint64_t p = lo; p + 5 <= hi; p++) {
        uint8_t op = *(const uint8_t *)(uintptr_t)p;
        if (op != 0xe8 && op != 0xe9) continue;
        int32_t rel;
        memcpy(&rel, (const void *)(uintptr_t)(p + 1), 4);
        if (p + 5 + (int64_t)rel != to) continue;
        int32_t nrel = (int32_t)((int64_t)(uintptr_t)t - (int64_t)(p + 5));
        DWORD old;
        VirtualProtect((void *)(uintptr_t)(p + 1), 4, PAGE_EXECUTE_READWRITE, &old);
        memcpy((void *)(uintptr_t)(p + 1), &nrel, 4);
        VirtualProtect((void *)(uintptr_t)(p + 1), 4, old, &old);
        n++;
    }
    FlushInstructionCache(GetCurrentProcess(), (void *)(uintptr_t)lo, (size_t)(hi - lo));
    LOG("[linkpos] hook: %d direct calls f_%llx -> f_%llx (host %llx..%llx) now go to the hook\n", n,
        (unsigned long long)caller, (unsigned long long)target, (unsigned long long)lo, (unsigned long long)hi);
    return n;
}

#define CC_VEL_CALLER (SS->cc_vel_caller)  // calls CC_VEL_STEP (watch1: f_70011c3a98+0x404)

static void opacity_init(void);
static void cam_hook_init(void);  // (s137b: the FPS camera's LookAtCamera::doUpdateMatrix hook, camera section)
static void dmg_init(void);
static void die_init(void);
static void redir_init(void);
static void sw_api_init(void);   // (s175 idea 1: runtime/linkpos_api_block.inc, the DLL's API table)
static void *sw_tap_get(int tap);
static void esync_init(void);   // (s168 esync: the exe side of "enemies act the same on every game", linkpos_esync_block.inc)
static void esync_frame(void);

void linkpos_init(void) {  // (main.c, after dispatch_init: before any guest code runs, so no dispatch cache holds it)
    // (S61 port) the game version's row of sora_sites.h; none (another version) or SAVAGEWIND_SORA_HOOKS=0 (the
    // safety boot): no hook, no guest call, no address read - g_ss stays zero and linkpos_frame does nothing
    static const struct { const char *ver; SoraSites s; } rows[] = {
#define SORA_SITE_A(n, a, b) a,
#define SORA_SITE_B(n, a, b) b,
        {"1.8.2", {SORA_SITES(SORA_SITE_A)}},
        {"1.9.0", {SORA_SITES(SORA_SITE_B)}},
#undef SORA_SITE_A
#undef SORA_SITE_B
    };
    memset(&g_ss, 0, sizeof g_ss);  // (an in-place reboot runs boot again)
    const char *hk = getenv("SAVAGEWIND_SORA_HOOKS");
    if (hk && *hk == '0') { LOG("[linkpos] SAVAGEWIND_SORA_HOOKS=0: no hooks, no Sora (game version %s)\n", g_game_version); return; }
    for (size_t i = 0; i < sizeof rows / sizeof *rows; i++)
        if (!strcmp(rows[i].ver, g_game_version)) g_ss = rows[i].s;
    if (!g_ss.player_info) { LOG("[linkpos] game version '%s' has no sora_sites.h row: no hooks, no Sora\n", g_game_version); return; }
    LOG("[linkpos] sites for game version %s (player_info %llx, cc_vel_step %llx)\n", g_game_version,
        (unsigned long long)g_ss.player_info, (unsigned long long)g_ss.cc_vel_step);
    opacity_init();
    cam_hook_init();  // (s137b: before the first guest frame, so the dispatch cache never holds the original)
    dmg_init();
    die_init();  // (s55 levels: enemy deaths for sora.c's kill credit)
    redir_init();  // (s56 party combat slice 2: enemies sent at Donald / Goofy)
    sw_api_init();  // (s175: the hook stubs; before any guest code)
    esync_init();  // (s168 enemy sync, SAVAGEWIND_LINK_ESYNC=1; after redir_init: it uses the AI tree defines)
    if (getenv("SAVAGEWIND_LINK_NO_VEL")) return;
    g_no_step = getenv("SAVAGEWIND_LINK_NO_STEP") != NULL;
    g_test_input = getenv("SAVAGEWIND_LINK_VEL_INPUT") != NULL;
    g_autocontinue = getenv("SAVAGEWIND_LINK_AUTOCONTINUE") != NULL;
    g_game_input_filter = link_input_filter;
    if (!(g_vel_step = dispatch_lookup(CC_VEL_STEP))) { LOG("[linkpos] no function at %llx: no velocity servo\n", CC_VEL_STEP); return; }
    hle_override(CC_VEL_STEP, vel_step, "linkpos velocity step");  // (indirect calls)
    hook_calls(CC_VEL_CALLER, CC_VEL_STEP, vel_step);
}

static void vel_frame(void) {
    static const char *spec;
    static int init, active = -1, rel;
    if (!init) {
        init = 1, spec = getenv("SAVAGEWIND_LINK_VEL");
        rel = spec && *spec == '+';  // "+<from>-<to>:...": frames after Link is in the world (g_t0)
        if (rel) spec++;
    }
    g_vel_head = servo_body(), g_vel_actor = g_servo_actor, g_vel_cc = g_servo_cc;
    g_vel_frame_id++;
    evel_frame();  // (s55 airhold slice 2)
    if (g_vel_head) step_frame(g_vel_head, state_of(g_vel_actor));
    if (g_step.kick) g_vel_kicks++;
    extern uint64_t vk_frame_counter(void);
    uint64_t present = vk_frame_counter();
    if (!g_t0 && g_vel_head && state_of(g_vel_actor) == LINK_GROUND) {
        g_t0 = present;
        LOG("[linkpos] Link in the world at presented frame %llu\n", (unsigned long long)present);
    }
    if (!spec || !*spec) return;
    if (present / 60 != (present - 1) / 60 && present > 2000)
        LOG("[linkpos] velocity step: %llu calls, %llu Link's, %llu applied, %llu kicks, %llu wakes, state %#x (presented %llu)\n",
            (unsigned long long)g_vel_calls, (unsigned long long)g_vel_link_calls, (unsigned long long)g_vel_applied,
            (unsigned long long)g_vel_kicks, (unsigned long long)g_vel_wakes, servo_body() ? *(const uint32_t *)(uintptr_t)(g_servo_actor + ACTOR_STATE) : 0,
            (unsigned long long)present);
    static uint64_t done;  // windows ended by their stop x (bit per window)
    int idx = 0, now = -1;
    float v[2] = {0, 0};
    for (const char *w = spec; *w && idx < 64; idx++) {
        unsigned long long from, to;
        float a, b, stop;
        int n = sscanf(w, "%llu-%llu:%f,%f,%f", &from, &to, &a, &b, &stop);
        if (rel) from += g_t0, to += g_t0;
        if (n >= 4 && (!rel || g_t0) && present >= from && present < to && !(done >> idx & 1)) {
            uint64_t head = servo_body();
            bool along_z = fabsf(b) > fabsf(a);  // (the stop is on the axis it mostly moves along)
            float x = head ? ((const float *)(uintptr_t)(head + BODY_COM1))[along_z ? 2 : 0] : 0, sgn = along_z ? b : a;
            if (n == 5 && head && (sgn > 0 ? x >= stop : x <= stop)) {
                done |= 1ull << idx;
                LOG("[linkpos] velocity window %d reached its stop %c at %.3f (presented frame %llu)\n", idx,
                    along_z ? 'z' : 'x', x, (unsigned long long)present);
            } else now = idx, v[0] = a, v[1] = b;
        }
        const char *semi = strchr(w, ';');
        if (!semi) break;
        w = semi + 1;
    }
    linkpos_set_velocity(now >= 0, v[0], v[1]);
    if (now != active) { LOG("[linkpos] velocity window %d at presented frame %llu\n", now, (unsigned long long)present); active = now; }
}

// ------------------------------------------------------------------ watch: who writes the body (step 2f)
// SAVAGEWIND_LINK_WATCH=<presented>:<frames>:<off>[,<off>...]  (up to 4 body head offsets, e.g. 0x90,0x20)
// At that presented frame, hardware write watchpoints (DR0..3, 4 bytes) go on every guest thread for <frames> game
// frames; a vectored handler records each write (thread, frame, value after, unwound host stack) and they are listed in
// <probe prefix>.watch.txt as swgame.dll offsets (`nm swgame.dll` -> f_<guest address>; tools/watch_names.py).
#define WATCH_MAX 8192
#define WATCH_DEPTH 10
#ifdef _WIN32  // (Deck Linux port: Windows only, debug registers + vectored handler; Linux: watch_frame below logs it)
typedef struct { uint32_t tid, dr; uint64_t frame, rip[WATCH_DEPTH]; float val; int32_t ival; } WatchHit;
static WatchHit g_whits[WATCH_MAX];
static volatile LONG g_nwhits;
static uint64_t g_waddr[4], g_wframe;
static int g_nwatch;

static LONG CALLBACK watch_handler(EXCEPTION_POINTERS *ep) {
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) return EXCEPTION_CONTINUE_SEARCH;
    CONTEXT *c = ep->ContextRecord;
    DWORD64 dr6 = c->Dr6;
    if (!(dr6 & 0xf)) return EXCEPTION_CONTINUE_SEARCH;
    c->Dr6 = 0;
    LONG i = InterlockedIncrement(&g_nwhits) - 1;
    if (i >= WATCH_MAX) return EXCEPTION_CONTINUE_EXECUTION;
    WatchHit *h = &g_whits[i];
    h->tid = GetCurrentThreadId(), h->frame = g_wframe;
    h->dr = dr6 & 1 ? 0 : dr6 & 2 ? 1 : dr6 & 4 ? 2 : 3;
    h->val = *(const float *)(uintptr_t)g_waddr[h->dr], h->ival = *(const int32_t *)(uintptr_t)g_waddr[h->dr];
    CONTEXT u = *c;
    for (int d = 0; d < WATCH_DEPTH; d++) {  // (as the sampler: x64 unwind data, a leaf's return address on the stack)
        h->rip[d] = u.Rip;
        if (!u.Rip) break;
        DWORD64 base = 0;
        PRUNTIME_FUNCTION f = RtlLookupFunctionEntry(u.Rip, &base, NULL);
        if (!f) {
            if (!u.Rsp || IsBadReadPtr((void *)u.Rsp, 8)) { if (d + 1 < WATCH_DEPTH) h->rip[d + 1] = 0; break; }
            u.Rip = *(DWORD64 *)u.Rsp, u.Rsp += 8;
            continue;
        }
        void *hd = NULL;
        DWORD64 fr = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, u.Rip, f, &u, &hd, &fr, NULL);
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}

// Sets (arm) or clears the debug registers of every guest thread. Runs on its own thread: the caller (the game's main
// thread, waiting for it) is one of the threads it suspends.
static DWORD WINAPI watch_set(void *arm) {
    extern int kernel_running_threads(pthread_t *host, uint32_t *handle, uint64_t *entry, int max);
    pthread_t host[128];
    uint32_t handle[128];
    uint64_t entry[128];
    int n = kernel_running_threads(host, handle, entry, 128), done = 0;
    for (int i = 0; i < n; i++) {
        HANDLE t = (HANDLE)pthread_gethandle(host[i]);
        if (!t || SuspendThread(t) == (DWORD)-1) continue;
        CONTEXT c;
        memset(&c, 0, sizeof c);
        c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (GetThreadContext(t, &c)) {
            DWORD64 dr7 = 0;
            if (arm) {
                DWORD64 *dr[4] = {&c.Dr0, &c.Dr1, &c.Dr2, &c.Dr3};
                for (int k = 0; k < g_nwatch; k++)  // enable k, R/W = 01 (write), LEN = 11 (4 bytes)
                    *dr[k] = g_waddr[k], dr7 |= (1ull << (2 * k)) | (1ull << (16 + 4 * k)) | (3ull << (18 + 4 * k));
            }
            c.Dr7 = dr7, c.Dr6 = 0;
            if (SetThreadContext(t, &c)) done++;
        }
        ResumeThread(t);
    }
    LOG("[linkpos] watch %s on %d of %d guest threads\n", arm ? "armed" : "cleared", done, n);
    return 0;
}

static void watch_run(void *arm) {
    HANDLE t = CreateThread(NULL, 0, watch_set, arm, 0, NULL);
    if (t) WaitForSingleObject(t, INFINITE), CloseHandle(t);
}
#endif  // _WIN32 (watch)

// Host address -> "f_<guest address>+<off>" from the game library's tables (the DLL is stripped: no nm symbols).
typedef struct { uint64_t host, guest; } FnPair;
static FnPair *g_fnmap;
static size_t g_nfn;

static int fnpair_cmp(const void *a, const void *b) {
    uint64_t x = ((const FnPair *)a)->host, y = ((const FnPair *)b)->host;
    return x < y ? -1 : x > y;
}

static void fnmap_build(void) {
    if (g_fnmap) return;
    const GameLibTables *t = gamelib_tables();
    size_t total = 0;
    for (uint32_t m = 0; t && m < *t->nmodules; m++) total += *t->modules[m].count;
    g_fnmap = total ? malloc(total * sizeof *g_fnmap) : NULL;
    for (uint32_t m = 0; g_fnmap && m < *t->nmodules; m++)
        for (uint64_t i = 0; i < *t->modules[m].count; i++)
            g_fnmap[g_nfn++] = (FnPair){(uint64_t)(uintptr_t)t->modules[m].fns[i], t->modules[m].addrs[i]};
    if (g_fnmap) qsort(g_fnmap, g_nfn, sizeof *g_fnmap, fnpair_cmp);
}

// Host code of a guest function: from its entry to the next function's (an upper bound: hook_calls only rewrites
// exact rel32 hits on one target inside it).
static bool host_range(uint64_t guest, uint64_t *lo, uint64_t *hi) {
    fnmap_build();
    uint64_t at = (uint64_t)(uintptr_t)dispatch_lookup(guest);
    for (size_t i = 0; at && i < g_nfn; i++)
        if (g_fnmap[i].host == at) { *lo = at, *hi = i + 1 < g_nfn ? g_fnmap[i + 1].host : at + 0x10000; return true; }
    return false;
}

static void fn_name(uint64_t rip, char *out, size_t n) {
    fnmap_build();
    size_t lo = 0, hi = g_nfn;  // the last function starting at or before rip
    while (lo < hi) { size_t mid = (lo + hi) / 2; if (g_fnmap[mid].host <= rip) lo = mid + 1; else hi = mid; }
    uint64_t dll = (uint64_t)(uintptr_t)GetModuleHandleA("swgame.dll");
    if (lo && dll && rip >= dll && rip - g_fnmap[lo - 1].host < 0x100000)
        snprintf(out, n, "f_%llx+%llx", (unsigned long long)g_fnmap[lo - 1].guest, (unsigned long long)(rip - g_fnmap[lo - 1].host));
    else
        snprintf(out, n, "%llx", (unsigned long long)rip);
}

#ifndef _WIN32
static void watch_frame(uint64_t frame) {
    static int warned;
    (void)frame;
    if (!warned++ && getenv("SAVAGEWIND_LINK_WATCH")) LOG("[linkpos] SAVAGEWIND_LINK_WATCH: Windows only (debug registers), ignored\n");
}
#else
static void watch_frame(uint64_t frame) {
    static const char *spec;
    static int init, state;  // 0 waiting, 1 armed, 2 done
    static unsigned long long at, frames, until;
    static char offs[128];
    if (!init) {
        init = 1;
        spec = getenv("SAVAGEWIND_LINK_WATCH");
        if (spec && sscanf(spec, "%llu:%llu:%127s", &at, &frames, offs) != 3) LOG("[linkpos] bad SAVAGEWIND_LINK_WATCH\n"), spec = NULL;
    }
    if (!spec || state == 2) return;
    g_wframe = frame;
    extern uint64_t vk_frame_counter(void);
    if (state == 0) {
        if (vk_frame_counter() < at) return;
        uint64_t head = servo_body();
        if (!head) return;
        g_nwatch = 0;
        for (char *s = offs, *e; *s && g_nwatch < 4; s = *e == ',' ? e + 1 : e) {
            uint64_t base = head;
            if (*s == 'e') base = enemy_nearest(), s++;  // e<off>: the nearest enemy actor + off (step 5)
            uint64_t off = strtoull(s, &e, 0);
            if (e == s || !base) break;
            g_waddr[g_nwatch++] = base + off;
        }
        AddVectoredExceptionHandler(1, watch_handler);
        for (int k = 0; k < g_nwatch; k++) LOG("[linkpos] watch %d: %llx (head %llx)\n", k, (unsigned long long)g_waddr[k], (unsigned long long)head);
        watch_run((void *)1);
        until = frame + frames, state = 1;
        return;
    }
    if (frame < until) return;
    watch_run(NULL);
    state = 2;
    char path[512];
    const char *prefix = getenv("SAVAGEWIND_LINK_PROBE");
    snprintf(path, sizeof path, "%s.watch.txt", prefix && *prefix ? prefix : "build/linkpos");
    FILE *f = fopen(path, "w");
    LONG n = g_nwhits < WATCH_MAX ? g_nwhits : WATCH_MAX;
    LOG("[linkpos] watch: %ld writes (%ld kept) -> %s\n", (long)g_nwhits, (long)n, path);
    if (!f) return;
    uint64_t dll = (uint64_t)(uintptr_t)GetModuleHandleA("swgame.dll"), exe = (uint64_t)(uintptr_t)GetModuleHandleA(NULL);
    fprintf(f, "# M swgame %llx exe %llx; watches", (unsigned long long)dll, (unsigned long long)exe);
    for (int k = 0; k < g_nwatch; k++) fprintf(f, " %d=%llx", k, (unsigned long long)g_waddr[k]);
    fprintf(f, "\n# frame tid dr value | host stack (game code as f_<guest function>+<host offset>)\n");
    for (LONG i = 0; i < n; i++) {
        WatchHit *h = &g_whits[i];
        fprintf(f, "%llu %u %u %g (int %d) |", (unsigned long long)h->frame, h->tid, h->dr, h->val, h->ival);
        char name[64];
        for (int d = 0; d < WATCH_DEPTH && h->rip[d]; d++) fn_name(h->rip[d], name, sizeof name), fprintf(f, " %s", name);
        fprintf(f, "\n");
    }
    fclose(f);
}
#endif  // _WIN32 (watch_frame)

// ------------------------------------------------------------------ getters for the Sora driver (step 3)
// Game thread only (they run servo_body's caches).
//   linkpos_link_pos(p)          Link's body position (CoM1, the physics authority), false without a body
//   linkpos_link_state_word()    actor +0xcec as is (0 without an actor), for logs
//   linkpos_hide_link(on)        actor +0x4f0 (final opacity) = 0 every frame while on (session 3: hides him, no fade)
//   linkpos_camera_fwd(f)        the camera's horizontal forward (unit x, z), false until it is found (below)
bool linkpos_link_pos(float p[3]) {
    uint64_t head = servo_body();
    if (!head) return false;
    memcpy(p, (const void *)(uintptr_t)(head + BODY_COM1), 12);
    return true;
}

uint32_t linkpos_link_state_word(void) {
    return servo_body() ? *(const uint32_t *)(uintptr_t)(g_servo_actor + ACTOR_STATE) : 0;
}

#define ACTOR_FINAL_OPACITY 0x4f0
static bool g_hide;

void linkpos_hide_link(bool on) { g_hide = on; }

// session 11 (recon/FAINT_LINK_RECON.md, from the 1.8.2 code): the drawn alpha is +0x4f0 * +0x4ec * +0x4e8, pushed to
// the model only when the dirty byte +0x68e is set; +0x4f0 is a request Link's own code steps back up (~0.057 a
// frame: the faint see-through Link). +0x4ec = 0 zeroes the product whatever he requests; it ramps back to 1 by
// itself once we stop writing.
#define ACTOR_START_OPACITY 0x4ec
#define ACTOR_OPACITY_DIRTY 0x68e
static volatile uint64_t g_opacity_hits, g_opacity_calls;  // (fix 2 below)

static void hide_frame(void) {
    if (!g_hide || !servo_body()) return;
    *(float *)(uintptr_t)(g_servo_actor + ACTOR_FINAL_OPACITY) = 0;
    *(float *)(uintptr_t)(g_servo_actor + ACTOR_START_OPACITY) = 0;
    *(uint8_t *)(uintptr_t)(g_servo_actor + ACTOR_OPACITY_DIRTY) = 1;
    static unsigned n;
    if (++n % 600 == 0) LOG("[linkpos] opacity apply: %llu calls, %llu on Link\n", (unsigned long long)g_opacity_calls, (unsigned long long)g_opacity_hits);
}

// Fix 2 (same report): hide_frame writes before the actor calc; Link's code can still request opacity after it.
// applyOpacityIfDirty (f_7001412f68, x0 = actor) runs after every request of the frame, right before the model gets
// the alpha: zero the factors there too. It has three direct callers (not one as the report said; hideb_dbg: the
// one in f_700068bd2c, a vtable-only override, never saw Link): the bl sites below, hooked in their functions.
//   SAVAGEWIND_LINK_NO_OPACITY_HOOK=1   leave it out (A/B against hide_frame alone)
#define OPACITY_APPLY (SS->opacity_apply)
#define opacity_sites ((const uint64_t[]){SS->opacity_call_a, SS->opacity_call_b, SS->opacity_call_c})
static GuestFn g_opacity_apply;

static uint64_t guest_fn_at(uint64_t guest) {  // the recompiled function containing a guest address (0: none)
    fnmap_build();
    uint64_t best = 0;
    for (size_t i = 0; i < g_nfn; i++)
        if (g_fnmap[i].guest <= guest && g_fnmap[i].guest > best) best = g_fnmap[i].guest;
    return best;
}

// (d) Link's equipment: weapons, sheath, armor pieces and the Sheikah Slate are actors of their own (the same opacity
// fields), which Link's model opacity doesn't reach (FAINT_LINK_RECON.md "Equipment"; session 13 near-log: the branch
// Weapon_Sword_044, its sheath Weapon_Lsheath_056, Armor_Default_Head/Upper/Lower/Extra_00/01, Item_Conductor = the
// slate on his right hip). Link's code passes his requested opacity on to them (+0x4f0 = 0), but with +0x4ec = 1
// they still wrote the scene depth (equipC_dbg_off: the held branch cut a strip out of Sora, equipE: the slate a
// rectangle at his hip); the same zero writes as Link's fix that (their actor flag bit 33 is clear: dither path).
// Which actors are his: they hold BaseProcLinks to him, i.e. Link's BaseProcLinkData pointer (BaseProc +0x68; the
// branch at +0x880/+0x938/+0xa08, the sheath at +0x840/+0x8d8; no Weapon_* lying in the world has it; Link holds the
// branch's at +0xb98). Only names in equip_prefix and only within 4 units (an enemy fighting Link may link to him
// too). The offset is found once per actor (first 0x1000 bytes; again every 60 applies while it has none, for
// something he picks up later) and checked on every apply, so a weapon he drops shows again.
//   SAVAGEWIND_LINK_EQUIP_LOG=1   log each candidate once (name, distance, opacity factors, where the two linkdata
//                                 pointers sit), and its factors again every 600 applies
//   SAVAGEWIND_LINK_NEAR_LOG=1    log every actor the apply sees within 2.5 units of Link once (any name)
//   SAVAGEWIND_LINK_NO_EQUIP_HIDE=1   log only (A/B)
#define BASEPROC_NAME 0x10  // sead::SafeString mName (BaseProc +0x8): its mStringTop
#define BASEPROC_LINKDATA 0x68
#define EQUIP_MAX 64
#define EQUIP_SCAN 0x1000
#define EQUIP_DIST 4.0f
static const char *const equip_prefix[] = {"Weapon_", "Armor_", "Item_Conductor"};
static struct { uint64_t actor; int link_off; unsigned n; size_t bytes; char armor[40]; DWORD seen; } g_equip[EQUIP_MAX];  // (s120 armor: name copy + last match tick)
static int g_nequip;
static SRWLOCK g_equip_lock = SRWLOCK_INIT;
static bool g_no_equip_hide;

static int find_u64(uint64_t base, size_t n, uint64_t v, char *list, size_t cap) {  // first offset (-1), all in list
    int first = -1;
    if (list) *list = 0;
    for (size_t o = 0; o + 8 <= n; o += 8)
        if (*(const uint64_t *)(uintptr_t)(base + o) == v) {
            if (first < 0) first = (int)o;
            if (list && strlen(list) + 12 < cap) sprintf(list + strlen(list), " +%#zx", o);
        }
    return first;
}

static float link_dist(uint64_t a) {
    uint64_t sv = g_servo_actor;  // (s110 crash fix: one read; a save load zeroes it on another thread)
    if (!sv) return 1e9f;
    const float *m = (const float *)(uintptr_t)(a + ACTOR_MTX), *l = (const float *)(uintptr_t)(sv + ACTOR_MTX);
    float dx = m[3] - l[3], dy = m[7] - l[7], dz = m[11] - l[11];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

static const char *actor_name(uint64_t a) {
    uint64_t name = *(const uint64_t *)(uintptr_t)(a + BASEPROC_NAME);
    return in_guest(name) ? (const char *)(uintptr_t)name : NULL;
}

static float opac(uint64_t a, int off) { return *(const float *)(uintptr_t)(a + off); }

static void near_log(uint64_t a, const char *name) {  // (SAVAGEWIND_LINK_NEAR_LOG)
    static uint64_t seen[64];
    static int nseen;
    AcquireSRWLockExclusive(&g_equip_lock);
    int k;
    for (k = 0; k < nseen && seen[k] != a; k++) {}
    if (k == nseen && nseen < 64) {
        seen[nseen++] = a;
        LOG("[linkpos] near: %llx %s dist %.2f opac %.3f %.3f %.3f\n", (unsigned long long)a, name, link_dist(a),
            opac(a, ACTOR_OPACITY), opac(a, ACTOR_START_OPACITY), opac(a, ACTOR_FINAL_OPACITY));
    }
    ReleaseSRWLockExclusive(&g_equip_lock);
}

static int equip_scan(uint64_t a, size_t bytes, bool first) {  // (under the lock)
    uint64_t sv = g_servo_actor;  // (s110 crash fix: a save load zeroes it on another thread)
    if (!sv) return -1;
    uint64_t ld = *(const uint64_t *)(uintptr_t)(sv + BASEPROC_LINKDATA);
    if (!in_guest(ld)) return -1;
    char in_it[160], in_link[160];
    int off = find_u64(a, bytes, ld, in_it, sizeof in_it);
    if (!first || !env_int("SAVAGEWIND_LINK_EQUIP_LOG", 0)) return off;
    find_u64(sv, 0x4000, *(const uint64_t *)(uintptr_t)(a + BASEPROC_LINKDATA), in_link, sizeof in_link);
    LOG("[linkpos] equip: %llx %s dist %.2f opac %.3f %.3f %.3f | Link's linkdata in it:%s | its in Link:%s\n",
        (unsigned long long)a, actor_name(a), link_dist(a), opac(a, ACTOR_OPACITY), opac(a, ACTOR_START_OPACITY),
        opac(a, ACTOR_FINAL_OPACITY), in_it[0] ? in_it : " -", in_link[0] ? in_link : " -");
    return off;
}

static bool is_link_equip(uint64_t a) {
    const char *name = actor_name(a);
    if (!name || link_dist(a) >= EQUIP_DIST) return false;
    static int near_on = -1;
    if (near_on < 0) near_on = env_int("SAVAGEWIND_LINK_NEAR_LOG", 0);
    if (near_on && link_dist(a) < 2.5f) near_log(a, name);
    size_t p;
    for (p = 0; p < sizeof equip_prefix / sizeof *equip_prefix; p++)
        if (!strncmp(name, equip_prefix[p], strlen(equip_prefix[p]))) break;
    if (p == sizeof equip_prefix / sizeof *equip_prefix) return false;
    int i;
    AcquireSRWLockExclusive(&g_equip_lock);
    for (i = 0; i < g_nequip && g_equip[i].actor != a; i++) {}
    if (i == g_nequip) {
        if (g_nequip == EQUIP_MAX) memmove(g_equip, g_equip + 1, sizeof *g_equip * --g_nequip), i--;  // (oldest out)
        g_equip[i].actor = a, g_equip[i].n = 0, g_equip[i].armor[0] = 0, g_equip[i].seen = 0, g_nequip++;
        g_equip[i].bytes = readable(a, EQUIP_SCAN);  // (VirtualQuery: once per actor)
        g_equip[i].link_off = equip_scan(a, g_equip[i].bytes, true);
    } else if (g_equip[i].link_off < 0 && g_equip[i].n % 60 == 0) {
        g_equip[i].link_off = equip_scan(a, g_equip[i].bytes, false);
    }
    int off = g_equip[i].link_off;
    if (++g_equip[i].n % 600 == 0 && env_int("SAVAGEWIND_LINK_EQUIP_LOG", 0))
        LOG("[linkpos] equip: %llx %s applies %u, link_off %#x, dist %.2f, opac %.3f %.3f %.3f\n", (unsigned long long)a,
            name, g_equip[i].n, off, link_dist(a), opac(a, ACTOR_OPACITY), opac(a, ACTOR_START_OPACITY),
            opac(a, ACTOR_FINAL_OPACITY));
    // (s120 armor) a worn Armor_ piece: keep a copy of its name (the actor can be freed when Link changes armor)
    uint64_t sv0 = g_servo_actor;
    if (p == 1 && off >= 0 && sv0 && *(const uint64_t *)(uintptr_t)(a + off) == *(const uint64_t *)(uintptr_t)(sv0 + BASEPROC_LINKDATA)) {
        snprintf(g_equip[i].armor, sizeof g_equip[i].armor, "%s", name);
        g_equip[i].seen = GetTickCount();
    }
    ReleaseSRWLockExclusive(&g_equip_lock);
    // (s110 crash fix, s110 last41.log: c0000005 at 0x68 = g_servo_actor 0 + BASEPROC_LINKDATA during a save load's
    // Demo025: the opacity hook runs on BotW's draw threads while the load clears Link's actor) one read, checked
    uint64_t sv = g_servo_actor;
    return off >= 0 && sv && *(const uint64_t *)(uintptr_t)(a + off) == *(const uint64_t *)(uintptr_t)(sv + BASEPROC_LINKDATA);
}

// ------------------------------------------------------------------ (s94 b8-slatehip) the Sheikah Slate on Sora's hip
// The Slate (Item_Conductor, Link's right hip) is hidden with the rest of Link's gear (is_link_equip above). Link's skeleton
// is still the thing Sora is drawn on (feet = his mMtx), so the Slate's own hip attach already lands near Sora's hip:
// showing it again is writing its opacity back (START / FINAL = 1, dirty) instead of zero_opacity. sora_mod.dll calls
// linkpos_slate_hip(show, snap, p) each present (looked up by name: NEEDS EXE RELINK, an older exe has no symbol).
//   show  1 = the Slate is drawn while Link is hidden; 0 = hidden as before. Honoured only for 30 game frames after the
//         call (the DLL stopped / unloaded -> hidden again).
//   snap  1 = the Slate's mMtx is rewritten each apply to Link's mMtx * (p[0..2] offset, p[3..5] yaw/pitch/roll deg)
//         [GUESS: the model follows mMtx rather than a bone chain; if it does not, only snap 0 does anything].
//         0 = the game's own attach (the Slate where BotW puts it on Link).
static uint64_t g_slm_actor;  // (s100) the Slate drawn last (slate_hip_apply), below: its model hook
static volatile int g_slate_show, g_slate_snap;
static volatile uint64_t g_slate_frame;
static float g_slate_p[6];
static unsigned g_slate_shown, g_slate_snapped;
void linkpos_slate_hip(int show, int snap, const float p[6]) {
    g_slate_show = show, g_slate_snap = snap, g_slate_frame = g_vel_frame_id;
    if (p) memcpy(g_slate_p, p, sizeof g_slate_p);
}
static void slate_hip_apply(uint64_t a) {  // the Slate shown (called instead of zero_opacity)
    {   // (s106 slatefade) rune mode: the Slate in Sora's hand is see-through although 0x4f0 / 0x4ec were written 1. The drawn
        // alpha is 0x4f0 * 0x4ec * 0x4e8 (FAINT_LINK_RECON.md) and 0x4e8 (the fade factor) was never written: log the three
        // factors as they come in (change-only, 0.05 steps) and write 0x4e8 = 1 too. SAVAGEWIND_SLATE_FADE_KEEP=1 = old.
        static int keep = -1, nlog;
        static int q_last[3] = {-1, -1, -1};
        if (keep < 0) keep = env_int("SAVAGEWIND_SLATE_FADE_KEEP", 0) != 0;
        float f8 = opac(a, ACTOR_OPACITY), fc = opac(a, ACTOR_START_OPACITY), f0 = opac(a, ACTOR_FINAL_OPACITY);
        int q[3] = {(int)(f8 * 20 + 0.5f), (int)(fc * 20 + 0.5f), (int)(f0 * 20 + 0.5f)};
        if ((q[0] != q_last[0] || q[1] != q_last[1] || q[2] != q_last[2]) && nlog < 60) {
            nlog++;
            memcpy(q_last, q, sizeof q);
            uint64_t mdl = *(const uint64_t *)(uintptr_t)(a + 0x4e0);
            LOG("[linkpos] slatefade: Item_Conductor %llx in: fade(4e8) %.3f start(4ec) %.3f req(4f0) %.3f flags524 %#x model %llx, Link state %#x%s\n",
                (unsigned long long)a, f8, fc, f0, *(const uint32_t *)(uintptr_t)(a + 0x524), (unsigned long long)mdl,
                linkpos_link_state_word(), keep ? " (fade kept)" : "");
        }
        if (!keep) *(float *)(uintptr_t)(a + ACTOR_OPACITY) = 1;
    }
    *(float *)(uintptr_t)(a + ACTOR_FINAL_OPACITY) = 1;
    *(float *)(uintptr_t)(a + ACTOR_START_OPACITY) = 1;
    *(uint8_t *)(uintptr_t)(a + ACTOR_OPACITY_DIRTY) = 1;
    g_slm_actor = a;  // (s100 slatehip2)
    if (g_slate_shown++ == 0) LOG("[linkpos] slatehip: Item_Conductor %llx shown (opacity back to 1)\n", (unsigned long long)a);
    uint64_t sv = g_servo_actor;  // (s110 crash fix)
    if (!g_slate_snap || !sv) return;
    float *m = (float *)(uintptr_t)(a + ACTOR_MTX);
    const float *l = (const float *)(uintptr_t)(sv + ACTOR_MTX);
    const float *p = g_slate_p;
    float bef[3] = {m[3], m[7], m[11]};
    float cy = cosf(p[3] * 0.0174533f), sy = sinf(p[3] * 0.0174533f), cp = cosf(p[4] * 0.0174533f), sp = sinf(p[4] * 0.0174533f),
          cr = cosf(p[5] * 0.0174533f), sr = sinf(p[5] * 0.0174533f);
    // local rotation = Ry(yaw) * Rx(pitch) * Rz(roll); rows of the result = rows of Link's R times it
    float ry[9] = {cy, 0, sy, 0, 1, 0, -sy, 0, cy}, rx[9] = {1, 0, 0, 0, cp, -sp, 0, sp, cp}, rz[9] = {cr, -sr, 0, sr, cr, 0, 0, 0, 1};
    float t1[9], t2[9], lr[9], nr[9];
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) {
        t1[i * 3 + j] = ry[i * 3] * rx[j] + ry[i * 3 + 1] * rx[3 + j] + ry[i * 3 + 2] * rx[6 + j];
    }
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) {
        t2[i * 3 + j] = t1[i * 3] * rz[j] + t1[i * 3 + 1] * rz[3 + j] + t1[i * 3 + 2] * rz[6 + j];
        lr[i * 3 + j] = l[i * 4 + j];
    }
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++)
        nr[i * 3 + j] = lr[i * 3] * t2[j] + lr[i * 3 + 1] * t2[3 + j] + lr[i * 3 + 2] * t2[6 + j];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) m[i * 4 + j] = nr[i * 3 + j];
        m[i * 4 + 3] = l[i * 4 + 3] + lr[i * 3] * p[0] + lr[i * 3 + 1] * p[1] + lr[i * 3 + 2] * p[2];
    }
    if (g_slate_snapped++ < 3 || g_slate_snapped % 1800 == 0)
        LOG("[linkpos] slatehip: snap #%u: Slate pos %.2f %.2f %.2f -> %.2f %.2f %.2f (Link %.2f %.2f %.2f)\n", g_slate_snapped,
            bef[0], bef[1], bef[2], m[3], m[7], m[11], l[3], l[7], l[11]);
}
// (s100 slatehip2, recon/S100_HELPER_SLATEHIP.md) The Slate's MODEL matrix. Every Actor's calc copies its mMtx (+0x398) to the
// gsys::Model's world matrix (model+0x58, sead Matrix34f) and then calls gsys::Model::calcWorld(model) (slate_model_call ->
// slate_model_target); the bones / the draw are built from model+0x58 there. Writing mMtx in the opacity apply (above) is
// too late for that frame and the attach rewrites it next frame, so this hook rewrites model+0x58 itself, right before
// calcWorld, for the Slate's model only. Nothing is stored: the next frame's calc copies mMtx again, so when the DLL stops
// calling linkpos_slate_model (30 game frames) or the Slate is not shown the game's own attach is back.
//   linkpos_slate_model(on, w, keep_rot)   w = sead Matrix34f row-major 3x4 (columns = axes, w[3] w[7] w[11] = translation);
//   keep_rot 1: only the translation is replaced (the game's own rotation stays).
static volatile int g_slm_on, g_slm_keep;
static volatile uint64_t g_slm_frame;
static volatile uint32_t g_slm_seq;  // odd while linkpos_slate_model writes g_slm_w (calc runs on several threads)
static float g_slm_w[12];
static unsigned g_slm_hits;
static HostFn g_slm_orig;
static volatile uint32_t g_slo_seq;  // (s107 slateocc) odd while the hook copies the Slate model matrix
static float g_slo_w[12];
static volatile uint64_t g_slo_frame;
void linkpos_slate_model(int on, const float w[12], int keep_rot) {
    g_slm_seq++;
    MemoryBarrier();
    g_slm_on = on != 0 && w != NULL, g_slm_keep = keep_rot != 0, g_slm_frame = g_vel_frame_id;
    if (w) memcpy(g_slm_w, w, sizeof g_slm_w);
    MemoryBarrier();
    g_slm_seq++;
}
// (s102 bombhand, recon/S102_HELPER_BOMBHAND.md) Held Remote Bombs. Same call site as the Slate (the generic Actor calc, so every
// actor's model passes here): a Obj_RemoteBomb* actor that is "held" (GUESS: its model position is within bh_rad horizontally of
// Link's feet and 0.5..2.6 m above them; a bomb on the ground is ~0.2 m up) gets its model translation replaced by the position
// the DLL gives (linkpos_bomb_hand: Sora's hand + offset). The rotation stays the game's. Nothing is stored: when the bomb
// leaves that zone (thrown) or the DLL stops calling (30 frames) the game's matrix is used untouched.
#define BH_N 4
static struct { uint64_t actor; uint64_t seen; int held, logged; } g_bh[BH_N];
static volatile int g_bh_on;
static volatile uint64_t g_bh_frame, g_bh_held_frame;
static volatile uint32_t g_bh_seq;
static float g_bh_p[3], g_bh_rad = 1.0f;
static unsigned g_bh_hits;
void linkpos_bomb_hand(int on, const float p[3], float radius_m) {
    g_bh_seq++;
    MemoryBarrier();
    g_bh_on = on != 0 && p != NULL, g_bh_frame = g_vel_frame_id;
    if (p) memcpy(g_bh_p, p, sizeof g_bh_p);
    if (radius_m > 0.1f) g_bh_rad = radius_m;
    MemoryBarrier();
    g_bh_seq++;
}
// 0 = no Remote Bomb seen near Link lately, 1 = one is near (the DLL starts tracking Sora's hand bone), 2 = one is held now
int linkpos_bomb_state(void) {
    if (g_vel_frame_id - g_bh_held_frame <= 3) return 2;
    for (int i = 0; i < BH_N; i++)
        if (g_bh[i].actor && g_vel_frame_id - g_bh[i].seen <= 30) return 1;
    return 0;
}
static void bh_note(uint64_t a) {  // from the opacity apply (once per drawn actor per frame), Obj_RemoteBomb* only
    int free_i = -1;
    for (int i = 0; i < BH_N; i++) {
        if (g_bh[i].actor == a) { g_bh[i].seen = g_vel_frame_id; return; }
        if (free_i < 0 && (!g_bh[i].actor || g_vel_frame_id - g_bh[i].seen > 120)) free_i = i;
    }
    if (free_i >= 0) g_bh[free_i].actor = a, g_bh[free_i].seen = g_vel_frame_id, g_bh[free_i].held = 0;
}
static void bh_model(uint64_t model) {
    uint64_t sv = g_servo_actor;  // (s110 crash fix: one read)
    if (!sv || !in_guest(sv)) return;
    for (int i = 0; i < BH_N; i++) {
        uint64_t a = g_bh[i].actor;
        if (!a || g_vel_frame_id - g_bh[i].seen > 30 || !in_guest(a) || model != *(const uint64_t *)(uintptr_t)(a + 0x4e0)) continue;
        float *m = (float *)(uintptr_t)(model + 0x58);
        const float *l = (const float *)(uintptr_t)(sv + ACTOR_MTX);
        float dx = m[3] - l[3], dz = m[11] - l[11], dy = m[7] - l[7];
        float hr = sqrtf(dx * dx + dz * dz);
        // latch: enter inside g_bh_rad * 0.7 and 0.5..2.6 m up; stay while inside g_bh_rad and 0.3..3 m up
        bool in = g_bh[i].held ? (hr < g_bh_rad && dy > 0.3f && dy < 3.0f) : (hr < g_bh_rad * 0.7f && dy > 0.5f && dy < 2.6f);
        if (in && !g_bh[i].held) {
            g_bh[i].held = 1, g_bh[i].logged = 0;
            LOG("[linkpos] bombhand: %s %llx picked up (model %.2f %.2f %.2f, Link feet %.2f %.2f %.2f)\n", actor_name(a), (unsigned long long)a,
                m[3], m[7], m[11], l[3], l[7], l[11]);
        } else if (!in && g_bh[i].held) {
            g_bh[i].held = 0;
            LOG("[linkpos] bombhand: %s %llx released (model %.2f %.2f %.2f)\n", actor_name(a), (unsigned long long)a, m[3], m[7], m[11]);
        }
        if (!g_bh[i].held) continue;
        g_bh_held_frame = g_vel_frame_id;
        if (!g_bh_on || g_vel_frame_id - g_bh_frame > 30) continue;
        float p[3];
        uint32_t q0;
        int n = 0;
        do { q0 = g_bh_seq; MemoryBarrier(); memcpy(p, g_bh_p, sizeof p); MemoryBarrier(); } while ((q0 & 1 || q0 != g_bh_seq) && ++n < 8);
        if ((q0 & 1) || q0 != g_bh_seq) continue;
        if (!g_bh[i].logged)
            g_bh[i].logged = 1, g_bh_hits++, LOG("[linkpos] bombhand: %s %llx model %.2f %.2f %.2f -> %.2f %.2f %.2f\n", actor_name(a), (unsigned long long)a,
                                               m[3], m[7], m[11], p[0], p[1], p[2]);
        m[3] = p[0], m[7] = p[1], m[11] = p[2];
        *(uint8_t *)(uintptr_t)(model + 0xa0) |= 1;
    }
}
// (s111 slatecut) The Slate model matrix the GAME computed this frame (before our rewrite above): tells the DLL whether the game has the
// Slate on Link's hip or in his hand (pedestal / pickup animations). out = sead Matrix34f row-major 3x4. False: not seen for 3 game frames.
static volatile uint32_t g_slg_seq;
static float g_slg_w[12];
static volatile uint64_t g_slg_frame;
bool linkpos_slate_model_game(float out[12]) {
    for (int n = 0; n < 8; n++) {
        uint32_t q0 = g_slg_seq;
        MemoryBarrier();
        float w[12];
        memcpy(w, g_slg_w, sizeof w);
        uint64_t fr = g_slg_frame;
        MemoryBarrier();
        if ((q0 & 1) || q0 != g_slg_seq) continue;
        if (!fr || g_vel_frame_id - fr > 3) return false;
        memcpy(out, w, sizeof w);
        return true;
    }
    return false;
}
// (s114 gearpark) Link's hidden gear (shield, weapons, bow: is_link_equip, opacity 0) still cut a hole in Roxas in the Slate
// cutscenes (marks mark_132420_1/_2, Demo008_0): while the Slate is in use the DLL keeps Link's mVisibilityMask bit 0 on (s113
// slvis), BotW copies Link's mask onto his gear every frame, and the gear at alpha 0 still lands in the scene depth Roxas is
// tested against. Fix: every hidden gear model (not the Slate) gets its world translation parked GEAR_PARK_Y below the world
// right before its calcWorld, whatever its mask: nothing of it (colour, depth, shadow) is near Link. The attach rewrites its
// matrix from Link's bone every frame (GUESS: true for gear on the body; Slate snap relies on the same input), so it is back as
// soon as Link is shown. SAVAGEWIND_LINK_GEAR_PARK=0 / DLL "set gearpark 0" = old (masks only).
#define GEAR_PARK_N 16
#define GEAR_PARK_Y (-20000.0f)
static volatile uint64_t g_gear_mdl[GEAR_PARK_N], g_gear_fr[GEAR_PARK_N];
static volatile int g_gear_park = -1;
static unsigned g_gear_parked;
void linkpos_gear_park(int on) {
    on = on < 0 ? 0 : on > 2 ? 2 : on;
    if (g_gear_park != on) LOG("[linkpos] gearpark %d: %s\n", on, on == 2 ? "Link's hidden gear moved away at draw time, always" : on ? "Link's hidden gear moved away while the Slate is shown" : "off (old: masks only)");
    g_gear_park = on;
}
static void gear_note(uint64_t a) {  // (opacity hook, any thread) a hidden gear actor's model, for slate_model_hook
    const char *n = actor_name(a);
    if (n && !strncmp(n, "Item_Conductor", 14)) return;  // (the Slate: slatecut / slateocc read its game matrix)
    uint64_t mdl = *(const uint64_t *)(uintptr_t)(a + 0x4e0);
    if (!in_guest(mdl)) return;
    int k = 0;
    for (int i = 0; i < GEAR_PARK_N; i++) {
        if (g_gear_mdl[i] == mdl) { k = i; break; }
        if (g_gear_fr[i] < g_gear_fr[k]) k = i;
    }
    g_gear_mdl[k] = mdl, g_gear_fr[k] = g_vel_frame_id;
}
static void gear_park(uint64_t model) {
    if (g_gear_park < 0) g_gear_park = env_int("SAVAGEWIND_LINK_GEAR_PARK", 1);
    if (!g_gear_park || !g_hide) return;
    // 1 (default) = only while the DLL shows the Slate (linkpos_slate_hip: the only time Link's bit 0 - and so the gear's - is
    // kept on); 2 = whenever Link is hidden (GUESS risk: anything that reads a gear model's bones, e.g. an arrow off the bow)
    if (g_gear_park == 1 && (!g_slate_show || g_vel_frame_id - g_slate_frame > 30)) return;
    for (int i = 0; i < GEAR_PARK_N; i++)
        if (g_gear_mdl[i] == model && g_vel_frame_id - g_gear_fr[i] <= 2) {
            float *m = (float *)(uintptr_t)(model + 0x58);
            if (g_gear_parked++ < 4 || g_gear_parked % 36000 == 0)
                LOG("[linkpos] gearpark: #%u model %llx at %.2f %.2f %.2f parked below the world\n", g_gear_parked,
                    (unsigned long long)model, m[3], m[7], m[11]);
            m[7] = GEAR_PARK_Y;
            *(uint8_t *)(uintptr_t)(model + 0xa0) |= 1;
            return;
        }
}
// ------------------------------------------------------------------ (s128 wpn) Link's BotW melee weapon in Sora's hand
// recon/S128_PLAN.md, S128_HELPER_EQUIP.md (where + drawing), S128_HELPER_DMG.md (wear). Three parts:
// (1) linkpos_weapon_info: Link's sword slot = PauseMenuDataMgr mEquippedWeapons[0] (mgr +0x447e0, PouchItem*): name
//     (SafeString cstr at item +0x30), life x100 (+0x20), Attack-up value (+0x80 when flags +0x88 bit 0). Plain reads.
// (2) linkpos_weapon_hand(on, off): the DLL wants it in Sora's hand (fresh <= WPN_FRESH game frames). The opacity hook then
//     shows that one actor (opacity 1 like the Slate, not parked) and the calcWorld hook writes its model matrix from Sora's
//     Keyblade grip (sora_draw_grip_world, bone 228): weapon +X = grip X, +Y = grip Z, +Z (the blade) = grip -Y, origin pulled
//     back along the blade by off[0] metres, then turned by off[1..3] deg (roll about the blade, pitch, yaw: GUESS keys).
//     linkpos_weapon_shown() = it really happened lately -> sora_draw.c hides the Keyblade groups.
// (3) linkpos_weapon_wear(n): n durability points (hits) queued; the weapon damage manager's own per-frame zero call
//     (a2 3, damage 0, f54 -1) is turned into BotW's real wear call addDamage(dm, 1, 100 n, 0, 0, 5, 15, 1) (probe
//     hit3.run.log, lr 70006a306c): BotW then does the warning, the break and the pouch sync itself.
#define WPN_FRESH 4
#define WPN_SLOTS_OFF 0x447e0
static volatile int g_wpn_on;
static volatile uint64_t g_wpn_frame, g_wpn_shown_frame;
static volatile uint32_t g_wpn_seq;
static float g_wpn_off[8];  // back, roll, pitch, yaw, (s131) scale, grip point in model space x y z
static volatile uint64_t g_wpn_actor, g_wpn_model;  // the held weapon actor + model seen by the opacity hook lately
static volatile uint64_t g_wpn_actor_frame;
static SRWLOCK g_wpn_lock = SRWLOCK_INIT;
static char g_wpn_name[64];  // the equipped weapon's actor name (under g_wpn_lock)
static int g_wpn_wear_q;     // queued wear points (under g_wpn_lock)
static unsigned g_wpn_moves, g_wpn_wears;
static uint64_t wpn_item(void) {  // Link's equipped sword-slot PouchItem, or 0
    uint64_t got = SS->pmdm_got;
    uint64_t pp = in_guest(got) ? *(const uint64_t *)(uintptr_t)got : 0;
    uint64_t mgr = in_guest(pp) ? *(const uint64_t *)(uintptr_t)pp : 0;
    uint64_t it = in_guest(mgr) ? *(const uint64_t *)(uintptr_t)(mgr + WPN_SLOTS_OFF) : 0;
    return in_guest(it) && *(const int32_t *)(uintptr_t)(it + 0x18) == 0 ? it : 0;
}
static bool wpn_item_name(uint64_t it, char *out, int cap) {
    uint64_t s = *(const uint64_t *)(uintptr_t)(it + 0x30);
    if (!in_guest(s) || cap < 2) return false;
    const char *p = (const char *)(uintptr_t)s;
    int i;
    for (i = 0; i < cap - 1 && p[i]; i++) out[i] = p[i];
    out[i] = 0;
    return i > 0 && !p[i];
}
int linkpos_weapon_info(char *actor, int cap, int *life, int *atk_up) {
    uint64_t it = wpn_item();
    char nm[64];
    if (!it || !wpn_item_name(it, nm, sizeof nm)) return 0;
    AcquireSRWLockExclusive(&g_wpn_lock);
    if (strcmp(g_wpn_name, nm)) {
        LOG("[linkpos] weapon: equipped %s (was %s), life %d, use %d\n", nm, g_wpn_name[0] ? g_wpn_name : "-",
            *(const int32_t *)(uintptr_t)(it + 0x20), *(const int32_t *)(uintptr_t)(it + 0x1c));
        snprintf(g_wpn_name, sizeof g_wpn_name, "%s", nm), g_wpn_wear_q = 0, g_wpn_actor = 0, g_wpn_model = 0;
    }
    ReleaseSRWLockExclusive(&g_wpn_lock);
    if (actor && cap > 0) snprintf(actor, (size_t)cap, "%s", nm);
    if (life) *life = *(const int32_t *)(uintptr_t)(it + 0x20);
    if (atk_up) *atk_up = (*(const uint32_t *)(uintptr_t)(it + 0x88) & 1) ? (int)*(const uint32_t *)(uintptr_t)(it + 0x80) : 0;
    return 1;
}
// (s131) off[8]: off[0..3] as below + off[4] = scale (BotW PlayerEqScale; <= 0 = 1), off[5..7] = the model-space point
// (metres, unscaled) that sits on Sora's grip. For the 3 monster arms (Stal arm models run along model +X, knob at the
// origin, recon/S130_HELPER_STALARM.md); every other weapon sends 1, 0 0 0 (= the s128-s130 behaviour).
void linkpos_weapon_hand2(int on, const float off[8]) {
    g_wpn_seq++;
    MemoryBarrier();
    g_wpn_on = on != 0, g_wpn_frame = g_vel_frame_id;
    if (off) memcpy(g_wpn_off, off, sizeof g_wpn_off);
    MemoryBarrier();
    g_wpn_seq++;
}
void linkpos_weapon_hand(int on, const float off[4]) {  // (older DLLs) scale 1, no grip point
    float o[8] = {0, 0, 0, 0, 1, 0, 0, 0};
    if (off) memcpy(o, off, 4 * sizeof(float));
    linkpos_weapon_hand2(on, off ? o : NULL);
}
// (s128b) the DLL's equipped weapon name (it walks the pouch list: mEquippedWeapons is filled only while the pause menu is open)
void linkpos_weapon_name(const char *nm) {
    if (!nm) return;
    AcquireSRWLockExclusive(&g_wpn_lock);
    if (strncmp(g_wpn_name, nm, sizeof g_wpn_name - 1)) {
        LOG("[linkpos] weapon: DLL says equipped %s (was %s)\n", nm, g_wpn_name[0] ? g_wpn_name : "-");
        snprintf(g_wpn_name, sizeof g_wpn_name, "%s", nm), g_wpn_wear_q = 0, g_wpn_actor = 0, g_wpn_model = 0;
    }
    ReleaseSRWLockExclusive(&g_wpn_lock);
}
bool linkpos_weapon_shown(void) { return g_wpn_shown_frame && g_vel_frame_id - g_wpn_shown_frame <= 2; }
void linkpos_weapon_wear(int n) {
    if (n <= 0) return;
    AcquireSRWLockExclusive(&g_wpn_lock);
    g_wpn_wear_q += n;
    ReleaseSRWLockExclusive(&g_wpn_lock);
}
static bool wpn_wanted(void) { return g_wpn_on && g_vel_frame_id - g_wpn_frame <= WPN_FRESH; }
// (opacity hook) is <a> Link's equipped weapon (a Weapon_ actor of Link: is_link_equip already true) and wanted in Sora's hand?
static bool wpn_hand_actor(uint64_t a) {
    if (!wpn_wanted()) return false;
    const char *n = actor_name(a);
    if (!n || strncmp(n, "Weapon_", 7)) return false;
    AcquireSRWLockShared(&g_wpn_lock);
    bool m = g_wpn_name[0] && !strcmp(n, g_wpn_name);
    ReleaseSRWLockShared(&g_wpn_lock);
    if (!m) return false;
    uint64_t mdl = *(const uint64_t *)(uintptr_t)(a + 0x4e0);
    if (!in_guest(mdl)) return false;
    if (g_wpn_actor != a) LOG("[linkpos] weapon: %s actor %llx model %llx goes to Sora's hand\n", n, (unsigned long long)a, (unsigned long long)mdl);
    g_wpn_actor = a, g_wpn_model = mdl, g_wpn_actor_frame = g_vel_frame_id;
    return true;
}
static void wpn_show(uint64_t a) {  // (opacity hook) like slate_hip_apply without the snap
    *(float *)(uintptr_t)(a + ACTOR_OPACITY) = 1;
    *(float *)(uintptr_t)(a + ACTOR_FINAL_OPACITY) = 1;
    *(float *)(uintptr_t)(a + ACTOR_START_OPACITY) = 1;
    *(uint8_t *)(uintptr_t)(a + ACTOR_OPACITY_DIRTY) = 1;
}
bool sora_draw_grip_world(float out[12], float *scale, float vel[3]);
// (s129 user: gliding / falling the weapon sat slightly off Sora's hand) this calcWorld uses the grip of Sora's LAST draw, so
// at speed the weapon trails him: lead it by his feet velocity x g_wpn_lead_ms (DLL key wlead, default 17 tuned live s130; 0 = old)
static volatile int g_wpn_lead_ms = 17;
void linkpos_weapon_lead(int ms) { g_wpn_lead_ms = ms < -200 ? -200 : ms > 200 ? 200 : ms; }
int linkpos_weapon_rox(void) { return 1; }  // (s137 roxwpn) marker: the grip bone follows the body (Roxas 229), so the DLL may let Roxas hold a weapon
static void wpn_model(uint64_t model) {  // (calcWorld hook) the held weapon's world matrix from Sora's grip
    if (!model || model != g_wpn_model || g_vel_frame_id - g_wpn_actor_frame > 2 || !wpn_wanted()) return;
    float g[12], off[8], sc, vel[3];
    uint32_t q0;
    int n = 0;
    do { q0 = g_wpn_seq; MemoryBarrier(); memcpy(off, g_wpn_off, sizeof off); MemoryBarrier(); } while ((q0 & 1 || q0 != g_wpn_seq) && ++n < 8);
    if ((q0 & 1) || q0 != g_wpn_seq || !sora_draw_grip_world(g, &sc, vel)) return;
    {
        float k = (float)g_wpn_lead_ms / 1000.0f;
        g[3] += vel[0] * k, g[7] += vel[1] * k, g[11] += vel[2] * k;
    }
    // grip columns: A0 = (g0 g4 g8), A1 = (g1 g5 g9), A2 = (g2 g6 g10); weapon axes X = A0, Y = A2, Z = -A1
    float X[3] = {g[0], g[4], g[8]}, Y[3] = {g[2], g[6], g[10]}, Z[3] = {-g[1], -g[5], -g[9]};
    // local turns (deg, GUESS keys, default 0): k 1 = roll about Z (turns X,Y), 2 = pitch about X (turns Y,Z), 3 = yaw about Y (turns Z,X)
    for (int k = 1; k <= 3; k++) {
        if (off[k] == 0) continue;
        float r = off[k] * 0.0174533f, cs = cosf(r), sn = sinf(r);
        float *p = k == 1 ? X : k == 2 ? Y : Z, *q = k == 1 ? Y : k == 2 ? Z : X;
        float np[3], nq[3];
        for (int i = 0; i < 3; i++) np[i] = cs * p[i] + sn * q[i], nq[i] = -sn * p[i] + cs * q[i];
        memcpy(p, np, sizeof np), memcpy(q, nq, sizeof nq);
    }
    // (s131) scale + model-space grip point: origin = grip - Z*back - s*(X*gx + Y*gy + Z*gz), columns x s
    float s = off[4] > 0.05f && off[4] < 4.0f ? off[4] : 1.0f;
    float *m = (float *)(uintptr_t)(model + 0x58);
    float bef[3] = {m[3], m[7], m[11]};
    for (int i = 0; i < 3; i++) m[i * 4 + 0] = X[i] * s, m[i * 4 + 1] = Y[i] * s, m[i * 4 + 2] = Z[i] * s;
    for (int i = 0; i < 3; i++)
        m[i * 4 + 3] = g[3 + 4 * i] - Z[i] * off[0] - s * (X[i] * off[5] + Y[i] * off[6] + Z[i] * off[7]);
    *(uint8_t *)(uintptr_t)(model + 0xa0) |= 1;
    {   // (s128b) its units' view bit 0 on (BotW copies hidden Link's mask, bit 0 off, onto his gear: the sword was in no 3D pass;
        // keeping LINK's bit on instead drew his standing shadow). Units = model +0x40 array of +0x38, *entry = unit, mask u16 +0xc.
        int nu = *(const int32_t *)(uintptr_t)(model + 0x38);
        uint64_t arr = *(const uint64_t *)(uintptr_t)(model + 0x40);
        if (nu > 0 && nu <= 64 && in_guest(arr))
            for (int i = 0; i < nu; i++) {
                uint64_t pu = *(const uint64_t *)(uintptr_t)(arr + 8 * (uint64_t)i), u = in_guest(pu) ? *(const uint64_t *)(uintptr_t)pu : 0;
                if (in_guest(u)) *(volatile uint16_t *)(uintptr_t)(u + 0xc) |= 1;
            }
    }
    g_wpn_shown_frame = g_vel_frame_id;
    if (g_wpn_moves++ < 3 || g_wpn_moves % 3600 == 0)
        LOG("[linkpos] weapon: move #%u model %llx %.2f %.2f %.2f -> grip %.2f %.2f %.2f (back %.2f m, turn %.0f %.0f %.0f, scale %.2f, grip pt %.2f %.2f %.2f)\n", g_wpn_moves,
            (unsigned long long)model, bef[0], bef[1], bef[2], m[3], m[7], m[11], off[0], off[1], off[2], off[3], s, off[5], off[6], off[7]);
}
// (addDamage hook) the held weapon's own per-frame zero call -> the queued wear
static void wpn_wear_ride(Cpu *c, uint64_t actor) {
    if (!g_wpn_wear_q || !actor || actor != g_wpn_actor || c->x[1] != 3 || (int32_t)c->x[2] != 0 || (int32_t)c->x[6] != -1) return;
    if (*(const uint16_t *)(uintptr_t)(actor + 0xe50) & 0x200) { g_wpn_wear_q = 0; return; }  // (S128_HELPER_BREAK.md: broken bit)
    AcquireSRWLockExclusive(&g_wpn_lock);
    int n = g_wpn_wear_q;
    g_wpn_wear_q = 0;
    ReleaseSRWLockExclusive(&g_wpn_lock);
    if (n <= 0) return;
    if (n > 20) n = 20;
    c->x[1] = 1, c->x[2] = (uint32_t)(100 * n), c->x[3] = 0, c->x[4] = 0, c->x[5] = 5, c->x[6] = 15, c->x[7] = 1;
    if (g_wpn_wears++ < 6 || g_wpn_wears % 200 == 0)
        LOG("[linkpos] weapon: wear #%u: %d hit(s) on %s (actor life %d)\n", g_wpn_wears, n, actor_name(actor), *(const int32_t *)(uintptr_t)(actor + 0x848));  // (0x848 = ACTOR_LIFE, defined further down)
}

static void slate_model_hook(Cpu *c) {  // gsys::Model::calcWorld(model), x0 = the model
    uint64_t model = c->x[0];
    if (model) bh_model(model), gear_park(model), wpn_model(model);
    if (g_slm_actor && model && in_guest(g_slm_actor) && model == *(const uint64_t *)(uintptr_t)(g_slm_actor + 0x4e0)) {  // (s111 slatecut) the game's own matrix, first
        g_slg_seq++;
        MemoryBarrier();
        memcpy(g_slg_w, (const float *)(uintptr_t)(model + 0x58), sizeof g_slg_w);
        g_slg_frame = g_vel_frame_id;
        MemoryBarrier();
        g_slg_seq++;
    }
    if (g_slm_on && g_slm_actor && model && g_vel_frame_id - g_slm_frame <= 30 && in_guest(g_slm_actor) &&
        model == *(const uint64_t *)(uintptr_t)(g_slm_actor + 0x4e0)) {
        float w[12];
        uint32_t q0;
        int keep, n = 0;
        do { q0 = g_slm_seq; MemoryBarrier(); memcpy(w, g_slm_w, sizeof w); keep = g_slm_keep; MemoryBarrier(); } while ((q0 & 1 || q0 != g_slm_seq) && ++n < 8);
        if (!(q0 & 1) && q0 == g_slm_seq) {
            float *m = (float *)(uintptr_t)(model + 0x58);
            float bef[3] = {m[3], m[7], m[11]};
            if (keep) m[3] = w[3], m[7] = w[7], m[11] = w[11];
            else memcpy(m, w, sizeof w);
            *(uint8_t *)(uintptr_t)(model + 0xa0) |= 1;
            if (g_slm_hits++ < 3 || g_slm_hits % 1800 == 0)
                LOG("[linkpos] slatemodel: #%u model %llx pos %.2f %.2f %.2f -> %.2f %.2f %.2f (%s)\n", g_slm_hits, (unsigned long long)model,
                    bef[0], bef[1], bef[2], m[3], m[7], m[11], keep ? "translation only" : "full matrix");
        }
    }
    if (g_slm_actor && model && in_guest(g_slm_actor) && model == *(const uint64_t *)(uintptr_t)(g_slm_actor + 0x4e0)) {  // (s107 slateocc)
        g_slo_seq++;
        MemoryBarrier();
        memcpy(g_slo_w, (const float *)(uintptr_t)(model + 0x58), sizeof g_slo_w);
        g_slo_frame = g_vel_frame_id;
        MemoryBarrier();
        g_slo_seq++;
    }
    g_slm_orig(c);
}
// (s107 slateocc, recon/S107_HELPER_SLATE.md) The Slate model's world matrix as it goes into calcWorld (after our own rewrite),
// for the draw pass's analytic occluder (sora_draw.c). out = sead Matrix34f row-major 3x4 (columns = local axes incl. scale,
// out[3] out[7] out[11] = translation). False: the Slate was not drawn for 3 game frames.
bool linkpos_slate_model_world(float out[12]) {
    for (int n = 0; n < 8; n++) {
        uint32_t q0 = g_slo_seq;
        MemoryBarrier();
        float w[12];
        memcpy(w, g_slo_w, sizeof w);
        uint64_t fr = g_slo_frame;
        MemoryBarrier();
        if ((q0 & 1) || q0 != g_slo_seq) continue;
        if (!fr || g_vel_frame_id - fr > 3) return false;
        memcpy(out, w, sizeof w);
        return true;
    }
    return false;
}
static void slate_model_init(void) {
    if (!SS->slate_model_call || !SS->slate_model_target) return;
    if (!(g_slm_orig = dispatch_lookup(SS->slate_model_target))) { LOG("[linkpos] slatemodel: no function at %llx\n", (unsigned long long)SS->slate_model_target); return; }
    uint64_t fn = guest_fn_at(SS->slate_model_call);
    if (!fn || !hook_calls(fn, SS->slate_model_target, slate_model_hook)) LOG("[linkpos] slatemodel: call at %llx not hooked (function %llx)\n", (unsigned long long)SS->slate_model_call, (unsigned long long)fn);
}
static bool slate_hip_wanted(uint64_t a) {
    if (!g_slate_show || g_vel_frame_id - g_slate_frame > 30) return false;
    const char *n = actor_name(a);
    return n && !strncmp(n, "Item_Conductor", 14);
}

static void zero_opacity(uint64_t a) {
    *(float *)(uintptr_t)(a + ACTOR_FINAL_OPACITY) = 0;
    *(float *)(uintptr_t)(a + ACTOR_START_OPACITY) = 0;
    *(uint8_t *)(uintptr_t)(a + ACTOR_OPACITY_DIRTY) = 1;
}

// ------------------------------------------------------------------ enemies (step 5: Sora's hits)
// Every drawn actor goes through applyOpacityIfDirty once a frame, so the opacity hook also keeps the enemy list:
// actors named Enemy_* (copies of name / position, so readers never touch an actor that may be gone). Entries not
// seen for ENEMY_STALE game frames drop out. The Actor vtable (zeldaret actActor.h numbering, BaseProc = slots 0-29):
// getLife = slot 87 (+0x2b8, s32 *), getDamageMgr = slot 127 (+0x3f8); unverified on 1.8.2, hence the log.
//   SAVAGEWIND_LINK_ENEMY_LOG=1   each enemy once (name, position, distance to Link, vtable + those two slots, where
//                                 its first 0x2000 bytes hold the ints 13 / 72 / 240 = red / blue / black Bokoblin
//                                 life), and the whole list every 600 game frames
#define ENEMY_MAX 64
#define ENEMY_STALE 60
#define VT_GET_LIFE 87
#define VT_GET_DAMAGE_MGR 127
#define ACTOR_LIFE 0x848        // (1.8.2: getLife = add x0, x0, #0x848; enemy1: 13 on the red Bokoblins)
#define ACTOR_DAMAGE_MGR 0x840  // (getDamageMgr = ldr x0, [x0, #0x840])
typedef struct { uint64_t actor, seen; float pos[3], dist; int32_t life; uint32_t hid; char name[48]; } Enemy;  // hid: mubin HashId, 0 = none (s166)
static Enemy g_enemy[ENEMY_MAX];
static int g_nenemy;
static SRWLOCK g_enemy_lock = SRWLOCK_INIT;

static void enemy_first_log(uint64_t a, const char *name) {
    uint64_t vt = *(const uint64_t *)(uintptr_t)a;
    char ints[200] = "";
    if (in_guest(vt)) {
        size_t n = readable(a, 0x2000);
        for (size_t o = 0; o + 4 <= n && strlen(ints) + 16 < sizeof ints; o += 4) {
            int32_t v = *(const int32_t *)(uintptr_t)(a + o);
            if (v == 13 || v == 72 || v == 240) sprintf(ints + strlen(ints), " %d@+%#zx", v, o);
        }
    }
    uint64_t dm = *(const uint64_t *)(uintptr_t)(a + ACTOR_DAMAGE_MGR), dmvt = in_guest(dm) ? *(const uint64_t *)(uintptr_t)dm : 0;
    if (in_guest(dmvt))
        LOG("[linkpos] enemy: %llx damage manager %llx vtable %llx: applyDamage %llx canTakeDamage %llx addDamage %llx\n",
            (unsigned long long)a, (unsigned long long)dm, (unsigned long long)dmvt, (unsigned long long)*(const uint64_t *)(uintptr_t)(dmvt + 8 * 15),
            (unsigned long long)*(const uint64_t *)(uintptr_t)(dmvt + 8 * 44), (unsigned long long)*(const uint64_t *)(uintptr_t)(dmvt + 8 * 47));
    const float *m = (const float *)(uintptr_t)(a + ACTOR_MTX);
    LOG("[linkpos] enemy: %llx %s at %.2f %.2f %.2f dist %.1f vtable %llx getLife %llx getDamageMgr %llx ints:%s\n",
        (unsigned long long)a, name, m[3], m[7], m[11], g_servo_actor ? link_dist(a) : -1.0f, (unsigned long long)vt,
        in_guest(vt) ? (unsigned long long)*(const uint64_t *)(uintptr_t)(vt + 8 * VT_GET_LIFE) : 0ull,
        in_guest(vt) ? (unsigned long long)*(const uint64_t *)(uintptr_t)(vt + 8 * VT_GET_DAMAGE_MGR) : 0ull,
        ints[0] ? ints : " -");
}

static void dmg_vtable_patch(uint64_t a);
static int edmg_queue_remote(uint64_t actor, uint32_t hid, int32_t dealt, int f54, int f40, int kill);

// s166 (recon/S166_HELPER_EXEDMG.md): the map HashId of a placed actor = *(u32*)(*(u64*)(actor+0x7c8)+0x58) (both game
// versions). 0 = spawned (null map object) / no HashId (0xFFFFFFFF) / unreadable.
static uint32_t actor_hid(uint64_t a) {
    if (!in_guest(a) || readable(a + 0x7c8, 8) != 8) return 0;
    uint64_t mo = *(const uint64_t *)(uintptr_t)(a + 0x7c8);
    if (!mo || !in_guest(mo) || readable(mo + 0x58, 4) != 4) return 0;
    uint32_t h = *(const uint32_t *)(uintptr_t)(mo + 0x58);
    return h == 0xFFFFFFFFu ? 0 : h;
}
static int g_edmg_on, g_edmg_log;  // SAVAGEWIND_LINK_EDMG / _LOG (read in dmg_init, before guest code runs)
static int g_esync_on, g_esync_log;  // SAVAGEWIND_LINK_ESYNC / _LOG (esync_init; it needs the placed enemies' HashId too)
#include "linkpos_edmg.h"  // (s166 E2: pending map / echo guard / f54 table, pure C, host-tested)
static EdmgEchoTab g_edmg_echo;  // (all of these under g_enemy_lock)
static EdmgPendTab g_edmg_pend;
static EdmgArgTab g_edmg_argt;
static struct { uint64_t cap, lost, local_only, applied, queued, pending, ignored, rides, ride_dead, echo_sub, pend_hit, queue_full, ride_expired; } g_edmg_st;

// s38: Animal_* too (boars, deer, ... : lock-on + Sora's hits), but not the Animal_Insect_* bugs.
static bool enemy_name_ok(const char *name) {
    if (!strncmp(name, "Enemy_", 6)) return true;
    return !strncmp(name, "Animal_", 7) && strncmp(name, "Animal_Insect_", 14);
}

static void enemy_note(uint64_t a) {
    const char *name = actor_name(a);
    if (!name || !enemy_name_ok(name)) return;
    static int log_on = -1;
    if (log_on < 0) log_on = env_int("SAVAGEWIND_LINK_ENEMY_LOG", 0);
    const float *m = (const float *)(uintptr_t)(a + ACTOR_MTX);
    AcquireSRWLockExclusive(&g_enemy_lock);
    int i;
    for (i = 0; i < g_nenemy && g_enemy[i].actor != a; i++) {}
    bool fresh = i == g_nenemy;
    if (fresh) {
        if (g_nenemy == ENEMY_MAX) { ReleaseSRWLockExclusive(&g_enemy_lock); return; }
        g_nenemy++;
        memset(&g_enemy[i], 0, sizeof *g_enemy);
        g_enemy[i].actor = a;
        snprintf(g_enemy[i].name, sizeof g_enemy[i].name, "%s", name);
        if (g_edmg_on || g_esync_on) g_enemy[i].hid = actor_hid(a);  // (gate off = no new reads; VirtualQuery under the lock otherwise)
    }
    uint32_t hid = g_enemy[i].hid;
    g_enemy[i].seen = g_vel_frame_id;
    g_enemy[i].pos[0] = m[3], g_enemy[i].pos[1] = m[7], g_enemy[i].pos[2] = m[11];
    g_enemy[i].dist = g_servo_actor ? link_dist(a) : -1;
    g_enemy[i].life = *(const int32_t *)(uintptr_t)(a + ACTOR_LIFE);
    EdmgPend pend;
    bool got_pend = false;
    if (g_edmg_on && hid && g_edmg_pend.n && g_enemy[i].life > 0 && edmg_pend_take(&g_edmg_pend, hid, g_vel_frame_id, &pend)) {
        // (s166 E2: a hit / kill that came while this enemy was not loaded here: it is queued now; the next ride applies it)
        if (edmg_queue_remote(a, hid, pend.dmg, pend.f54, pend.f40, pend.kill)) g_edmg_st.pend_hit++, got_pend = true;
        else edmg_pend_add(&g_edmg_pend, hid, pend.dmg, pend.f54, pend.f40, pend.kill, pend.first), g_edmg_st.queue_full++;
    }
    ReleaseSRWLockExclusive(&g_enemy_lock);
    if (got_pend && g_edmg_log)
        LOG("[linkpos] edmg pending applied hid=%08x name=%s dmg=%d kill=%d f54=%d\n", hid, name, pend.dmg, pend.kill, pend.f54);
    if (fresh && log_on) enemy_first_log(a, name);
    if (fresh && g_edmg_on && g_edmg_log) LOG("[linkpos] enemy id name=%s hid=%08x\n", name, hid);
    if (fresh) dmg_vtable_patch(a);
}

// Copies of the enemy list for sora.c (step 5 part 2: the blade sweep): actor, feet position (mMtx translation), life.
int linkpos_enemies(uint64_t *actor, float (*pos)[3], int32_t *life, int max) {
    AcquireSRWLockExclusive(&g_enemy_lock);
    int n = 0;
    for (int i = 0; i < g_nenemy && n < max; i++, n++) {
        actor[n] = g_enemy[i].actor, life[n] = g_enemy[i].life;
        memcpy(pos[n], g_enemy[i].pos, sizeof pos[n]);
    }
    ReleaseSRWLockExclusive(&g_enemy_lock);
    return n;
}

static uint64_t enemy_nearest(void) {  // the enemy nearest to Link right now (0: none)
    uint64_t best = 0;
    float bd = 1e30f;
    AcquireSRWLockExclusive(&g_enemy_lock);
    for (int i = 0; i < g_nenemy; i++)
        if (g_enemy[i].dist >= 0 && g_enemy[i].dist < bd) bd = g_enemy[i].dist, best = g_enemy[i].actor;
    ReleaseSRWLockExclusive(&g_enemy_lock);
    if (best) LOG("[linkpos] nearest enemy %llx at %.2f\n", (unsigned long long)best, bd);
    return best;
}

// The damage path (hit2: the watch on a Bokoblin's life +0x848 caught only applyDamage f_7000a81da8 writing it once a
// frame, from f_700046839c). uking::act::DamageManagerBase (zeldaret actDamageManagerBase.h) virtual slot 47 =
// addDamage(this, s64 a2, s32 damage, s32 df48, s32 minDmg, s32 f50, s32 f54, s32 f40): it adds up a frame's damage
// that applyDamage then takes off the life. 0x7000a82004 = the enemies' one (vtable 7001b59d28: Bokoblins, Golem).
//   SAVAGEWIND_LINK_DAMAGE_LOG=1   log every call (whose manager, its actor and life, all args, the guest caller)
#define DMG_ADD_ENEMY (SS->dmg_add_enemy)
#define DM_ACTOR 0x18  // IDamageManager mActor
static GuestFn g_dmg_add;

// hit3 (Link's Tree Branch on a red Bokoblin, life 13 -> 11 -> 9 -> 7 -> 3): each hit = addDamage(dm, 2, 2, 2, 0, 0, 15,
// 0) from f_7000a77294's caller, which calls it EVERY frame for every enemy with a2 = 2..5 (and 7 from 0x7000a76ce4)
// and all zeros. So a hit of ours rides on that call: linkpos_damage_enemy(actor, n, f54) queues n, and the next a2 == 2
// call for that actor's manager gets the branch's arguments with damage n (the game then does the rest: life, reaction,
// death, drops). Game thread or not: the queue is under g_enemy_lock.
// f54 = the damage type / size and the frame's priority (DAMAGE_ARGS_RECON.md Q2/Q3): 15 small (the branch), 0x11
// medium, 0x14 / 0x17 large, 0x16 critical, 0x15 explosion, 0x1b barrier blow-away; <= 0 = 15. Hits queued for one
// enemy in the same frame add up their damage and keep the highest f54.
// f40 = the element (DAMAGE_ARGS_RECON.md Q2): 0 none, 1 fire, 2 ice, 3 electric (5 = attr bit 0x400000); it goes with
// the highest f54 of the frame.
//   SAVAGEWIND_LINK_DAMAGE_TEST=<presented>:<n>[:<f54>[:<f40>]][,...]   damage the enemy nearest to Link (tests, no swing)
#define DMG_SLOT_WEAPON 2
#define DMG_QUEUE 16
#define DMG_F54_SMALL 15
// rn / rkill: damage that came from ANOTHER PLAYER'S game (shared enemy HP, linkpos_edmg_apply); rn is added to n at the
// ride (edmg_remote_amount, with the enemy's life of that moment) and noted in the echo guard. rframe = when it was queued.
// rhid = that enemy's HashId (s167 review SF-1: an expired ride goes back to pending even after the row left g_enemy).
static struct { uint64_t actor; int n, f54, f40; int rn, rkill; uint64_t rframe; uint32_t rhid; } g_dmg_q[DMG_QUEUE];
static int g_ndmg_q;

void linkpos_damage_enemy_ex(uint64_t actor, int n, int f54, int f40) {
    if (f54 <= 0) f54 = DMG_F54_SMALL;
    AcquireSRWLockExclusive(&g_enemy_lock);
    int i;
    for (i = 0; i < g_ndmg_q && g_dmg_q[i].actor != actor; i++) {}
    if (i < g_ndmg_q) {
        g_dmg_q[i].n += n;
        if (f54 > g_dmg_q[i].f54) g_dmg_q[i].f54 = f54, g_dmg_q[i].f40 = f40;
    } else if (g_ndmg_q < DMG_QUEUE) {
        memset(&g_dmg_q[g_ndmg_q], 0, sizeof g_dmg_q[0]);
        g_dmg_q[g_ndmg_q].actor = actor, g_dmg_q[g_ndmg_q].f54 = f54, g_dmg_q[g_ndmg_q].f40 = f40, g_dmg_q[g_ndmg_q++].n = n;
    }
    ReleaseSRWLockExclusive(&g_enemy_lock);
    LOG("[linkpos] damage %d (f54 %#x f40 %d) queued for enemy %llx\n", n, f54, f40, (unsigned long long)actor);
}
void linkpos_damage_enemy(uint64_t actor, int n, int f54) { linkpos_damage_enemy_ex(actor, n, f54, 0); }

static void edmg_args_note(uint64_t actor, int32_t damage, int32_t f54, int32_t f40);
static void dmg_add_common(Cpu *c, GuestFn orig) {
    uint64_t dm = c->x[0], actor = in_guest(dm) ? *(const uint64_t *)(uintptr_t)(dm + DM_ACTOR) : 0;
    wpn_wear_ride(c, actor);  // (s128 wpn) Sora's hits wear Link's weapon
    if (g_ndmg_q && c->x[1] == DMG_SLOT_WEAPON && (int32_t)c->x[2] == 0) {
        int n = 0, f54 = DMG_F54_SMALL, f40 = 0;
        int32_t ramt = 0;
        AcquireSRWLockExclusive(&g_enemy_lock);
        for (int i = 0; i < g_ndmg_q; i++)
            if (g_dmg_q[i].actor == actor) {
                int rn = g_dmg_q[i].rn, rkill = g_dmg_q[i].rkill;
                n = g_dmg_q[i].n, f54 = g_dmg_q[i].f54, f40 = g_dmg_q[i].f40, g_dmg_q[i] = g_dmg_q[--g_ndmg_q];
                if (rn > 0 || rkill) {  // (s166 edmg: another player's damage; the kill clamp uses the life of THIS moment)
                    ramt = edmg_remote_amount(*(const int32_t *)(uintptr_t)(actor + ACTOR_LIFE), rn, rkill);
                    if (ramt > 0) edmg_echo_add(&g_edmg_echo, actor, ramt, g_vel_frame_id), n += ramt, g_edmg_st.rides++;
                    else g_edmg_st.ride_dead++;
                }
                break;
            }
        ReleaseSRWLockExclusive(&g_enemy_lock);
        if (n) {
            c->x[2] = c->x[3] = (uint32_t)n, c->x[4] = 0, c->x[5] = 0, c->x[6] = (uint32_t)f54, c->x[7] = (uint32_t)f40;
            LOG("[linkpos] damage %d f54 %#x f40 %d applied to %llx%s (life %d before)\n", n, f54, f40, (unsigned long long)actor, ramt > 0 ? " (remote part)" : "",
                *(const int32_t *)(uintptr_t)(actor + ACTOR_LIFE));
        }
    }
    if (g_edmg_on) edmg_args_note(actor, (int32_t)c->x[2], (int32_t)c->x[6], (int32_t)c->x[7]);  // (s166 edmg: f54 / f40 of the frame's hits)
    static int log_on = -1;
    if (log_on < 0) log_on = env_int("SAVAGEWIND_LINK_DAMAGE_LOG", 0);
    if (log_on < 2 && !(int32_t)c->x[2] && (int32_t)c->x[6] == -1) { orig(c); return; }  // (the per-frame zeros)
    const char *name = in_guest(actor) ? actor_name(actor) : NULL;
    if (log_on) LOG("[linkpos] addDamage dm %llx actor %llx %s life %d | a2 %llx damage %d df48 %d minDmg %d f50 %d f54 %d f40 %d | lr %llx\n",
        (unsigned long long)dm, (unsigned long long)actor, name ? name : "?",
        in_guest(actor) ? *(const int32_t *)(uintptr_t)(actor + ACTOR_LIFE) : -1, (unsigned long long)c->x[1], (int32_t)c->x[2],
        (int32_t)c->x[3], (int32_t)c->x[4], (int32_t)c->x[5], (int32_t)c->x[6], (int32_t)c->x[7], (unsigned long long)c->x[30]);
    orig(c);
}
static void dmg_add_hook(Cpu *c) { dmg_add_common(c, g_dmg_add); }

// s166 (shared enemy HP, step E1: capture + log only, no network): hook applyDamage (DamageManager vslot 15) and report
// how much life the game took off each enemy. Gate: SAVAGEWIND_LINK_EDMG=1 (default off: nothing is hooked).
//   SAVAGEWIND_LINK_EDMG_LOG=1   log `edmg cap` per hit and `enemy id` per new enemy
// linkpos_edmg_take() drains the ring (sora_net.h sends it); linkpos_edmg_apply() is what a received hit calls (E2).
#define DMG_APPLY_ENEMY (SS->dmg_apply_enemy)
#define EDMG_Q 32
typedef struct { uint64_t actor, frame; uint32_t hid; int32_t dealt, before, after, f54, f40, kill; char name[48]; } LinkposEdmg;
static LinkposEdmg g_edmg[EDMG_Q];
static int g_nedmg;
static uint64_t g_edmg_total, g_edmg_lost;
static GuestFn g_dmg_apply;

static void edmg_args_note(uint64_t actor, int32_t damage, int32_t f54, int32_t f40) {  // (called from dmg_add_common)
    if (damage <= 0 || !actor) return;
    AcquireSRWLockExclusive(&g_enemy_lock);
    edmg_arg_note(&g_edmg_argt, actor, g_vel_frame_id, f54, f40);
    ReleaseSRWLockExclusive(&g_enemy_lock);
}

static void dmg_apply_hook(Cpu *c) {
    uint64_t dm = c->x[0], lp = c->x[1];
    int32_t before = in_guest(lp) && readable(lp, 4) == 4 ? *(const int32_t *)(uintptr_t)lp : 0;
    g_dmg_apply(c);
    uint64_t ret = c->x[0];  // (the game's own return value: kept for the caller)
    if (before <= 0 || !in_guest(lp)) return;
    int32_t after = *(const int32_t *)(uintptr_t)lp;
    if (after >= before) return;
    int32_t drop = before - after;
    uint64_t actor = in_guest(dm) && readable(dm + DM_ACTOR, 8) == 8 ? *(const uint64_t *)(uintptr_t)(dm + DM_ACTOR) : 0;
    const char *name = actor && in_guest(actor) ? actor_name(actor) : NULL;
    if (!name || !enemy_name_ok(name)) return;  // (the player's own manager and weapons end up here too)
    LinkposEdmg e;
    memset(&e, 0, sizeof e);
    e.actor = actor, e.frame = g_vel_frame_id, e.hid = actor_hid(actor);
    e.before = before, e.after = after, e.kill = after <= 0, e.f54 = DMG_F54_SMALL;
    snprintf(e.name, sizeof e.name, "%s", name);
    uint64_t total = 0;
    int32_t echo;
    AcquireSRWLockExclusive(&g_enemy_lock);
    echo = edmg_echo_take(&g_edmg_echo, actor, g_vel_frame_id);  // (the part of this drop that another player's game put in: never sent back)
    if (echo > drop) echo = drop;
    if (e.kill && e.hid && g_edmg_pend.n) {  // (s167 review SF-2: it died here: a waiting remote kill is done, else it kills the Blood Moon respawn)
        EdmgPend t;
        (void)edmg_pend_take(&g_edmg_pend, e.hid, g_vel_frame_id, &t);
    }
    if (echo) g_edmg_st.echo_sub++;
    e.dealt = drop - echo;
    (void)edmg_arg_get(&g_edmg_argt, actor, g_vel_frame_id, &e.f54, &e.f40);  // (this frame or the one before)
    if (e.dealt > 0 && e.hid) {  // (hid 0 = a spawned enemy: stays local; review E1 SHOULD-FIX 4)
        if (g_nedmg == EDMG_Q) memmove(g_edmg, g_edmg + 1, sizeof *g_edmg * (EDMG_Q - 1)), g_nedmg--, g_edmg_lost++, g_edmg_st.lost++;
        g_edmg[g_nedmg++] = e;
        total = ++g_edmg_total;
        g_edmg_st.cap++;
    } else if (e.dealt > 0) g_edmg_st.local_only++;
    ReleaseSRWLockExclusive(&g_enemy_lock);
    if (g_edmg_log && (e.dealt > 0 || echo))
        LOG("[linkpos] edmg cap hid=%08x name=%s dealt=%d life=%d->%d kill=%d f54=%d f40=%d echo=%d%s #%llu\n", e.hid, name, e.dealt, before,
            after, e.kill, e.f54, e.f40, echo, e.dealt > 0 && !e.hid ? " (spawned: local only)" : "", (unsigned long long)total);
    c->x[0] = ret;
}

// Drains up to max captured hits into out (oldest first): the count; -1 = another struct size (DLL / exe mismatch).
int linkpos_edmg_take(LinkposEdmg *out, int max, int size) {
    if (size != (int)sizeof(LinkposEdmg) || !out || max <= 0) return -1;
    AcquireSRWLockExclusive(&g_enemy_lock);
    int n = g_nedmg < max ? g_nedmg : max;
    memcpy(out, g_edmg, sizeof *g_edmg * (size_t)n);
    memmove(g_edmg, g_edmg + n, sizeof *g_edmg * (size_t)(g_nedmg - n));
    g_nedmg -= n;
    ReleaseSRWLockExclusive(&g_enemy_lock);
    return n;
}

#define BASEPROC_STATE_OFF 0x64  // (u8: 3 = being deleted; = BASEPROC_STATE further down)
// ---- E2: another player's hit on an enemy of ours (called from sora_net.h, DLL thread or game thread) ----
// Queues it as the "remote part" of the ride in dmg_add_common (the game's normal pipeline: life, flinch, death, drops);
// the echo guard keeps the capture hook from sending it back. Enemy not loaded here (not in the list within 3 frames) =
// the pending map: damage waits EDMG_PEND_TTL frames, a KILL waits without a TTL (it is applied the moment the enemy
// shows up in the list, enemy_note; if it never does, until the session ends). Under g_enemy_lock.
static int edmg_queue_remote(uint64_t actor, uint32_t hid, int32_t dealt, int f54, int f40, int kill) {  // lock held; 0 = queue full
    int i;
    for (i = 0; i < g_ndmg_q && g_dmg_q[i].actor != actor; i++) {}
    if (i == g_ndmg_q) {
        if (g_ndmg_q == DMG_QUEUE) return 0;
        memset(&g_dmg_q[g_ndmg_q], 0, sizeof g_dmg_q[0]);
        g_dmg_q[g_ndmg_q].actor = actor, g_dmg_q[g_ndmg_q].f54 = DMG_F54_SMALL, g_ndmg_q++;
    }
    if (!g_dmg_q[i].rn && !g_dmg_q[i].rkill) g_dmg_q[i].rframe = g_vel_frame_id;
    g_dmg_q[i].rhid = hid;
    if (dealt > 0) g_dmg_q[i].rn = g_dmg_q[i].rn + dealt > EDMG_DMG_CAP ? EDMG_DMG_CAP : g_dmg_q[i].rn + dealt;
    g_dmg_q[i].rkill |= kill ? 1 : 0;
    if (f54 > g_dmg_q[i].f54) g_dmg_q[i].f54 = f54, g_dmg_q[i].f40 = f40;
    return 1;
}

// returns EDMG_QUEUED / EDMG_PENDING / EDMG_IGNORED, -1 = capture off (env) or hid 0
int linkpos_edmg_apply(uint32_t hid, int dealt, int after, int f54, int f40, int kill) {
    (void)after;  // (damage deltas: the sender's life is only for the log)
    if (!g_edmg_on || !hid) return -1;
    if (f54 <= 0 || f54 > 100) f54 = DMG_F54_SMALL;
    if (f40 < 0 || f40 > 5) f40 = 0;  // (s167 review SF-4: element from the wire; known 0-3, 5)
    if (dealt < 0) dealt = 0;
    int plan, row = -1;
    AcquireSRWLockExclusive(&g_enemy_lock);
    for (int i = 0; i < g_nenemy; i++)
        if (g_enemy[i].hid == hid && g_vel_frame_id - g_enemy[i].seen <= 3 && (row < 0 || g_enemy[i].seen > g_enemy[row].seen)) row = i;
    int32_t life = 0;
    int state = 0;
    if (row >= 0) {
        uint64_t a = g_enemy[row].actor;
        bool lok = in_guest(a) && readable(a + ACTOR_LIFE, 4) == 4;
        life = lok ? *(const int32_t *)(uintptr_t)(a + ACTOR_LIFE) : -1;  // (-1 = unreadable -> pending, s167 SF-6)
        if (lok && life < 0) life = 0;  // (the game's own negative life = dead, never "unreadable")
        state = in_guest(a) && readable(a + BASEPROC_STATE_OFF, 1) == 1 ? *(const uint8_t *)(uintptr_t)(a + BASEPROC_STATE_OFF) : 0;
    }
    plan = edmg_apply_plan(row >= 0, life, state, dealt, kill);
    if (plan == EDMG_QUEUED && !edmg_queue_remote(g_enemy[row].actor, hid, dealt, f54, f40, kill)) plan = EDMG_PENDING, g_edmg_st.queue_full++;
    if (plan == EDMG_PENDING) edmg_pend_expire(&g_edmg_pend, g_vel_frame_id), edmg_pend_add(&g_edmg_pend, hid, dealt, f54, f40, kill, g_vel_frame_id);
    g_edmg_st.applied++;
    if (plan == EDMG_QUEUED) g_edmg_st.queued++;
    else if (plan == EDMG_PENDING) g_edmg_st.pending++;
    else g_edmg_st.ignored++;
    ReleaseSRWLockExclusive(&g_enemy_lock);
    if (g_edmg_log)
        LOG("[linkpos] edmg apply hid=%08x dealt=%d after=%d kill=%d f54=%d f40=%d -> %s (life %d state %d)\n", hid, dealt, after, kill, f54, f40,
            plan == EDMG_QUEUED ? "queued" : plan == EDMG_PENDING ? "pending (not loaded)" : "ignored (dead / deleting)", life, state);
    return plan;
}

// s180: a save was loaded (the DLL calls this when BotW reads a slot's game_data.sav): the world is new, so every remote hit / KILL still
// waiting for its enemy (the pending map has no TTL for kills), queued for an actor, or remembered as an echo belongs to the old world.
// User s180: after Load slot 3 on both, COPY2's camp had 2 of 6 Bokoblins - 2 kills from the fight before the load were applied at spawn.
// Returns how many pending entries were dropped (kills in *kills).
int linkpos_edmg_reset(int *kills) {
    AcquireSRWLockExclusive(&g_enemy_lock);
    int n = g_edmg_pend.n, k = 0;
    for (int i = 0; i < g_edmg_pend.n; i++) k += g_edmg_pend.e[i].kill;
    int q = g_ndmg_q;
    g_nedmg = 0, g_ndmg_q = 0;
    memset(&g_edmg_echo, 0, sizeof g_edmg_echo), memset(&g_edmg_pend, 0, sizeof g_edmg_pend), memset(&g_edmg_argt, 0, sizeof g_edmg_argt);
    ReleaseSRWLockExclusive(&g_enemy_lock);
    if (kills) *kills = k;
    LOG("[linkpos] edmg reset (save loaded): dropped %d pending (%d kills), %d queued\n", n, k, q);
    return n;
}

// counters for `net diag` (the DLL passes its array size): applied, queued, pending, ignored, rides, ride_dead, echo_sub, pend_hit,
// queue_full, ride_expired, cap, lost, local_only, pending map size, pending kills, evicted, expired; returns the count written
int linkpos_edmg_stats(uint64_t *out, int n) {
    if (!out || n <= 0) return -1;
    AcquireSRWLockExclusive(&g_enemy_lock);
    int pk = 0;
    for (int i = 0; i < g_edmg_pend.n; i++) pk += g_edmg_pend.e[i].kill;
    uint64_t v[] = {g_edmg_st.applied, g_edmg_st.queued, g_edmg_st.pending, g_edmg_st.ignored, g_edmg_st.rides, g_edmg_st.ride_dead,
                    g_edmg_st.echo_sub, g_edmg_st.pend_hit, g_edmg_st.queue_full, g_edmg_st.ride_expired, g_edmg_st.cap, g_edmg_st.lost,
                    g_edmg_st.local_only, (uint64_t)g_edmg_pend.n, (uint64_t)pk, g_edmg_pend.evicted, g_edmg_pend.expired};
    int k = (int)(sizeof v / sizeof *v);
    if (k > n) k = n;
    memcpy(out, v, sizeof *v * (size_t)k);
    ReleaseSRWLockExclusive(&g_enemy_lock);
    return k;
}

// s38 animals: hle_override goes by function address and is checked before the per-thread dcache fills, so one added
// at run time isn't reliable. Instead, the first actor whose damage manager vtable has another addDamage (slot 47)
// gets THAT vtable slot patched (guest memory) to one of DMG_STUBS hle_register()ed stubs, each running the same
// queue logic over the slot's original function.
#define DMG_STUBS 4
#define DMG_VT_ADD 47
static struct { uint64_t vt, orig, stub; GuestFn fn; } g_dmg_stub[DMG_STUBS];
static int g_ndmg_stub;
static SRWLOCK g_dmg_stub_lock = SRWLOCK_INIT;
static void dmg_stub0(Cpu *c) { dmg_add_common(c, g_dmg_stub[0].fn); }
static void dmg_stub1(Cpu *c) { dmg_add_common(c, g_dmg_stub[1].fn); }
static void dmg_stub2(Cpu *c) { dmg_add_common(c, g_dmg_stub[2].fn); }
static void dmg_stub3(Cpu *c) { dmg_add_common(c, g_dmg_stub[3].fn); }

static void dmg_vtable_patch(uint64_t a) {
    uint64_t dm = *(const uint64_t *)(uintptr_t)(a + ACTOR_DAMAGE_MGR);
    if (!g_dmg_add || !in_guest(dm)) return;
    uint64_t vt = *(const uint64_t *)(uintptr_t)dm;
    if (!in_guest(vt)) return;
    uint64_t *slot = (uint64_t *)(uintptr_t)(vt + 8 * DMG_VT_ADD), fn = *slot;
    if (fn == DMG_ADD_ENEMY) return;
    AcquireSRWLockExclusive(&g_dmg_stub_lock);
    int i;
    for (i = 0; i < g_ndmg_stub && g_dmg_stub[i].vt != vt && g_dmg_stub[i].stub != fn; i++) {}
    GuestFn of = i == g_ndmg_stub && i < DMG_STUBS ? dispatch_lookup(fn) : NULL;
    if (of && g_dmg_stub[i].stub) {
        g_dmg_stub[i].vt = vt, g_dmg_stub[i].orig = fn, g_dmg_stub[i].fn = of;
        g_ndmg_stub++;
        DWORD old;
        if (VirtualProtect(slot, 8, PAGE_READWRITE, &old)) {
            *slot = g_dmg_stub[i].stub;
            VirtualProtect(slot, 8, old, &old);
            LOG("[linkpos] animal damage vtable %llx slot 47 %llx -> stub %d (%s)\n", (unsigned long long)vt,
                (unsigned long long)fn, i, actor_name(a));
        } else
            LOG("[linkpos] animal damage vtable %llx: VirtualProtect failed (%lu)\n", (unsigned long long)vt, GetLastError());
    } else if (i == g_ndmg_stub) {
        static int warned;
        if (!warned++) LOG("[linkpos] damage vtable %llx slot 47 %llx (%s): no stub (%s)\n", (unsigned long long)vt,
                           (unsigned long long)fn, actor_name(a), i == DMG_STUBS ? "all used" : "no function there");
    }
    ReleaseSRWLockExclusive(&g_dmg_stub_lock);
}

static uint64_t g_track_actor, g_track_until;  // the last DAMAGE_TEST enemy, traced per frame (enemy_frame)

static void dmg_test_frame(void) {  // (SAVAGEWIND_LINK_DAMAGE_TEST)
    static const char *spec;
    static int init;
    static uint64_t done;
    if (!init) init = 1, spec = getenv("SAVAGEWIND_LINK_DAMAGE_TEST");
    if (!spec) return;
    extern uint64_t vk_frame_counter(void);
    uint64_t present = vk_frame_counter();
    int idx = 0;
    for (const char *w = spec; *w && idx < 64; idx++) {
        unsigned long long at;
        int n, f54 = 0, f40 = 0;
        if (sscanf(w, "%llu:%d:%i:%i", &at, &n, &f54, &f40) >= 2 && present >= at && !(done >> idx & 1)) {
            done |= 1ull << idx;
            uint64_t e = enemy_nearest();
            if (e) linkpos_damage_enemy_ex(e, n, f54, f40), g_track_actor = e, g_track_until = g_vel_frame_id + 90;
        }
        const char *comma = strchr(w, ',');
        if (!comma) break;
        w = comma + 1;
    }
}

// Fall damage: the gatherer FUN_7100a7a010 (DAMAGE_ARGS_RECON.md: no direct callers) returns 1 with f50 0x15 / f54 0x20
// when the fall was higher than the threshold (GParam +0xe28); Link's damage AI then plays the land damage or the
// death (godmode can't stop that: it fires on the hit itself). KH2 has no fall damage, so in Sora mode it returns 0.
//   SAVAGEWIND_LINK_FALL_DAMAGE=1   keep BotW's fall damage in Sora mode
#define DMG_FALL_GATHER (SS->dmg_fall_gather)
static GuestFn g_fall_gather;
static int fall_drop(void) {
    static int drop = -1;
    if (drop < 0) drop = getenv("SAVAGEWIND_SORA") && !env_int("SAVAGEWIND_LINK_FALL_DAMAGE", 0);
    return drop;
}
static void fall_gather_hook(Cpu *c) {
    static int seen;
    if (!seen++) LOG("[linkpos] fall gatherer called (first time), frame %llu\n", (unsigned long long)g_vel_frame_id);
    int drop = fall_drop();
    g_fall_gather(c);
    if (drop && (uint32_t)c->x[0]) {
        c->x[0] = 0;
        LOG("[linkpos] fall damage dropped (Sora mode), frame %llu\n", (unsigned long long)g_vel_frame_id);
    }
}

// live_s34: the gatherer hook above never fired while a 50 m fall still took life 12 -> 2 on landing, so the hit comes
// another way. The player's addDamage override FUN_7100bb1adc (DAMAGE_ARGS_RECON.md; same args as the enemies' one):
// in Sora mode every nonzero call is logged and a fall (f54 0x20 or f50 0x15) gets damage 0 and f54 -1 (= no hit).
#define DMG_ADD_PLAYER (SS->dmg_add_player)
static GuestFn g_dmg_add_player;
// (s88 b3-guard) KH2 Guard: sora.c's linkpos_guard_set(1) each frame the guard blocks (the guard window facing the enemy,
// or the 80 invulnerable frames after a block); a hit on Link then gets damage 0 / f54 -1 (= no hit: no life lost, no
// flinch), as the fall damage does below. Counted for sora.c's block fx / sound / reflect. Stale (hot reload, a dead
// DLL) after 3 frames. Live: SAVAGEWIND_SORA's "set guard 0" never sets it.
static volatile int g_guard_set; static volatile uint64_t g_guard_frame; static volatile int g_guard_blocks, g_guard_dmg;
void linkpos_guard_set(int on) { g_guard_set = on, g_guard_frame = g_vel_frame_id; }
int linkpos_guard_blocks(void) { return g_guard_blocks; }
int linkpos_guard_last_damage(void) { return g_guard_dmg; }
// (s104) KH2 Second Chance / Once More, clamped HERE (before applyDamage takes the frame's damage off his life, so BotW
// never starts a death; the s103 DLL write after the fact could land mid-death). sora_chestabil.h calls
// linkpos_survive_set(mask) every frame: bit 0 = Second Chance (a hit that would take him from > 1 quarter heart to 0
// leaves 1), bit 4 (s110, sora_mickey.h) = ANY fatal hit leaves 1 (even a lone one at 1 quarter heart: the KH2 Mickey rescue), bit 1 = Once More (at 1 quarter heart, a hit within a combo = SV_COMBO_MS of the combo's first hit, each hit
// < SV_GAP_MS after the one before, leaves 1). KH2: both unlimited, no re-arm. Stale after 3 frames (hot reload, dead DLL).
// The frame's damage is summed (several addDamage calls a frame). Falls (dropped above in Sora mode) come in as 0.
#define SV_GAP_MS 1500
#define SV_COMBO_MS 3000
static volatile int g_surv_mask, g_surv_n, g_surv_what;
static volatile uint64_t g_surv_frame;
void linkpos_survive_set(int mask) { g_surv_mask = mask, g_surv_frame = g_vel_frame_id; }
int linkpos_survive_count(int *what) { if (what) *what = g_surv_what; return g_surv_n; }
static void survive_clamp(Cpu *c) {
    static uint64_t sum_frame, last_ms, chain_ms;
    static int sum;
    int32_t dmg = (int32_t)c->x[2];
    if (dmg <= 0) return;
    uint64_t now = GetTickCount64();
    bool chain = last_ms && now - last_ms < SV_GAP_MS && now - chain_ms <= SV_COMBO_MS;
    if (!chain) chain_ms = now;
    last_ms = now;
    if (g_vel_frame_id != sum_frame) sum_frame = g_vel_frame_id, sum = 0;
    uint64_t dm = c->x[0], actor = in_guest(dm) ? *(const uint64_t *)(uintptr_t)(dm + DM_ACTOR) : 0;
    int mask = g_vel_frame_id - g_surv_frame <= 3 ? g_surv_mask : 0;
    if (mask && in_guest(actor)) {
        int left = *(const int32_t *)(uintptr_t)(actor + ACTOR_LIFE) - sum;
        if (left >= 1 && dmg >= left) {
            int what = left > 1 && (mask & 1) ? 1 : left == 1 && (mask & 2) && chain ? 2 : (mask & 4) ? 3 : 0;  // (s110 b22-mickey: mask 4 = ANY fatal hit leaves 1, also a lone one at 1 quarter heart; what 3)
            if (what) {
                int keep = left - 1;  // (0 at 1 quarter heart: f54 is kept, so the hit still flinches him)
                c->x[2] = (uint32_t)keep;
                if ((int32_t)c->x[3] > keep) c->x[3] = (uint32_t)keep;
                if ((int32_t)c->x[4] > keep) c->x[4] = (uint32_t)keep;
                g_surv_n++, g_surv_what = what;
                LOG("[linkpos] %s: hit %d on life %d -> %d taken (1 quarter heart left), f50 %#x f54 %#x\n",
                    what == 1 ? "Second Chance" : what == 2 ? "Once More" : "Mickey rescue (any fatal hit)", dmg, left, keep, (int32_t)c->x[5], (int32_t)c->x[6]);
                dmg = keep;
            }
        }
    }
    sum += dmg;
}
// (s109) KH2 Defense: sora_levels.h's sl_def_tick sends the percent BotW's hit on Link keeps (100 = unchanged) every
// frame; stale after 3 frames (hot reload, dead DLL) = 100. Rounded up like KH2, min 1; falls are not scaled.
static volatile int g_hurt_pct = 100;
static volatile uint64_t g_hurt_frame;
// (s120 armor) the DLL's KH2 Equipment page: the Armor_* actors worn by Link and drawn (opacity hook) within 1 s of the newest such sighting, names
// copied into out[max][40] (no dups); -> count. Link's armor actors carry his BaseProcLinkData (is_link_equip). Any thread.
int linkpos_link_armor(char (*out)[40], int max) {
    int n = 0;
    DWORD newest = 0;
    AcquireSRWLockShared(&g_equip_lock);
    // (s120 fix) relative to the NEWEST armor sighting, not the clock: in the KH2 pause menu BotW may stop drawing Link, so
    // the last worn set stays valid; a piece taken off stops being seen while the new one is, and drops out after 1 s
    for (int i = 0; i < g_nequip; i++) if (g_equip[i].armor[0] && (int)(g_equip[i].seen - newest) > 0) newest = g_equip[i].seen;
    for (int i = 0; i < g_nequip && n < max; i++) {
        if (!g_equip[i].armor[0] || newest - g_equip[i].seen > 1000) continue;
        int k = 0;
        while (k < n && strcmp(out[k], g_equip[i].armor)) k++;
        if (k == n) memcpy(out[n++], g_equip[i].armor, 40);
    }
    ReleaseSRWLockShared(&g_equip_lock);
    return n;
}
void linkpos_hurt_scale_set(int pct) { g_hurt_pct = pct < 0 ? 0 : pct > 1000 ? 1000 : pct, g_hurt_frame = g_vel_frame_id; }
static void hurt_scale(Cpu *c) {
    int pct = g_vel_frame_id - g_hurt_frame <= 3 ? g_hurt_pct : 100;
    int32_t dmg = (int32_t)c->x[2], f50 = (int32_t)c->x[5], f54 = (int32_t)c->x[6];
    if (pct == 100 || dmg <= 0 || f54 == 0x20 || f50 == 0x15 || (f50 == 0xf && (f54 == 0x19 || f54 == 0x1a))) return;
    int32_t d = (int32_t)(((int64_t)dmg * pct + 99) / 100);
    if (d < 1) d = 1;
    c->x[2] = (uint32_t)d;
    if ((int32_t)c->x[3] > d) c->x[3] = (uint32_t)d;
    if ((int32_t)c->x[4] > d) c->x[4] = (uint32_t)d;
    LOG("[linkpos] khdef: hit %d -> %d (x%d%%) f50 %#x f54 %#x\n", dmg, d, pct, f50, f54);
}
static void dmg_add_player_hook(Cpu *c) {
    int drop = fall_drop();
    int32_t dmg = (int32_t)c->x[2], f50 = (int32_t)c->x[5], f54 = (int32_t)c->x[6];
    if (g_guard_set && g_vel_frame_id - g_guard_frame <= 3 && dmg > 0 &&
        !(f54 == 0x20 || f50 == 0x15 || (f50 == 0xf && (f54 == 0x19 || f54 == 0x1a)))) {  // (not a fall)
        // (s119) guard_set(2) = Mickey mode: a LARGE hit (f54 0x15 explosion / 0x16 / 0xa: the kinds BotW's player hit
        // handler FUN_7100bd59c8 reacts to without a life loss, DAMAGE_ARGS_RECON.md) keeps its type with damage 0, so BotW
        // knocks Link down / ragdolls him and Mickey copies it (sora_ragmap_mickey.h). 1 = the whole hit dropped (KH2 Guard).
        bool keep = g_guard_set == 2 && (f54 == 0x15 || f54 == 0x16 || f54 == 0xa);
        g_guard_blocks++, g_guard_dmg = dmg;
        LOG("[linkpos] Guard blocked: player addDamage damage %d f50 %#x f54 %#x f40 %d dropped%s, frame %llu\n", dmg, f50, f54,
            (int32_t)c->x[7], keep ? " (reaction kept: Mickey)" : "", (unsigned long long)g_vel_frame_id);
        c->x[2] = c->x[3] = c->x[4] = 0;
        if (!keep) c->x[5] = 0, c->x[6] = (uint32_t)-1;
        g_dmg_add_player(c);
        return;
    }
    if (drop && (dmg || f54 != -1)) {
        // fall_menu_s34p: a 40 m fall (BotW's own fall, the servo saw no walk-off) killed him with a2 5 damage 12 f50 0xf
        // f54 0x19 f40 4 (not the gatherer's 0x15 / 0x20) - that combination counts as the fall too; live_s35: a treetop
        // drop came as damage 96 f50 0xf f54 0x1a f40 4 (the lethal-height kind) and killed him
        bool fall = f54 == 0x20 || f50 == 0x15 || (f50 == 0xf && (f54 == 0x19 || f54 == 0x1a));
        LOG("[linkpos] player addDamage a2 %llx damage %d df48 %d minDmg %d f50 %#x f54 %#x f40 %d lr %llx%s\n",
            (unsigned long long)c->x[1], dmg, (int32_t)c->x[3], (int32_t)c->x[4], f50, f54, (int32_t)c->x[7],
            (unsigned long long)c->x[30], fall ? " -> FALL, dropped" : "");
        if (fall) c->x[2] = c->x[3] = c->x[4] = 0, c->x[5] = 0, c->x[6] = (uint32_t)-1;
    }
    hurt_scale(c);     // (s109 KH2 Defense, before the survive clamp sees the damage)
    survive_clamp(c);  // (s104 Second Chance / Once More)
    g_dmg_add_player(c);
}

// (s90b) Guard sends the Guardian beam back (BotW's shield parry; recon/S90_HELPER_PARRY.md option B). The beam's fire
// action asks vslot 0x100 = BEAM_REFLECT_CHK(this, rec) "(rec+0x18 & 0xA) != 0" (just guard / shield mirror) after it
// touched something; 1 = BotW's own reflect (direction negated, speed x0.3, layer swapped so it hits enemies). Our Guard
// sets none of Link's guard flags, so it said 0 and the beam exploded on Sora. Hook: say 1 while sora.c's guard WINDOW is
// live (linkpos_guard_reflect_set each frame; not the 80 invulnerable frames), the hit point rec+0..8 is within `rad` of
// Sora, and (face not NaN) the beam's travel direction this+0x40..0x48 (GUESS layout) comes at his front. Stale after 3
// frames. Any other case = the original. Called on the beam's job, so only values the DLL stored are read here.
#define BEAM_REFLECT_CHK (SS->beam_reflect_chk)
static GuestFn g_beam_reflect_chk;
static volatile int g_refl_on, g_refl_n;
static volatile uint64_t g_refl_frame;
static float g_refl_p[3], g_refl_face, g_refl_rad;
void linkpos_guard_reflect_set(int on, const float p[3], float face, float rad) {
    if (on && p) memcpy(g_refl_p, p, 12), g_refl_face = face, g_refl_rad = rad;
    g_refl_frame = g_vel_frame_id, g_refl_on = on;
}
int linkpos_guard_reflects(void) { return g_refl_n; }
static void beam_reflect_hook(Cpu *c) {
    uint64_t self = c->x[0], rec = c->x[1];
    if (g_refl_on && g_vel_frame_id - g_refl_frame <= 3 && rec && readable(rec, 0x1c) == 0x1c) {
        const float *h = (const float *)(uintptr_t)rec;
        float dx = h[0] - g_refl_p[0], dy = h[1] - g_refl_p[1], dz = h[2] - g_refl_p[2];
        float d = sqrtf(dx * dx + dy * dy + dz * dz), dot = 0;
        bool close_by = d <= g_refl_rad, front = true;
        if (close_by && g_refl_face == g_refl_face && self && readable(self + 0x40, 12) == 12) {  // (NaN face = no front test)
            const float *v = (const float *)(uintptr_t)(self + 0x40);
            dot = v[0] * sinf(g_refl_face) + v[2] * cosf(g_refl_face);  // < 0 = flying against his facing = at his front
            front = !(v[0] * v[0] + v[2] * v[2] > 1e-6f) || dot < 0;   // (no horizontal direction: no test)
        }
        static int miss;
        if (close_by && front) {
            g_refl_n++;
            LOG("[linkpos] Guard REFLECTS beam %llx (rec flags %#x, hit %.2f m from Sora, dir.face %.2f) #%d frame %llu\n",
                (unsigned long long)self, *(const uint8_t *)(uintptr_t)(rec + 0x18), d, dot, g_refl_n,
                (unsigned long long)g_vel_frame_id);
            c->x[0] = 1;
            return;
        }
        if (miss++ < 40)
            LOG("[linkpos] Guard window, beam %llx NOT reflected: hit %.2f m from Sora (max %.2f), dir.face %.2f%s\n",
                (unsigned long long)self, d, g_refl_rad, dot, front ? "" : " (from behind)");
    }
    g_beam_reflect_chk(c);
}

static void dmg_init(void) {  // (always on: the hook is how Sora's hits land; LOG=1 nonzero calls, 2 every call)
    if (BEAM_REFLECT_CHK && (g_beam_reflect_chk = dispatch_lookup(BEAM_REFLECT_CHK)))
        hle_override(BEAM_REFLECT_CHK, beam_reflect_hook, "linkpos beam reflect check");
    else LOG("[linkpos] no function at %llx: Guard does not reflect beams\n", (unsigned long long)BEAM_REFLECT_CHK);
    if ((g_dmg_add_player = dispatch_lookup(DMG_ADD_PLAYER))) hle_override(DMG_ADD_PLAYER, dmg_add_player_hook, "linkpos player addDamage");
    else LOG("[linkpos] no function at %llx: no player addDamage hook\n", DMG_ADD_PLAYER);
    if ((g_fall_gather = dispatch_lookup(DMG_FALL_GATHER))) hle_override(DMG_FALL_GATHER, fall_gather_hook, "linkpos fall damage");
    else LOG("[linkpos] no function at %llx: fall damage stays\n", DMG_FALL_GATHER);
    g_edmg_on = env_int("SAVAGEWIND_LINK_EDMG", 0) != 0, g_edmg_log = env_int("SAVAGEWIND_LINK_EDMG_LOG", 0) != 0;
    // (an in-place reboot runs linkpos_init again: the old world's hits, echoes and pending hits mean nothing now; review E1 SHOULD-FIX 3)
    AcquireSRWLockExclusive(&g_enemy_lock);
    g_nedmg = 0, g_ndmg_q = 0;
    memset(&g_edmg_echo, 0, sizeof g_edmg_echo), memset(&g_edmg_pend, 0, sizeof g_edmg_pend), memset(&g_edmg_argt, 0, sizeof g_edmg_argt);
    ReleaseSRWLockExclusive(&g_enemy_lock);
    if (g_edmg_on) {
        if (DMG_APPLY_ENEMY && (g_dmg_apply = dispatch_lookup(DMG_APPLY_ENEMY))) {
            hle_override(DMG_APPLY_ENEMY, dmg_apply_hook, "linkpos applyDamage (edmg)");
            LOG("[linkpos] edmg capture on (applyDamage %llx)%s\n", (unsigned long long)DMG_APPLY_ENEMY, g_edmg_log ? ", log on" : "");
        } else { g_edmg_on = 0; LOG("[linkpos] edmg: no function at %llx: capture off\n", (unsigned long long)DMG_APPLY_ENEMY); }
    }
    if (!(g_dmg_add = dispatch_lookup(DMG_ADD_ENEMY))) { LOG("[linkpos] no function at %llx: no addDamage hook\n", DMG_ADD_ENEMY); return; }
    hle_override(DMG_ADD_ENEMY, dmg_add_hook, "linkpos addDamage");
    static const HostFn stubs[DMG_STUBS] = {dmg_stub0, dmg_stub1, dmg_stub2, dmg_stub3};
    static const char *const names[DMG_STUBS] = {"<linkpos addDamage vt 0>", "<linkpos addDamage vt 1>",
                                                 "<linkpos addDamage vt 2>", "<linkpos addDamage vt 3>"};
    for (int i = 0; i < DMG_STUBS; i++) g_dmg_stub[i].stub = hle_register(names[i], stubs[i]);  // (startup thread)
}

// (s55 levels, slice 2) The enemy list's name for an actor (copied under the lock): its length, 0 = not in the list.
int linkpos_enemy_name(uint64_t actor, char *buf, int n) {
    if (!buf || n <= 0) return 0;
    buf[0] = 0;
    int k = 0;
    AcquireSRWLockExclusive(&g_enemy_lock);
    for (int i = 0; i < g_nenemy; i++)
        if (g_enemy[i].actor == actor) {
            snprintf(buf, (size_t)n, "%s", g_enemy[i].name);
            k = (int)strlen(buf);
            break;
        }
    ReleaseSRWLockExclusive(&g_enemy_lock);
    return k;
}

// Enemy deaths (s55 levels, slice 2; recon/S55_HELPER_LEVELS23.md). Actor vtable slot 54 (+0x1b0) is the die call:
// the enemies' override FUN_71004682bc (in 8 vtables, the Bokoblin's 0x7101a63fd0 and the Chuchu's 0x7101a65af0 among
// them) sends the PlayReport "KillEnemy" (name, map, position: FUN_7100ad10c0), bumps Defeated_<name>_Num (cap 10:
// FUN_7100ac6840) and ends in the base Actor slot 54 FUN_710141c4a8 (the delete request); two more overrides
// (FUN_710047bd60, FUN_7100684c6c) call it with a bl. The AI caller FUN_710048ca5c calls slot 54 (reason 0) only once
// getLife() <= 0. Hooks: hle_override on both functions (the vtable calls: 8 + 55 vtables) + hook_calls on the two bl
// sites; the base one only queues actors with enemy-list names (Enemy_* / Animal_*, e.g. wolves and bears).
// linkpos_enemy_died() hands the queue to sora_mod.dll (exact deaths, off-screen ones too, with the name).
//   SAVAGEWIND_LINK_DIE_LOG=1   also log the slot 54 calls of every other actor (any name)
//   SAVAGEWIND_LINK_NO_DIE_HOOK=1   leave the hooks out
#define DIE_ENEMY (SS->die_enemy)
#define DIE_BASE (SS->die_base)
#define DIE_Q 32
typedef struct { uint64_t actor, frame; int32_t life, kind, reason; char name[48]; } LinkposDied;  // kind 1 enemy, 2 base
static LinkposDied g_died[DIE_Q];
static int g_ndied;
static uint64_t g_died_total, g_died_lost;
static GuestFn g_die_enemy, g_die_base;
static int redir_drop(uint64_t enemy, const char *why);  // (redirect section, s56)

static void die_note(Cpu *c, int kind) {
    uint64_t a = c->x[0];
    const char *name = in_guest(a) ? actor_name(a) : NULL;
    static int log_all = -1;
    if (log_all < 0) log_all = env_int("SAVAGEWIND_LINK_DIE_LOG", 0);
    if (!name || !enemy_name_ok(name)) {
        if (log_all) LOG("[linkpos] die slot 54 (%s) %llx %s reason %d: not an enemy name, not queued\n",
                         kind == 1 ? "enemy" : "base", (unsigned long long)a, name ? name : "?", (int32_t)c->x[1]);
        return;
    }
    int32_t life = *(const int32_t *)(uintptr_t)(a + ACTOR_LIFE);
    bool queued = false;
    uint64_t lost, total;
    AcquireSRWLockExclusive(&g_enemy_lock);
    int i;
    for (i = 0; i < g_ndied && !(g_died[i].actor == a && g_died[i].frame == g_vel_frame_id); i++) {}
    if (i == g_ndied) {  // (once per actor and frame; a full queue drops its oldest)
        if (g_ndied == DIE_Q) memmove(g_died, g_died + 1, sizeof *g_died * (DIE_Q - 1)), g_ndied--, g_died_lost++;
        LinkposDied *d = &g_died[g_ndied++];
        d->actor = a, d->frame = g_vel_frame_id, d->life = life, d->kind = kind, d->reason = (int32_t)c->x[1];
        snprintf(d->name, sizeof d->name, "%s", name);
        g_died_total++, queued = true;
    }
    lost = g_died_lost, total = g_died_total;
    ReleaseSRWLockExclusive(&g_enemy_lock);
    redir_drop(a, "died");
    if (queued)
        LOG("[linkpos] died %llx %s life %d reason %d (%s slot 54) game frame %llu, #%llu%s\n", (unsigned long long)a, name,
            life, (int32_t)c->x[1], kind == 1 ? "enemy" : "base", (unsigned long long)g_vel_frame_id,
            (unsigned long long)total, lost ? " (queue overflowed before)" : "");
}
static void die_enemy_hook(Cpu *c) { die_note(c, 1); g_die_enemy(c); }
static void die_base_hook(Cpu *c) { die_note(c, 2); g_die_base(c); }

// Drains up to max queued deaths into out (oldest first): the count; -1 = another struct size (DLL / exe mismatch).
int linkpos_enemy_died(LinkposDied *out, int max, int size) {
    if (size != (int)sizeof(LinkposDied) || !out || max <= 0) return -1;
    AcquireSRWLockExclusive(&g_enemy_lock);
    int n = g_ndied < max ? g_ndied : max;
    memcpy(out, g_died, sizeof *g_died * (size_t)n);
    memmove(g_died, g_died + n, sizeof *g_died * (size_t)(g_ndied - n));
    g_ndied -= n;
    ReleaseSRWLockExclusive(&g_enemy_lock);
    return n;
}

static void die_init(void) {  // (startup, after dmg_init: before any guest code runs)
    if (getenv("SAVAGEWIND_LINK_NO_DIE_HOOK")) { LOG("[linkpos] die hooks off (SAVAGEWIND_LINK_NO_DIE_HOOK)\n"); return; }
    if ((g_die_base = dispatch_lookup(DIE_BASE))) hle_override(DIE_BASE, die_base_hook, "linkpos actor die (base)");
    else LOG("[linkpos] no function at %llx: no base die hook\n", DIE_BASE);
    if (!(g_die_enemy = dispatch_lookup(DIE_ENEMY))) { LOG("[linkpos] no function at %llx: no enemy die hook\n", DIE_ENEMY); return; }
    hle_override(DIE_ENEMY, die_enemy_hook, "linkpos enemy die");
    int n = hook_calls(SS->die_enemy_caller_a, DIE_ENEMY, die_enemy_hook) + hook_calls(SS->die_enemy_caller_b, DIE_ENEMY, die_enemy_hook);
    LOG("[linkpos] die hooks: enemy %llx (+%d of 2 bl sites), base %llx\n", DIE_ENEMY, n, DIE_BASE);
}

static void enemy_frame(void) {  // (game thread, once a frame: drop stale entries, the periodic log)
    static int log_on = -1;
    if (log_on < 0) log_on = env_int("SAVAGEWIND_LINK_ENEMY_LOG", 0);
    AcquireSRWLockExclusive(&g_enemy_lock);
    for (int i = 0; i < g_ndmg_q;)  // (s166 E2: a remote hit whose enemy never got its addDamage call goes back to the pending map)
        if ((g_dmg_q[i].rn > 0 || g_dmg_q[i].rkill) && g_vel_frame_id - g_dmg_q[i].rframe > EDMG_RIDE_TTL) {
            uint32_t h = g_dmg_q[i].rhid;  // (s167 SF-1: kept in the queue entry, the row may be gone already)
            if (h) edmg_pend_add(&g_edmg_pend, h, g_dmg_q[i].rn, g_dmg_q[i].f54, g_dmg_q[i].f40, g_dmg_q[i].rkill, g_dmg_q[i].rframe);
            g_edmg_st.ride_expired++;
            if (g_dmg_q[i].n > 0) g_dmg_q[i].rn = 0, g_dmg_q[i].rkill = 0, i++;  // (a local hit of Sora's stays)
            else g_dmg_q[i] = g_dmg_q[--g_ndmg_q];
        } else i++;
    if (g_edmg_pend.n && g_vel_frame_id % 600 == 0) edmg_pend_expire(&g_edmg_pend, g_vel_frame_id);
    for (int i = 0; i < g_nenemy;)
        if (g_vel_frame_id - g_enemy[i].seen > ENEMY_STALE) g_enemy[i] = g_enemy[--g_nenemy];
        else i++;
    if (log_on && g_vel_frame_id % (getenv("SAVAGEWIND_LINK_DAMAGE_TEST") ? 120 : 600) == 0) {
        LOG("[linkpos] enemies (game frame %llu): %d\n", (unsigned long long)g_vel_frame_id, g_nenemy);
        for (int i = 0; i < g_nenemy; i++)
            LOG("[linkpos]   %llx %s at %.2f %.2f %.2f dist %.1f\n", (unsigned long long)g_enemy[i].actor, g_enemy[i].name,
                g_enemy[i].pos[0], g_enemy[i].pos[1], g_enemy[i].pos[2], g_enemy[i].dist);
    }
    for (int i = 0; i < g_nenemy && g_vel_frame_id < g_track_until; i++)
        if (g_enemy[i].actor == g_track_actor)
            LOG("[linkpos] track %llx frame %llu at %.2f %.2f %.2f life %d\n", (unsigned long long)g_track_actor,
                (unsigned long long)g_vel_frame_id, g_enemy[i].pos[0], g_enemy[i].pos[1], g_enemy[i].pos[2], g_enemy[i].life);
    ReleaseSRWLockExclusive(&g_enemy_lock);
    dmg_test_frame();
}

// Link's life (life_s16a: the actor's +0x848 like the enemies', quarter hearts: 12 = 3 hearts; it steps down at each hit
// and reaches 0 at Game Over). -1: no Link.
//   SAVAGEWIND_LINK_GODMODE=1   tests: his life is put back to the highest value seen, every frame (the camp kills him
//                               in ~20 s otherwise)
int linkpos_link_life(void) { return servo_body() ? *(const int32_t *)(uintptr_t)(g_servo_actor + ACTOR_LIFE) : -1; }

// linkpos_link_hits(): how many times his life dropped (counted before godmode puts it back): sora.c's flinch.
static volatile int g_link_hits;
int linkpos_link_hits(void) { return g_link_hits; }

// linkpos_link_ragdoll(): 1 while BotW tumbles Link ("rolling": a big hit), 2 while he lies dead ("dead"), 0 not, -1 no
// Link / no ragdoll. RAGDOLL_RECON.md section 5: rag = [[link+0x578]+0x90], in the world (flags +0xc0 & 0x10), and the
// blend-weight state s8 [ps+0x112] = Player.brgbw's list position + 2 (the name lookup 0x71011e6d10 keeps 0 and 1 for
// two built-in names): 2 rolling, 3 dead, 4 climbing, 5 gliding (rag_s21a: the 12 -> 2 hit = 2, the fall death = 3).
int linkpos_link_ragdoll(void) {
    if (!servo_body()) return -1;
    uint64_t ps = *(const uint64_t *)(uintptr_t)(g_servo_actor + 0x578);
    if (!in_guest(ps)) return -1;
    uint64_t rag = *(const uint64_t *)(uintptr_t)(ps + 0x90);
    if (!in_guest(rag)) return -1;
    int st = *(const int8_t *)(uintptr_t)(ps + 0x112);
    return (*(const uint32_t *)(uintptr_t)(rag + 0xc0) & 0x10) && (st == 2 || st == 3) ? st - 1 : 0;
}

static int32_t g_life_max;  // the highest life seen (max life is not read yet): life_frame, linkpos_link_heal

// linkpos_link_heal(q): Cure. q quarter hearts onto his life, at most the highest life seen. Returns the new life.
int linkpos_link_heal(int q) {
    if (!servo_body()) return -1;
    int32_t *life = (int32_t *)(uintptr_t)(g_servo_actor + ACTOR_LIFE), v = *life + q;
    if (*life <= 0) return *life;  // (no heal from the dead)
    *life = v > g_life_max ? g_life_max : v;
    LOG("[linkpos] heal %d: life %d\n", q, *life);
    return *life;
}

static void life_frame(void) {
    static int god = -1, rag_was = -2;
    static int32_t last = -1;
    if (god < 0) god = env_int("SAVAGEWIND_LINK_GODMODE", 0);
    if (!servo_body()) { last = -1; return; }
    int rag = linkpos_link_ragdoll();
    if (rag != rag_was) {
        uint64_t ps = *(const uint64_t *)(uintptr_t)(g_servo_actor + 0x578);
        LOG("[linkpos] ragdoll %d (frame %llu, state word %#x, ps+0x112 %d)\n", rag, (unsigned long long)g_vel_frame_id,
            linkpos_link_state_word(), in_guest(ps) ? *(const int8_t *)(uintptr_t)(ps + 0x112) : -99);
        rag_was = rag;
    }
    // SAVAGEWIND_LINK_RAG_LOG=1: the raw fields behind it, on every change (session 21: it never reported 1)
    static int rag_log = -1;
    if (rag_log < 0) rag_log = env_int("SAVAGEWIND_LINK_RAG_LOG", 0);
    if (rag_log) {
        static uint64_t was_rg = 1;
        static uint32_t was_fl = 0xffffffff;
        static int was_st = -100;
        uint64_t ps = *(const uint64_t *)(uintptr_t)(g_servo_actor + 0x578);
        uint64_t rg = in_guest(ps) ? *(const uint64_t *)(uintptr_t)(ps + 0x90) : 0;
        uint32_t fl = in_guest(rg) ? *(const uint32_t *)(uintptr_t)(rg + 0xc0) : 0;
        int st = in_guest(ps) ? *(const int8_t *)(uintptr_t)(ps + 0x112) : -99;
        if (rg != was_rg || fl != was_fl || st != was_st)
            LOG("[linkpos] rag raw: ps %llx rag %llx flags %#x state %d (frame %llu, state word %#x, life %d)\n",
                (unsigned long long)ps, (unsigned long long)rg, fl, st, (unsigned long long)g_vel_frame_id,
                linkpos_link_state_word(), *(const int32_t *)(uintptr_t)(g_servo_actor + ACTOR_LIFE));
        was_rg = rg, was_fl = fl, was_st = st;
    }
    int32_t *life = (int32_t *)(uintptr_t)(g_servo_actor + ACTOR_LIFE);
    if (last >= 0 && *life < last) {
        g_link_hits++;
        LOG("[linkpos] Link hit: life %d -> %d (frame %llu, state word %#x)\n", last, *life,
            (unsigned long long)g_vel_frame_id, linkpos_link_state_word());
    }
    if (*life > g_life_max) g_life_max = *life;
    if (god && *life > 0 && *life < g_life_max) *life = g_life_max;
    last = *life;
}

static void break_note(uint64_t a);  // (breakables section, s55)
static void tree_note(uint64_t a), stasis_note(uint64_t a);  // (world section, s122)
static void fire_note(uint64_t a);   // (fire world section, s59)

static bool rune_obj_shown(uint64_t a) {  // (s100d) Obj_RemoteBomb* (ball, cube, Lv2): only Link makes them
    static int off = -1;
    if (off < 0) { const char *e = getenv("SAVAGEWIND_LINK_BOMB_HIDE"); off = e && *e && atoi(e) ? 1 : 0; }
    if (off) return false;
    const char *n = actor_name(a);
    // (s102 live: no bombshow line ever while holding a bomb: GUESS the actors are RemoteBomb / RemoteBomb2 /
    // RemoteBombCube / RemoteBombCube2, no Obj_ prefix; both spellings match now; any other *Bomb* name logged once)
    if (n && strstr(n, "Bomb") && strncmp(n, "RemoteBomb", 10) && strncmp(n, "Obj_RemoteBomb", 14)) {
        static int said;
        if (said++ < 4) LOG("[linkpos] bombshow: other Bomb actor %s %llx (not matched)\n", n, (unsigned long long)a);
    }
    if (!n || (strncmp(n, "Obj_RemoteBomb", 14) && strncmp(n, "RemoteBomb", 10))) return false;
    static unsigned logged;
    bh_note(a);
    if (logged++ == 0) LOG("[linkpos] bombshow: %s %llx kept visible (opacity factors %.2f %.2f %.2f before)\n", n, (unsigned long long)a,
                           opac(a, 0x4e8), opac(a, ACTOR_START_OPACITY), opac(a, ACTOR_FINAL_OPACITY));
    return true;
}

// (s108 slatexlu) The Slate held in rune mode was NOT in any 3D pass of the s107 F9 dump (frame 8908, Stasis: its projected
// box px 1203-1253 x 932-994 empty in every scene image) although its three opacity factors were written 1 before the
// apply (slatefade log) - only its glow showed = the "see-through blue ghost". applyOpacityIfDirty takes the alpha from
// the VIRTUAL getFinalOpacity (vtable +0x458): Link's class does not override it, the Slate's may (GUESS: an item class
// that multiplies in its holder's opacity, Link's ~0). Probe: the Slate's own getFinalOpacity value, logged (change-only,
// 0.05 steps). Fix: after BotW's apply, the ModelMgr apply again with alpha 1 (every material of the Slate's model,
// dither off). SAVAGEWIND_SLATE_XLU=0 = off (old), 2 = probe only.
static int g_slate_xlu = -1;
static float guest_float_ret(const Cpu *c) { float f; memcpy(&f, &c->v[0], 4); return f; }
static void slate_xlu_probe(Cpu *c, uint64_t a) {
    if (g_slate_xlu < 0) g_slate_xlu = env_int("SAVAGEWIND_SLATE_XLU", 1);
    if (!g_slate_xlu) return;
    uint64_t vt = *(const uint64_t *)(uintptr_t)a, fn = in_guest(vt) ? *(const uint64_t *)(uintptr_t)(vt + 0x458) : 0;
    if (!in_guest(fn) || !dispatch_lookup(fn)) return;
    Cpu save = *c;
    c->x[0] = a;
    guest_call(c, fn);
    float v = guest_float_ret(c);
    *c = save;
    static int q_last = -1, nlog;
    int q = (int)(v * 20 + 0.5f);
    if (q != q_last && nlog < 40) {
        nlog++, q_last = q;
        uint64_t lvt = g_servo_actor ? *(const uint64_t *)(uintptr_t)g_servo_actor : 0;
        uint64_t lfn = in_guest(lvt) ? *(const uint64_t *)(uintptr_t)(lvt + 0x458) : 0;
        LOG("[linkpos] slatexlu: Slate getFinalOpacity %.3f (its factors 4e8 %.3f 4ec %.3f 4f0 %.3f) vslot139 %llx (Link's %llx%s), Link state %#x\n",
            v, opac(a, 0x4e8), opac(a, ACTOR_START_OPACITY), opac(a, ACTOR_FINAL_OPACITY), (unsigned long long)fn, (unsigned long long)lfn,
            fn == lfn ? ", same" : ", OVERRIDDEN", linkpos_link_state_word());
    }
}
static void slate_xlu_force(Cpu *c, uint64_t a) {
    if (g_slate_xlu != 1 || !SS->model_alpha_apply || !SS->model_mgr_got) return;
    uint64_t model = *(const uint64_t *)(uintptr_t)(a + 0x4e0), got = SS->model_mgr_got;
    uint64_t mgrp = *(const uint64_t *)(uintptr_t)got, mgr = in_guest(mgrp) ? *(const uint64_t *)(uintptr_t)mgrp : 0;
    if (!in_guest(model) || !in_guest(mgr) || !dispatch_lookup(SS->model_alpha_apply)) return;
    Cpu save = *c;
    float one = 1.0f;
    uint32_t ob;
    memcpy(&ob, &one, 4);
    c->x[0] = mgr, c->x[1] = model, c->x[2] = 0, c->v[0] = ob;
    guest_call(c, SS->model_alpha_apply);
    *c = save;
    static int said;
    if (!said++) LOG("[linkpos] slatexlu: Slate model %llx alpha forced to 1 after BotW's apply (ModelMgr %llx)\n", (unsigned long long)model, (unsigned long long)mgr);
}

// (s108 chest) Treasure chests (actors named TBox_*) drawn near Link: the DLL's chest-open trigger (sora_chestanim.h:
// an event + a chest within reach) in place of the pelvis-drop guess that fired on a climb in s107.
static struct { SRWLOCK lock; uint64_t actor, frame; float pos[3], dist; char name[48]; } g_tbox = {SRWLOCK_INIT};
static void tbox_note(uint64_t a) {  // (opacity hook, any thread)
    if (!g_servo_actor || a == g_servo_actor) return;
    const char *n = actor_name(a);
    if (!n || strncmp(n, "TBox_", 5)) return;
    float d = link_dist(a);
    if (d > 6.0f) return;
    const float *m = (const float *)(uintptr_t)(a + ACTOR_MTX);
    AcquireSRWLockExclusive(&g_tbox.lock);
    if (g_tbox.frame != g_vel_frame_id || d < g_tbox.dist || g_tbox.actor == a) {
        if (g_tbox.actor != a) {
            static int nlog;
            if (nlog++ < 20) LOG("[linkpos] chest near: %s %llx at %.2f %.2f %.2f, %.2f m from Link\n", n, (unsigned long long)a, m[3], m[7], m[11], d);
        }
        g_tbox.actor = a, g_tbox.frame = g_vel_frame_id, g_tbox.dist = d;
        g_tbox.pos[0] = m[3], g_tbox.pos[1] = m[7], g_tbox.pos[2] = m[11];
        snprintf(g_tbox.name, sizeof g_tbox.name, "%s", n);
    }
    ReleaseSRWLockExclusive(&g_tbox.lock);
}
// The nearest TBox_* drawn within 6 m of Link this or the last frame: its distance (m), else -1. pos / name may be NULL.
float linkpos_tbox_near(float pos[3], char *name, int n) {
    AcquireSRWLockExclusive(&g_tbox.lock);
    float d = g_tbox.actor && g_vel_frame_id - g_tbox.frame <= 1 ? g_tbox.dist : -1.0f;
    if (d >= 0 && pos) memcpy(pos, g_tbox.pos, sizeof g_tbox.pos);
    if (d >= 0 && name && n > 0) snprintf(name, (size_t)n, "%s", g_tbox.name);
    ReleaseSRWLockExclusive(&g_tbox.lock);
    return d;
}

static void cam_override_late(void);  // (s137 fps: the FPS camera's late rewrite, camera section below)
static void opacity_apply(Cpu *c) {
    g_opacity_calls++;
    cam_override_late();
    uint64_t a = c->x[0];
    if (in_guest(a)) enemy_note(a), break_note(a), tree_note(a), stasis_note(a), fire_note(a), tbox_note(a);
    if (g_hide && g_servo_actor && a == g_servo_actor) {
        zero_opacity(a);
        if (g_opacity_hits++ == 0) LOG("[linkpos] opacity hook: hiding Link (actor %llx)\n", (unsigned long long)g_servo_actor);
    } else if (g_servo_actor && in_guest(a) && is_link_equip(a) && g_hide && !g_no_equip_hide) {
        if (slate_hip_wanted(a)) {  // (s94 b8-slatehip)
            slate_hip_apply(a);
            slate_xlu_probe(c, a);  // (s108 slatexlu)
            g_opacity_apply(c);
            slate_xlu_force(c, a);
            return;
        }
        if (wpn_hand_actor(a)) {  // (s128 wpn) Link's equipped weapon in Sora's hand: shown, not parked
            wpn_show(a);
            g_opacity_apply(c);
            slate_xlu_force(c, a);
            return;
        }
        gear_note(a);  // (s114 gearpark)
        zero_opacity(a);
    } else if (g_hide && g_servo_actor && in_guest(a) && rune_obj_shown(a)) {
        // (s100d) Remote Bombs: user saw only the blue glow over Sora's hands, no bomb (GUESS: the held bomb takes Link's
        // requested opacity 0 like his weapons do). Opacity back to 1, as for the Slate. SAVAGEWIND_LINK_BOMB_HIDE=1 = old.
        *(float *)(uintptr_t)(a + ACTOR_FINAL_OPACITY) = 1;
        *(float *)(uintptr_t)(a + ACTOR_START_OPACITY) = 1;
        *(uint8_t *)(uintptr_t)(a + ACTOR_OPACITY_DIRTY) = 1;
    }
    g_opacity_apply(c);
}

static void slate_model_init(void);
static void opacity_init(void) {
    if (getenv("SAVAGEWIND_LINK_NO_OPACITY_HOOK")) return;
    g_no_equip_hide = env_int("SAVAGEWIND_LINK_NO_EQUIP_HIDE", 0) != 0;
    if (!(g_opacity_apply = dispatch_lookup(OPACITY_APPLY))) { LOG("[linkpos] no function at %llx: no opacity hook\n", OPACITY_APPLY); return; }
    hle_override(OPACITY_APPLY, opacity_apply, "linkpos opacity apply");  // (indirect calls)
    for (size_t i = 0; i < 3; i++) {
        uint64_t fn = guest_fn_at(opacity_sites[i]);
        if (!fn || !hook_calls(fn, OPACITY_APPLY, opacity_apply))
            LOG("[linkpos] opacity hook: call at %llx not hooked (function %llx)\n", (unsigned long long)opacity_sites[i], (unsigned long long)fn);
    }
    slate_model_init();  // (s100 slatehip2)
}

// The camera: the stick is camera-relative. sead::LookAtCamera = {vtable, Matrix34f mMatrix (view, +0x8), Vector3f mPos
// (+0x38), mAt (+0x44), mUp (+0x50)}. Found by scanning resident guest memory (as SAVAGEWIND_LINK_FIND) for pos / at /
// up triples where up is a unit vector, at is near Link and pos is 0.5..60 units from at; candidates whose 12 floats
// before pos are a view matrix looking along at - pos rank first. Checked every frame (9 floats), scanned again when it
// stops matching for CAM_LOST frames (at most every CAM_RESCAN presented frames: a scan stalls the game ~1 s).
//   SAVAGEWIND_LINK_CAM=1    look for it from world entry on even without the driver asking
#define CAM_POS 0x38
#define CAM_LOST 15
#define CAM_RESCAN 300
#define CAM_MAX 32
static uint64_t g_cam, g_cams[CAM_MAX];
static int g_ncams, g_cam_score[CAM_MAX];
static bool g_cam_wanted;
static float g_cam_link[3];

static float len3(const float *a) { return sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]); }

// 0: not a camera here; 1: pos / at / up fit; 2: and a view matrix before it agrees.
static int cam_score_l(const float *f, bool with_matrix, const float *link) {
    const float *pos = f, *at = f + 3, *up = f + 6;
    for (int i = 0; i < 9; i++) if (!(f[i] > -1e5f && f[i] < 1e5f)) return 0;
    float ul = len3(up), d[3] = {pos[0] - at[0], pos[1] - at[1], pos[2] - at[2]};
    float dl = len3(d), l[3] = {at[0] - link[0], at[1] - link[1], at[2] - link[2]};
    if (!(ul > 0.98f && ul < 1.02f && up[1] > 0.3f && dl > 0.5f && dl < 60 && len3(l) < 6)) return 0;
    if (!with_matrix) return 1;
    const float *m = f - 12;  // rows: right, up, back (= (pos - at) / |pos - at| for a look-at view)
    for (int r = 0; r < 3; r++) {
        float rl = len3(m + 4 * r);
        if (!(rl > 0.98f && rl < 1.02f)) return 1;
    }
    float dot = (m[8] * d[0] + m[9] * d[1] + m[10] * d[2]) / dl;
    return dot > 0.99f || dot < -0.99f ? 2 : 1;
}
static int cam_score(const float *f, bool with_matrix) { return cam_score_l(f, with_matrix, g_cam_link); }

// The camera found by the scan, frame to frame (s51): BotW pulls it in close at a wall (pos - at under the scan's
// 0.5 u) and it lags Link on fast moves, so only "still a camera" is asked here. Before, cam_score's scan rule dropped
// it for good there and the rescan found nothing: no camera = Sora not drawn for CAM_RESCAN presents (the glide into a
// wall "disappears 5-10 s", live_s47: the same 722ea7ece0 matched again after it).
static bool cam_track(const float *f) {
    for (int i = 0; i < 9; i++) if (!(f[i] > -1e5f && f[i] < 1e5f)) return false;
    const float *pos = f, *at = f + 3, *up = f + 6;
    float ul = len3(up), d[3] = {pos[0] - at[0], pos[1] - at[1], pos[2] - at[2]};
    float l[3] = {at[0] - g_cam_link[0], at[1] - g_cam_link[1], at[2] - g_cam_link[2]};
    return ul > 0.98f && ul < 1.02f && up[1] > 0.3f && len3(d) > 0.01f && len3(d) < 100 && len3(l) < 30;
}
// (s58) looser: still a camera (finite, unit up, near Link) - BotW's close-in cave camera fails cam_track's rule
static bool cam_alive(const float *f) {
    for (int i = 0; i < 9; i++) if (!(f[i] > -1e5f && f[i] < 1e5f)) return false;
    float ul = len3(f + 6), l[3] = {f[3] - g_cam_link[0], f[4] - g_cam_link[1], f[5] - g_cam_link[2]};
    return ul > 0.95f && ul < 1.05f && len3(l) < 100;
}
static const char *cam_why(const float *f) {  // the first cam_track rule that fails (log only)
    for (int i = 0; i < 9; i++) if (!(f[i] > -1e5f && f[i] < 1e5f)) return "not finite";
    float ul = len3(f + 6), d[3] = {f[0] - f[3], f[1] - f[4], f[2] - f[5]};
    float l[3] = {f[3] - g_cam_link[0], f[4] - g_cam_link[1], f[5] - g_cam_link[2]};
    return !(ul > 0.98f && ul < 1.02f) ? "up not unit" : !(f[7] > 0.3f) ? "up tilted" : !(len3(d) > 0.01f) ? "pos = at"
         : !(len3(d) < 100) ? "pos-at > 100" : !(len3(l) < 30) ? "at > 30 u from Link" : "ok";
}

// The projection (Sora's draw pass, runtime/sora_draw.c): sead::PerspectiveProjection = {Projection (vtable, dirty
// flags, Matrix44f mMatrix, mDeviceMatrix, posture, z scale / offset), f32 mNear, mFar, mAngle (fovy, rad), mFovySin,
// mFovyCos, mFovyTan (of fovy / 2), mAspect, Vector2f mOffset}. Found in the camera's scan by the sin / cos / tan
// triple; *proj points at mNear. Several exist (shadow and UI views too): plausible gameplay values rank by aspect.
#define PROJ_MAX 16
static uint64_t g_proj, g_projs[PROJ_MAX];
static int g_nprojs;

static bool proj_ok(const float *f) {  // f = mNear
    float n = f[0], fa = f[1], a = f[2], s = f[3], c = f[4], t = f[5], asp = f[6];
    if (!(s > 0.05f && s < 0.95f && c > 0.3f && c <= 1 && t > 0.05f && t < 3)) return false;
    if (!(fabsf(s * s + c * c - 1) < 1e-3f && fabsf(s / c - t) < 1e-3f * t && fabsf(2 * asinf(s) - a) < 1e-3f)) return false;
    return n > 0 && n < fa && fa < 1e7f && asp > 0.2f && asp < 5;
}

// near <= 2, far >= 100, fovy 20..110 deg, a wide-screen aspect (draw1: a 4:3 one came first). (Deck s10) BotW's
// gameplay projection takes the window's aspect: 1.6 at the Deck's 1280x800 (PC 16:9 = 1.7778). The old 1.7..1.85 rule
// found no gameplay projection there = Sora drawn with the fallback projection AND "proj lost: rescanning" forever (a
// 3 GB heap walk every ~7 s, -5% fps). 1.45..2.5 = 16:10 to 21:9; the others in the scan are 4:3 (1.3333) and 1:1.
static bool proj_gameplay(const float *f) {
    return f[0] <= 2 && f[1] >= 100 && f[2] > 0.35f && f[2] < 1.92f && f[6] > 1.45f && f[6] < 2.5f;
}

// (s111 camfast) One scan's findings. The scan itself (cam_page over for_each_page) reads only guest memory and this
// struct, so it can run on a worker thread; the choice among the candidates (cam_apply) stays on the game thread.
typedef struct {
    uint64_t cams[CAM_MAX], projs[PROJ_MAX];
    int cam_score[CAM_MAX], ncams, nprojs;
    float link[3];  // Link's position when the scan started (cam_score's "at near Link")
    bool live;      // (s111 review) async: take Link's position per page from g_scan_link (the game runs meanwhile)
    LONG gen;       // g_scan_gen at the request (a result from an abandoned / pre-reboot worker is dropped)
    double ms;
} CamScan;

// (s111 review) Link's position, published by cam_frame on the game thread every frame. The async scan reads it per
// page: the old blocking scan saw camera and Link from one frozen frame; a 4 s worker with the start position only
// would fail the "at within 6 u of Link" rule on every page walked after Link moved (running / horse / glide).
// (Three plain float loads: a mix of two consecutive frames is off by well under a unit.)
static volatile float g_scan_link[3];

static void cam_page(uint64_t pg, void *ctx) {
    CamScan *sc = ctx;
    const float *f = (const float *)(uintptr_t)pg;
    float link[3] = {sc->link[0], sc->link[1], sc->link[2]};
    if (sc->live) link[0] = g_scan_link[0], link[1] = g_scan_link[1], link[2] = g_scan_link[2];
    for (size_t i = 0; i + 9 <= 0x1000 / 4; i++) {
        int s = cam_score_l(f + i, i >= 12, link);
        if (s && sc->ncams < CAM_MAX) sc->cam_score[sc->ncams] = s, sc->cams[sc->ncams++] = pg + i * 4;
        if (i + 7 <= 0x1000 / 4 && sc->nprojs < PROJ_MAX && proj_ok(f + i)) sc->projs[sc->nprojs++] = pg + i * 4;
    }
}

static void cam_run_scan(CamScan *sc) {  // the whole page walk (any thread)
    LARGE_INTEGER qf, t0, t1;
    QueryPerformanceFrequency(&qf);
    QueryPerformanceCounter(&t0);
    sc->ncams = sc->nprojs = 0;
    for_each_page(cam_page, sc);
    QueryPerformanceCounter(&t1);
    sc->ms = (double)(t1.QuadPart - t0.QuadPart) * 1e3 / (double)qf.QuadPart;
}

// Game thread: pick the camera / projection from a finished scan (the old cam_scan's rules, unchanged) and log it.
// Returns the camera chosen.
static uint64_t cam_apply(const CamScan *sc, const char *how) {
    uint64_t prev = g_cam;
    g_ncams = sc->ncams, g_cam = 0, g_nprojs = sc->nprojs, g_proj = 0;
    memcpy(g_cams, sc->cams, sizeof g_cams), memcpy(g_cam_score, sc->cam_score, sizeof g_cam_score);
    memcpy(g_projs, sc->projs, sizeof g_projs);
    // (s111 review) an async result is up to seconds old: first the score-2 candidates that are STILL score 2 now (the old
    // blocking scan's choice was by construction valid at apply time), then the scan's first score-2 as before
    for (int k = 0; k < g_ncams && !g_cam; k++)
        if (g_cam_score[k] == 2 && cam_score((const float *)(uintptr_t)g_cams[k], true) == 2) g_cam = g_cams[k];
    for (int k = 0; k < g_ncams && !g_cam; k++) if (g_cam_score[k] == 2) g_cam = g_cams[k];
    if (!g_cam && prev) g_cam = prev, LOG("[linkpos] camera scan: no new camera, keeping %llx\n", (unsigned long long)prev);
    for (int k = 0; k < g_nprojs && !g_proj; k++) if (proj_gameplay((const float *)(uintptr_t)g_projs[k])) g_proj = g_projs[k];
    for (int k = 0; k < g_nprojs; k++) {
        const float *f = (const float *)(uintptr_t)g_projs[k];
        LOG("[linkpos]   proj %llx%s near %.3f far %.1f fovy %.2f deg aspect %.4f offset %.3f %.3f\n",
            (unsigned long long)g_projs[k], g_projs[k] == g_proj ? " (used)" : "", f[0], f[1], f[2] * 57.29578f, f[6],
            f[7], f[8]);
    }
    LOG("[linkpos] camera scan: %d candidates in %.0f ms (%s), using %llx\n", g_ncams, sc->ms, how, (unsigned long long)g_cam);
    for (int k = 0; k < g_ncams; k++) {
        const float *f = (const float *)(uintptr_t)g_cams[k];
        LOG("[linkpos]   cam %llx score %d pos %.2f %.2f %.2f at %.2f %.2f %.2f up %.2f %.2f %.2f\n",
            (unsigned long long)g_cams[k], g_cam_score[k], f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8]);
    }
    return g_cam;
}

// The old behaviour: the whole scan on the game thread (blocks it ~4 s: s111 last41.log, 6 x 4.1-4.8 s "slow frame").
static void cam_scan(void) {
    static CamScan sc;
    memcpy(sc.link, g_cam_link, sizeof sc.link);
    cam_run_scan(&sc);
    cam_apply(&sc, "sync, game thread blocked");
}

// (s111 camfast) The scan on a worker thread: the game keeps running (previous camera kept, or none, meanwhile) and the
// finished findings are picked up by cam_frame on the game thread. A worker that is not done CAM_ASYNC_TIMEOUT presents
// after its start is abandoned (its result dropped) and the old blocking scan runs instead.
//   SAVAGEWIND_LINK_CAM_SYNC=1  always the old blocking scan on the game thread
//   SAVAGEWIND_LINK_CAM_CMP=1   test mode: after each async scan also run the old one and log both choices
#define CAM_ASYNC_TIMEOUT 1800
static volatile LONG g_scan_gen;       // bumped to abandon the running worker
static CamScan *volatile g_scan_done;  // worker -> game thread: the finished findings (heap, freed by the game thread)
static bool g_scan_pending;            // game thread only
static uint64_t g_scan_req_present;

typedef struct { CamScan *sc; LONG gen; } CamJob;
static DWORD WINAPI cam_worker(LPVOID arg) {
    CamJob *job = arg;
    cam_run_scan(job->sc);
    // (s111 review) Crash safety of reading live guest memory here while the game runs: the guest window is one fixed
    // reservation (platform/windows.c plat_reserve_guest), never released; pages the game decommits meanwhile
    // (svc UnmapMemory -> plat_decommit, plat_reset_guest on a reboot) are MEM_RESERVE again, and a read of one faults
    // into the process-wide vectored handler, whose lazy_commit commits a zeroed 64 KB chunk and resumes this thread
    // (the same handler every guest thread relies on). Nothing in a normal run makes guest pages NOACCESS (only the
    // SAVAGEWIND watchpoint debug mode, handled per thread by rt_fault_watch). Values may be torn: cam_apply re-checks.
    // (The old scan had the same exposure: guest threads are host threads, they kept running while it walked.)
    if (job->gen == g_scan_gen) free(InterlockedExchangePointer((PVOID volatile *)&g_scan_done, job->sc));
    else free(job->sc);
    free(job);
    return 0;
}

static void cam_scan_request(uint64_t present) {
    static int sync_env = -1;
    if (sync_env < 0) sync_env = env_int("SAVAGEWIND_LINK_CAM_SYNC", 0) != 0;
    if (sync_env) { cam_scan(); return; }
    if (g_scan_pending) return;
    CamScan *sc = calloc(1, sizeof *sc);
    CamJob *job = malloc(sizeof *job);
    HANDLE h = NULL;
    if (sc && job) {
        memcpy(sc->link, g_cam_link, sizeof sc->link);
        sc->live = true;
        job->sc = sc, job->gen = sc->gen = g_scan_gen;
        h = CreateThread(NULL, 0, cam_worker, job, 0, NULL);
    }
    if (!h) {
        free(sc), free(job);
        LOG("[linkpos] camera scan: no worker thread: scanning on the game thread\n");
        cam_scan();
        return;
    }
    SetThreadPriority(h, THREAD_PRIORITY_BELOW_NORMAL);
    CloseHandle(h);
    g_scan_pending = true, g_scan_req_present = present;
    LOG("[linkpos] camera scan: started on a worker thread (presented %llu), game continues\n", (unsigned long long)present);
}

// Game thread, every cam_frame: take a finished async scan (apply + log), or give up on a stuck one. True: just applied.
static bool cam_scan_poll(uint64_t present) {
    CamScan *sc = (CamScan *)InterlockedExchangePointer((PVOID volatile *)&g_scan_done, NULL);
    if (sc && sc->gen != g_scan_gen) free(sc), sc = NULL;  // (abandoned worker that passed its gen check just before the bump)
    if (sc) {
        g_scan_pending = false;
        char how[64];
        snprintf(how, sizeof how, "async, %llu presents later", (unsigned long long)(present - g_scan_req_present));
        uint64_t chose = cam_apply(sc, how);
        uint64_t chose_proj = g_proj;
        free(sc);
        if (env_int("SAVAGEWIND_LINK_CAM_CMP", 0)) {  // the old scan too (blocks once): do both pick the same camera?
            uint64_t old = g_cam;
            cam_scan();  // (its "keeping" rule sees the async choice as the previous camera)
            LOG("[linkpos] camera scan CMP: async chose %llx (proj %llx), old scan chose %llx (proj %llx): %s\n",
                (unsigned long long)chose, (unsigned long long)chose_proj, (unsigned long long)g_cam,
                (unsigned long long)g_proj, chose == g_cam && chose_proj == g_proj ? "SAME" : "DIFFERENT");
            (void)old;
        }
        return true;
    }
    if (g_scan_pending && present > g_scan_req_present + CAM_ASYNC_TIMEOUT) {
        LOG("[linkpos] camera scan: worker not done after %d presents: abandoned, old blocking scan\n", CAM_ASYNC_TIMEOUT);
        InterlockedIncrement(&g_scan_gen);
        g_scan_pending = false;
        cam_scan();
    }
    return false;
}

static void cam_frame(void) {
    static uint64_t last_scan;
    static int lost;
    if (!(g_cam_wanted || g_t0) || !linkpos_link_pos(g_cam_link)) return;
    g_scan_link[0] = g_cam_link[0], g_scan_link[1] = g_cam_link[1], g_scan_link[2] = g_cam_link[2];  // (the async scan's)
    if (!g_cam_wanted) {
        static int env = -1;
        if (env < 0) env = getenv("SAVAGEWIND_LINK_CAM") != NULL;
        if (!env) return;
    }
    if (*(uint32_t *)(uintptr_t)PAUSE_STATE) return;
    extern uint64_t vk_frame_counter(void);
    uint64_t present = vk_frame_counter();
    if (cam_scan_poll(present)) last_scan = present, lost = 0;
    if (g_cam) {  // (the scan rule failing alone is logged once per stretch: what BotW's camera did there)
        static bool off_rule;
        const float *c = (const float *)(uintptr_t)g_cam;
        bool rule = cam_score(c, false) != 0;
        if (!rule && !off_rule && cam_track(c)) {
            float d[3] = {c[0] - c[3], c[1] - c[4], c[2] - c[5]}, l[3] = {c[3] - g_cam_link[0], c[4] - g_cam_link[1], c[5] - g_cam_link[2]};
            LOG("[linkpos] camera %llx off the scan rule (pos-at %.2f u, at %.2f u from Link): still tracked\n",
                (unsigned long long)g_cam, len3(d), len3(l));
        }
        off_rule = !rule;
    }
    // (s85h #13, the tower cutscene) BotW freed/rewrote the scanned projection: linkpos_camera_proj failed proj_ok
    // and sora_draw fell back to far 10000 = depth mismatch. Re-pick from the last scan's candidates, else rescan.
    if (last_scan && !(g_proj && proj_ok((const float *)(uintptr_t)g_proj))) {
        static bool proj_lost;
        uint64_t was = g_proj;
        g_proj = 0;
        for (int k = 0; k < g_nprojs && !g_proj; k++) {
            const float *f = (const float *)(uintptr_t)g_projs[k];
            if (proj_ok(f) && proj_gameplay(f)) g_proj = g_projs[k];
        }
        if (g_proj) {
            LOG("[linkpos] proj %llx lost: using the scan's %llx\n", (unsigned long long)was, (unsigned long long)g_proj);
            proj_lost = false;
        } else if (present - last_scan >= CAM_RESCAN) {
            LOG("[linkpos] proj %llx lost (presented %llu): rescanning\n", (unsigned long long)was, (unsigned long long)present);
            last_scan = present, lost = 0, proj_lost = true;
            cam_scan_request(present);
            return;
        } else if (!proj_lost) {
            proj_lost = true;
            LOG("[linkpos] proj %llx lost: waiting for the rescan\n", (unsigned long long)was);
        }
    }
    if (g_cam && cam_track((const float *)(uintptr_t)g_cam)) { lost = 0; return; }
    // (s58, user: "when sora died i didnt see any model for a second or two") BotW's death camera dropped the scanned
    // camera; the scan's other score-2 copies of the view (live_s58: 72dddfe4c8, 72e73c25c0, there since the start)
    // kept tracking. Switch to one at once: no CAM_LOST wait + 4 s rescan with Sora undrawn.
    for (int k = 0; g_cam && k < g_ncams; k++)
        if (g_cam_score[k] == 2 && g_cams[k] != g_cam && cam_track((const float *)(uintptr_t)g_cams[k])) {
            LOG("[linkpos] camera %llx lost (presented %llu): using the scan's %llx\n", (unsigned long long)g_cam,
                (unsigned long long)present, (unsigned long long)g_cams[k]);
            g_cam = g_cams[k], lost = 0;
            return;
        }
    // (s58 live, the Bokoblin cave: all 3 copies failed cam_track within 15 presents, the scan found 0 candidates in
    // 4.3 s) still a camera by cam_alive = keep drawing with it; the stretch is logged once with the failing rule
    static bool loose;
    if (g_cam && cam_alive((const float *)(uintptr_t)g_cam)) {
        if (!loose) {
            const float *c = (const float *)(uintptr_t)g_cam;
            LOG("[linkpos] camera %llx off cam_track (%s; pos %.2f %.2f %.2f at %.2f %.2f %.2f up %.2f %.2f %.2f, Link %.2f "
                "%.2f %.2f): kept (cam_alive)\n", (unsigned long long)g_cam, cam_why(c), c[0], c[1], c[2], c[3], c[4], c[5],
                c[6], c[7], c[8], g_cam_link[0], g_cam_link[1], g_cam_link[2]);
        }
        loose = true, lost = 0;
        return;
    }
    if (loose && g_cam) LOG("[linkpos] camera %llx not alive either (%s)\n", (unsigned long long)g_cam, cam_why((const float *)(uintptr_t)g_cam));
    loose = false;
    if (g_cam && ++lost < CAM_LOST) return;
    if (last_scan && present - last_scan < CAM_RESCAN) return;
    if (g_cam) LOG("[linkpos] camera %llx lost (presented %llu)\n", (unsigned long long)g_cam, (unsigned long long)present);
    last_scan = present, lost = 0;
    cam_scan_request(present);
}

bool linkpos_camera_fwd(float f[2]) {
    g_cam_wanted = true;
    if (!g_cam || !cam_alive((const float *)(uintptr_t)g_cam)) return false;  // (s58: cam_alive, the cave camera)
    const float *c = (const float *)(uintptr_t)g_cam;
    float x = c[3] - c[0], z = c[5] - c[2], l = sqrtf(x * x + z * z);
    if (l < 1e-4f) return false;
    f[0] = x / l, f[1] = z / l;
    return true;
}

// Sora's draw pass (game thread, like the rest): the camera's view matrix (sead Matrix34f rows right / up / back, the
// translation in column 3: eye = M * (p, 1)) and the projection's near, far, fovy (rad), aspect.
bool linkpos_camera_view(float m[12]) {
    g_cam_wanted = true;
    if (!g_cam || !cam_alive((const float *)(uintptr_t)g_cam)) return false;  // (s58: cam_alive, the cave camera)
    memcpy(m, (const void *)(uintptr_t)(g_cam - 12 * 4), 48);
    return true;
}

// (s137 fps, recon/S137_HELPER_FPS.md + S137_REVIEW_FPS.md M4) A camera WRITE for Chicken Little's FPS Mode (the vanilla first-person camera,
// KH2 field camera mode 9). DLL: linkpos_camera_set(on, pos, at, up) each frame while the mode runs (GetProcAddress, so an older exe = BotW's camera).
// pos / at / up go into the scanned sead::LookAtCamera (pos +0, at +12, up +24 floats from g_cam) and into the view matrix before it (Matrix34f rows
// right / up / back, translation in column 3: M = [R | -R*pos]), also into every other scan copy of the view (score 2) that still tracks (GUESS: the
// render may read a copy). Written at three points:
//   1. at once in linkpos_camera_set (inside sora_frame, so this frame's skin_publish snapshots see the same view as the world),
//   2. right after sora_frame in linkpos_frame (cam_override_apply),
//   3. LATE: BotW's camera update runs after rt_vfr_update (= after 1 and 2) and rewrites the struct. opacity_apply (the actors' per-frame
//      applyOpacityIfDirty, ~30k calls a frame, model calc) checks whether g_cam still holds our pos / at and writes again when it does not
//      (cam_override_late). GUESS: that the model calc runs after BotW's camera calc. SAVAGEWIND_LINK_CAMLATE=0 = off.
// The probe (cam_override_probe, at the start of the next linkpos_frame = after BotW's whole frame): is g_cam's pos / at still ours?
// kept / lost counted and logged; linkpos_camera_override_state(): 0 off, 1 written (not confirmed yet), 2 kept (3 frames in a row), 3 lost
// (30 frames in a row, none kept): the DLL then falls back to BotW's camera. Never while paused; lapses 20 presents after the DLL's last call.
static struct { volatile bool on; float pos[3], at[3], up[3]; uint64_t at_frame; } g_camset;
static struct { volatile bool wrote; int keep_run, lost_run, state; uint32_t kept, lost, late; } g_camp;
static int g_camlate = -1;
static void cam_write_one(uint64_t cam) {
    float *c = (float *)(uintptr_t)cam;
    float back[3] = {g_camset.pos[0] - g_camset.at[0], g_camset.pos[1] - g_camset.at[1], g_camset.pos[2] - g_camset.at[2]};
    float bl = len3(back);
    if (bl < 1e-5f) return;
    for (int i = 0; i < 3; i++) back[i] /= bl;
    float r[3] = {g_camset.up[1] * back[2] - g_camset.up[2] * back[1], g_camset.up[2] * back[0] - g_camset.up[0] * back[2], g_camset.up[0] * back[1] - g_camset.up[1] * back[0]};
    float rl = len3(r);
    if (rl < 1e-5f) return;
    for (int i = 0; i < 3; i++) r[i] /= rl;
    float u[3] = {back[1] * r[2] - back[2] * r[1], back[2] * r[0] - back[0] * r[2], back[0] * r[1] - back[1] * r[0]};
    float *m = c - 12;  // the view matrix just before pos (see cam_score_l)
    const float *rows[3] = {r, u, back};
    for (int k = 0; k < 3; k++) {
        m[4 * k + 0] = rows[k][0], m[4 * k + 1] = rows[k][1], m[4 * k + 2] = rows[k][2];
        m[4 * k + 3] = -(rows[k][0] * g_camset.pos[0] + rows[k][1] * g_camset.pos[1] + rows[k][2] * g_camset.pos[2]);
    }
    memcpy(c, g_camset.pos, 12), memcpy(c + 3, g_camset.at, 12), memcpy(c + 6, g_camset.up, 12);
}
static bool cam_override_write(void) {  // false: nothing written (off, paused, no camera)
    if (!g_camset.on || *(volatile uint32_t *)(uintptr_t)PAUSE_STATE) return false;
    if (!g_cam || !cam_alive((const float *)(uintptr_t)g_cam)) return false;
    cam_write_one(g_cam);
    for (int k = 0; k < g_ncams; k++)
        if (g_cam_score[k] == 2 && g_cams[k] != g_cam && cam_track((const float *)(uintptr_t)g_cams[k])) cam_write_one(g_cams[k]);
    g_camp.wrote = true;
    return true;
}
static void cam_probe_reset(void) { g_camp.wrote = false, g_camp.keep_run = g_camp.lost_run = 0, g_camp.state = 0; }
void linkpos_camera_set(bool on, const float pos[3], const float at[3], const float up[3]) {
    extern uint64_t vk_frame_counter(void);
    if (!on || !pos || !at || !up) {
        if (g_camset.on) LOG("[linkpos] camera override off (kept %u / lost %u frames, %u late rewrites)\n", g_camp.kept, g_camp.lost, g_camp.late);
        g_camset.on = false;
        cam_probe_reset();
        return;
    }
    if (!g_camset.on) LOG("[linkpos] camera override on (FPS Mode first person%s)\n", g_camp.state == 2 ? ", kept before" : "");  // (the probe state survives a lapse / pause: no Sora flash after each pause)
    memcpy(g_camset.pos, pos, 12), memcpy(g_camset.at, at, 12), memcpy(g_camset.up, up, 12);
    g_camset.at_frame = vk_frame_counter(), g_camset.on = true;
    g_cam_wanted = true;
    cam_override_write();  // (1: at once, before this frame's snapshots)
}
// (s137 fps) 0 off, 1 written / not confirmed, 2 kept by BotW's frame, 3 lost (BotW rewrites it after us): the DLL's fallback test
int linkpos_camera_override_state(void) { return g_camset.on ? (g_camp.state ? g_camp.state : 1) : 0; }  // (the state resets only on an explicit off)
static void cam_override_apply(void) {  // (2: after sora_frame)
    extern uint64_t vk_frame_counter(void);
    if (!g_camset.on) return;
    if (vk_frame_counter() - g_camset.at_frame > 20) { g_camset.on = false, g_camp.wrote = false; LOG("[linkpos] camera override lapsed (the DLL stopped calling)\n"); return; }
    cam_override_write();
}
// (3) from opacity_apply (any guest thread): BotW rewrote g_cam since our write -> write again. Cheap when off (one bool) or still ours (6 floats).
static void cam_override_late(void) {
    if (!g_camset.on || !g_camp.wrote || !g_cam) return;
    if (g_camlate < 0) g_camlate = env_int("SAVAGEWIND_LINK_CAMLATE", 1);
    if (!g_camlate) return;
    const float *c = (const float *)(uintptr_t)g_cam;
    if (c[0] == g_camset.pos[0] && c[1] == g_camset.pos[1] && c[2] == g_camset.pos[2] && c[3] == g_camset.at[0] && c[4] == g_camset.at[1] && c[5] == g_camset.at[2]) return;
    if (cam_override_write()) g_camp.late++;
}
// (s137b, recon/S137_REVIEW_FPS.md "s137b camera") (4) the MATRIX itself. s137 live: points 1-3 were LOST every frame ("last miss 8.465 u",
// 0 late rewrites): BotW's camera controller sets pos / at after them and rebuilds the view. The rebuild is sead::LookAtCamera::doUpdateMatrix
// (this, Matrix34f *dst) = runtime 0x7000eb2d5c on 1.9.0 (Ghidra 0x7100eb2d5c; decomp recon/ghidra/out/s137cam2.c: reads pos +0x38 / at +0x44 /
// up +0x50, writes dst = rows right / up / back + translation). It is vtable slot 4 (+0x20) of both LookAtCamera vtables the scanned cameras carry
// (0x7001d05158, 0x7001d1eee0; read live from the scan's 3 cameras), so sead::Camera::updateViewMatrix reaches it by an INDIRECT call -> the
// hle_override below sees it (its 3 direct BL callers 0x700103c4f0 / 0x700124a948 / 0x700148950c build stack temporaries, not the game camera).
// While the override is on, a call on a scanned gameplay camera (g_cam or a scan copy, still near Link) gets our pos / at / up first: BotW's own
// update, after its controller ran, then builds OUR view matrix. SAVAGEWIND_LINK_NO_CAMHOOK=1 = off.
static GuestFn g_lookat_mtx;
static volatile uint32_t g_lookat_calls, g_lookat_subst;
static bool cam_is_scanned(uint64_t pos_addr) {
    uint64_t g = g_cam;
    if (!g) return false;
    if (pos_addr == g) return true;
    for (int k = 0; k < g_ncams && k < CAM_MAX; k++)
        if (g_cams[k] == pos_addr) return true;
    return false;
}
static void lookat_mtx_hook(Cpu *c) {
    uint64_t self = c->x[0];
    g_lookat_calls++;
    if (g_camset.on && in_guest(self) && cam_is_scanned(self + CAM_POS) && !*(volatile uint32_t *)(uintptr_t)PAUSE_STATE) {
        float *f = (float *)(uintptr_t)(self + CAM_POS);
        if (cam_alive(f)) {  // (finite, unit up, `at` within 100 u of Link: never a stale address reused by another camera)
            memcpy(f, g_camset.pos, 12), memcpy(f + 3, g_camset.at, 12), memcpy(f + 6, g_camset.up, 12);
            g_lookat_subst++, g_camp.wrote = true;
        }
    }
    g_lookat_mtx(c);
}
static void cam_hook_init(void) {
    if (getenv("SAVAGEWIND_LINK_NO_CAMHOOK")) { LOG("[linkpos] camera hook off (SAVAGEWIND_LINK_NO_CAMHOOK)\n"); return; }
    if (!SS->lookat_update_mtx || !(g_lookat_mtx = dispatch_lookup(SS->lookat_update_mtx))) {
        LOG("[linkpos] no LookAtCamera::doUpdateMatrix at %llx: FPS camera = the plain writes only\n", (unsigned long long)SS->lookat_update_mtx);
        return;
    }
    hle_override(SS->lookat_update_mtx, lookat_mtx_hook, "linkpos LookAtCamera::doUpdateMatrix");  // (indirect calls: updateViewMatrix's vcall)
    LOG("[linkpos] camera hook: sead::LookAtCamera::doUpdateMatrix %llx (the FPS Mode camera)\n", (unsigned long long)SS->lookat_update_mtx);
}

// start of linkpos_frame (game thread): did the last write survive BotW's frame? (s137b) The test is the VIEW MATRIX before g_cam (what the
// render uses), not pos / at: BotW may set pos / at again after its matrix update; the matrix must be the one our pos / at / up give.
static float cam_mtx_miss(const float *c) {
    float back[3] = {g_camset.pos[0] - g_camset.at[0], g_camset.pos[1] - g_camset.at[1], g_camset.pos[2] - g_camset.at[2]}, bl = len3(back);
    if (bl < 1e-5f) return 1e9f;
    for (int i = 0; i < 3; i++) back[i] /= bl;
    float r[3] = {g_camset.up[1] * back[2] - g_camset.up[2] * back[1], g_camset.up[2] * back[0] - g_camset.up[0] * back[2], g_camset.up[0] * back[1] - g_camset.up[1] * back[0]}, rl = len3(r);
    if (rl < 1e-5f) return 1e9f;
    for (int i = 0; i < 3; i++) r[i] /= rl;
    float u[3] = {back[1] * r[2] - back[2] * r[1], back[2] * r[0] - back[0] * r[2], back[0] * r[1] - back[1] * r[0]};
    const float *m = c - 12, *rows[3] = {r, u, back};
    float worst = 0;
    for (int k = 0; k < 3; k++) {
        float t = -(rows[k][0] * g_camset.pos[0] + rows[k][1] * g_camset.pos[1] + rows[k][2] * g_camset.pos[2]);
        for (int j = 0; j < 3; j++) worst = fmaxf(worst, fabsf(m[4 * k + j] - rows[k][j]));
        worst = fmaxf(worst, fabsf(m[4 * k + 3] - t) * 0.1f);  // (translation in world units: the 2e-3 test = 2 cm)
    }
    return worst;
}
static void cam_override_probe(void) {
    if (!g_camset.on || !g_camp.wrote || *(volatile uint32_t *)(uintptr_t)PAUSE_STATE) return;
    g_camp.wrote = false;
    if (!g_cam || !cam_alive((const float *)(uintptr_t)g_cam)) return;
    const float *c = (const float *)(uintptr_t)g_cam;
    float d[3] = {c[0] - g_camset.pos[0], c[1] - g_camset.pos[1], c[2] - g_camset.pos[2]}, mm = cam_mtx_miss(c);
    bool kept = mm < 2e-3f;
    if (kept) g_camp.kept++, g_camp.keep_run++, g_camp.lost_run = 0;
    else g_camp.lost++, g_camp.lost_run++, g_camp.keep_run = 0;
    int st = g_camp.keep_run >= 3 ? 2 : g_camp.lost_run >= 30 ? 3 : g_camp.state;
    if (st != g_camp.state)
        LOG("[linkpos] camera override %s (kept %u / lost %u frames, %u late rewrites, matrix hook %u calls / %u ours; last matrix miss %.4f, pos miss %.3f u)\n",
            st == 2 ? "KEPT: BotW's frame leaves our camera" : "LOST: BotW rewrites the camera after our writes", g_camp.kept, g_camp.lost, g_camp.late,
            g_lookat_calls, g_lookat_subst, mm, len3(d));
    g_camp.state = st;
    if ((g_camp.kept + g_camp.lost) % 300 == 0)
        LOG("[linkpos] camera override: kept %u / lost %u frames, %u late rewrites, matrix hook %u calls / %u ours\n", g_camp.kept, g_camp.lost, g_camp.late,
            g_lookat_calls, g_lookat_subst);
}

bool linkpos_camera_proj(float p[4]) {
    if (!g_proj || !proj_ok((const float *)(uintptr_t)g_proj)) return false;
    const float *f = (const float *)(uintptr_t)g_proj;
    p[0] = f[0], p[1] = f[1], p[2] = f[2], p[3] = f[6];
    return true;
}

// World ray cast (recon/BOTW_RAYCAST_RECON.md): segment from -> to (BotW units) against the static ground layers
// (8,10,9,2,12: ground, ground objects, trees; Link, enemies and NPCs ignored) through the game's own helper
// 0x7000ac4390 bool (s0 yoff, x0 from*, x1 to*, x2 hitPos*, x3 hitNormal*, x4 mat* | 0), which builds a
// ksys::phys::RayCast on its stack and locks the world itself. ONLY inside linkpos_frame (game main thread, between
// two frames' updates): never from vel_step (physics workers, mid-step) -> false anywhere else.
#define RC_GROUND_SEG (SS->ray_ground_seg)
static bool g_in_frame;
static uint64_t g_nrc, g_nrc_refused;

bool linkpos_raycast(const float from[3], const float to[3], float hit[3], float normal[3]) {
    Cpu *c = g_in_frame ? kernel_current_cpu() : NULL;
    if (!c || !dispatch_lookup(RC_GROUND_SEG)) {
        if (!g_nrc_refused++) LOG("[linkpos] ray cast refused (%s)\n", g_in_frame ? "no guest cpu / helper" : "outside linkpos_frame");
        return false;
    }
    Cpu save = *c;                                 // x, sp, flags, v: all restored below
    uint64_t sp = (c->sp - 0x80) & ~0xfull;        // scratch below the live guest sp (AArch64 has no red zone)
    float *g = (float *)(uintptr_t)sp;             // g[0..2] from, g[4..6] to, g[8..10] pos, g[12..14] normal
    memset(g, 0, 0x40);
    memcpy(g, from, 12), memcpy(g + 4, to, 12);
    c->sp = sp;
    c->x[0] = sp, c->x[1] = sp + 0x10, c->x[2] = sp + 0x20, c->x[3] = sp + 0x30, c->x[4] = 0;
    c->v[0] = 0;                                   // s0 = 0.0f (y offset)
    guest_call(c, RC_GROUND_SEG);
    bool ok = (c->x[0] & 1) != 0;
    if (ok) {
        if (hit) memcpy(hit, g + 8, 12);
        if (normal) memcpy(normal, g + 12, 12);
    }
    *c = save;
    if (!g_nrc++) LOG("[linkpos] first ray cast (%.2f %.2f %.2f) -> (%.2f %.2f %.2f): %s at (%.2f %.2f %.2f) n (%.2f %.2f %.2f)\n",
                      from[0], from[1], from[2], to[0], to[1], to[2], ok ? "hit" : "no hit", g[8], g[9], g[10], g[12], g[13], g[14]);
    return ok;
}

// Game data flags (s55 levels, slice 2; recon/S54_HELPER_LV_LEVELS.md 4.2 A, S55_HELPER_LEVELS23.md): a guest call of
// the gdt getters the way the Defeated counter FUN_7100ac6840 calls getS32 (disassembly 0x7100ac68d8..f4):
// x0 = TriggerParam [[gdt::Manager + 0xbe0]], x1 = &out, x2 = &sead::SafeString {vtable [GOT 0x7101c05290] + 0x10,
// name}, w3 = the check-permission byte [mgr + 0xbe8], w4 = 1. getS32 FUN_71010238f4 (s32 array +0x18), getBool
// FUN_710102385c (bool array +0x8, strb out). Manager = [0x7101cd9908] (adrp+add at 0x7100ac6858). Same rule as the ray
// cast: only inside linkpos_frame (the game main thread, between frames) -> 0 anywhere else, or when the name is
// unknown (the getter returns false). Names <= 95 chars, copied onto the guest stack with the SafeString.
#define GDT_MGR_PTR (SS->gdt_mgr)
#define GDT_TRIGGER 0xbe0
#define GDT_PERM 0xbe8
#define GDT_GET_BOOL (SS->gdt_get_bool)
#define GDT_GET_S32 (SS->gdt_get_s32)
#define SAFESTRING_VT_GOT (SS->safestring_vt_got)
static uint64_t g_ngdt, g_ngdt_refused;

static bool gdt_get(const char *name, uint64_t fn, int32_t *out) {
    Cpu *c = g_in_frame ? kernel_current_cpu() : NULL;
    size_t len = name ? strlen(name) : 0;
    uint64_t mgr = *(const uint64_t *)(uintptr_t)GDT_MGR_PTR, tp = 0, param = 0, vt = *(const uint64_t *)(uintptr_t)SAFESTRING_VT_GOT;
    if (in_guest(mgr)) tp = *(const uint64_t *)(uintptr_t)(mgr + GDT_TRIGGER);
    if (in_guest(tp)) param = *(const uint64_t *)(uintptr_t)tp;
    if (!c || !len || len > 95 || !in_guest(param) || !in_guest(vt) || !dispatch_lookup(fn)) {
        if (!g_ngdt_refused++)
            LOG("[linkpos] gdt get refused (%s)\n", !c ? (g_in_frame ? "no guest cpu" : "outside linkpos_frame")
                                                       : !in_guest(param) ? "no gdt manager yet"
                                                       : !in_guest(vt) ? "SafeString vtable GOT not a guest pointer"
                                                       : !dispatch_lookup(fn) ? "no getter function" : "bad name");
        return false;
    }
    Cpu save = *c;                             // restored below (x, sp, flags, v)
    uint64_t sp = (c->sp - 0xa0) & ~0xfull;    // scratch below the live guest sp: +0 SafeString, +0x10 out, +0x20 name
    uint8_t *g = (uint8_t *)(uintptr_t)sp;
    memset(g, 0, 0xa0);
    uint64_t ss[2] = {vt + 0x10, sp + 0x20};
    memcpy(g, ss, 16);
    memcpy(g + 0x20, name, len);
    c->sp = sp;
    c->x[0] = param, c->x[1] = sp + 0x10, c->x[2] = sp;
    c->x[3] = *(const uint8_t *)(uintptr_t)(mgr + GDT_PERM), c->x[4] = 1;
    guest_call(c, fn);
    bool ok = (c->x[0] & 1) != 0;
    int32_t v = fn == GDT_GET_BOOL ? (g[0x10] & 1) : *(const int32_t *)(g + 0x10);
    *c = save;
    if (ok && out) *out = v;
    if (!g_ngdt++) LOG("[linkpos] first gdt get %s %s: %s %d\n", fn == GDT_GET_BOOL ? "bool" : "s32", name, ok ? "found" : "not found", v);
    return ok;
}
// 1 = found (*out = the value), 0 = unknown name / not now. (sora_mod.dll, by GetProcAddress)
int linkpos_gdt_s32(const char *name, int32_t *out) { return gdt_get(name, GDT_GET_S32, out); }
int linkpos_gdt_bool(const char *name, int *out) {
    int32_t v = 0;
    if (!gdt_get(name, GDT_GET_BOOL, &v)) return 0;
    if (out) *out = v;
    return 1;
}

// s98 b13-pouch: take one named item out of BotW's REAL inventory (recon/S98_HELPER_POUCH.md). The same call Link's own
// code makes (e.g. the caller f_7000cead24 on 1.9.0 removes "Animal_Insect_F"): ui::PauseMenuDataMgr::removeItem(
// this = [[pmdm_got]], const sead::SafeString& name) = f_7000d93320 on 1.9.0 (f_7000c80050 on 1.8.2). It takes the
// first non-equipped PouchItem with that name (a stack loses one, anything else is deleted), then updateInventoryInfo /
// updateListHeads / saveToGameData, which is what syncs the gdt PorchItem arrays. Same rule as the gdt getters above:
// a guest call, only inside linkpos_frame. 1 = called, -1 = refused (not now / no site), -2 = no PauseMenuDataMgr yet.
#define POUCH_REMOVE_ITEM (SS->pouch_remove_item)
#define PMDM_GOT (SS->pmdm_got)
static uint64_t g_npouch_rm, g_npouch_refused;
int linkpos_pouch_remove(const char *name) {
    Cpu *c = g_in_frame ? kernel_current_cpu() : NULL;
    size_t len = name ? strlen(name) : 0;
    uint64_t fn = POUCH_REMOVE_ITEM, got = PMDM_GOT, vt = in_guest(SAFESTRING_VT_GOT) ? *(const uint64_t *)(uintptr_t)SAFESTRING_VT_GOT : 0;
    uint64_t pp = fn && in_guest(got) ? *(const uint64_t *)(uintptr_t)got : 0;  // &PauseMenuDataMgr::sInstance
    uint64_t mgr = in_guest(pp) ? *(const uint64_t *)(uintptr_t)pp : 0;
    if (!c || !len || len > 95 || !in_guest(vt) || !fn || !in_guest(pp) || !dispatch_lookup(fn)) {
        if (!g_npouch_refused++)
            LOG("[linkpos] pouch remove refused (%s)\n", !fn ? "no site for this game version" : !c ? (g_in_frame ? "no guest cpu" : "outside linkpos_frame")
                                                       : !in_guest(pp) ? "PauseMenuDataMgr GOT slot not a guest pointer" : "bad name / no SafeString vtable / no function");
        return -1;
    }
    if (!in_guest(mgr)) return -2;
    Cpu save = *c;                             // restored below (x, sp, flags, v)
    uint64_t sp = (c->sp - 0xa0) & ~0xfull;    // scratch below the live guest sp: +0 SafeString, +0x20 name
    uint8_t *g = (uint8_t *)(uintptr_t)sp;
    memset(g, 0, 0xa0);
    uint64_t ss[2] = {vt + 0x10, sp + 0x20};
    memcpy(g, ss, 16);
    memcpy(g + 0x20, name, len);
    c->sp = sp;
    c->x[0] = mgr, c->x[1] = sp;
    guest_call(c, fn);
    *c = save;
    if (!g_npouch_rm++) LOG("[linkpos] first pouch remove: %s (PauseMenuDataMgr %llx)\n", name, (unsigned long long)mgr);
    return 1;
}

// s99 b14-runes: choose the Sheikah Slate rune like BotW's own wheel does (recon/S99_HELPER_RUNES.md). The wheel screen's
// confirm calls f(obj = [rune_sel_ptr], id) (1.9.0 0x7100b33d6c); the object's per-frame update then sees +0x240 != +0x248,
// refreshes the HUD icon, sets +0x248 and writes the gdt s32 PlayerEquipItemType. ids: 0 round bomb, 1 cube bomb, 2 Magnesis,
// 3 Stasis, 4 Cryonis, 5 Camera (6 Master Cycle, 7 amiibo). Only inside linkpos_frame (a guest call). 1 = called, -1 = refused
// (no site / not now), -2 = no rune object yet, -3 = id out of 0..7.
#define RUNE_SET_REQ (SS->rune_set_req)
#define RUNE_SEL_PTR (SS->rune_sel_ptr)
static uint64_t g_nrune_set, g_nrune_refused;
int linkpos_rune_select(int id) {
    Cpu *c = g_in_frame ? kernel_current_cpu() : NULL;
    uint64_t fn = RUNE_SET_REQ, pp = RUNE_SEL_PTR;
    uint64_t obj = fn && in_guest(pp) ? *(const uint64_t *)(uintptr_t)pp : 0;
    if (id < 0 || id > 7) return -3;
    if (!c || !fn || !in_guest(pp) || !dispatch_lookup(fn)) {
        if (!g_nrune_refused++) LOG("[linkpos] rune select refused (%s)\n", !fn ? "no site for this game version" : !c ? (g_in_frame ? "no guest cpu" : "outside linkpos_frame") : "pointer var not a guest pointer / no function");
        return -1;
    }
    if (!in_guest(obj)) return -2;
    Cpu save = *c;
    c->x[0] = obj, c->x[1] = (uint64_t)id;
    guest_call(c, fn);
    *c = save;
    if (!g_nrune_set++) LOG("[linkpos] first rune select: id %d (rune object %llx)\n", id, (unsigned long long)obj);
    return 1;
}
// the rune object's requested (+0x240) and current (+0x248) ids, -1 = none. 1 = read, 0 = no object / no site. Plain reads (any thread).
int linkpos_rune_get(int *req, int *cur) {
    uint64_t pp = RUNE_SET_REQ ? RUNE_SEL_PTR : 0;
    uint64_t obj = in_guest(pp) ? *(const uint64_t *)(uintptr_t)pp : 0;
    if (!in_guest(obj) || !in_guest(obj + 0x250)) return 0;
    if (req) *req = *(const int32_t *)(uintptr_t)(obj + 0x240);
    if (cur) *cur = *(const int32_t *)(uintptr_t)(obj + 0x248);
    return 1;
}

// ------------------------------------------------------------------ breakables (s55: Sora breaks boxes, barrels, pots)
// recon/S55_HELPER_BREAK.md. BotW's GameObject damage step f_700046839c (actor vslot 160) breaks an actor whose
// DamageParam has Breakable set as soon as its life is below 1: life = 0, then killWithDropsAndEffects(0) (vslot 54:
// the break effect, the vanish, its drop table). So a hit of Sora's = a life write here (game main thread, inside
// sora_frame: the context of the god-mode life write); BotW does the rest. The list comes from the opacity hook:
// non-enemy actors with ActorParam (+0x570) -> DamageParam (+0xd8) -> Breakable (+0x418) set, a "MapDynamic*" profile
// (ActorParam +0x68 SafeString, C string at +0x70; leaves out GuardianComponent legs), a damage manager (+0x840: without
// one the damage step returns before the break) and within BREAK_DIST u of Link.
//   SAVAGEWIND_LINK_BREAK_LOG=1   each breakable once (name, profile, life, damage manager + its slots 22 / 47, the
//                                 actor's vtable + slot 160, the rates) and every hit
//   SAVAGEWIND_LINK_NO_BREAK=1    no list (sora.c then breaks nothing)
#include "linkpos_break.h"
#define ACTOR_PARAM 0x570
#define AP_PROFILE_CSTR 0x70
#define AP_DAMAGE_PARAM 0xd8
#define DP_BREAKABLE 0x418
#define DP_RATE_N 0x3b8
#define DP_RATE_P 0x3c0
#define DP_LIGHTNING 0xd68
#define BASEPROC_STATE 0x64  // (u8: 3 = being deleted; FUN_7100991e20 tests it before killWithDropsAndEffects)
#define BREAK_MAX 64
#define BREAK_DIST 30.0f
#define BREAK_STALE 60
typedef struct { LinkposBreakable b; uint64_t vt, ap, seen; } Brk;
static Brk g_brk[BREAK_MAX];
static int g_nbrk;
static SRWLOCK g_brk_lock = SRWLOCK_INIT;

static uint64_t brk_dp(uint64_t a, uint64_t *ap_out) {  // its DamageParam if it is a breakable (0: not one)
    uint64_t ap = *(const uint64_t *)(uintptr_t)(a + ACTOR_PARAM);
    if (!in_guest(ap)) return 0;
    uint64_t dp = *(const uint64_t *)(uintptr_t)(ap + AP_DAMAGE_PARAM);
    if (!in_guest(dp) || !*(const uint8_t *)(uintptr_t)(dp + DP_BREAKABLE)) return 0;
    if (ap_out) *ap_out = ap;
    return dp;
}

static void break_first_log(uint64_t a, const Brk *e, const char *profile) {
    uint64_t vt = *(const uint64_t *)(uintptr_t)a, dm = *(const uint64_t *)(uintptr_t)(a + ACTOR_DAMAGE_MGR);
    uint64_t dmvt = in_guest(dm) ? *(const uint64_t *)(uintptr_t)dm : 0;
    const float *r = e->b.rate;
    LOG("[linkpos] breakable: %llx %s (%s) at %.2f %.2f %.2f life %d | vtable %llx slot160 %llx | dm %llx vtable %llx "
        "calcDamage %llx addDamage %llx | rate sword %.2f large %.2f arrow %.2f bomb %.2f | lightning %d\n",
        (unsigned long long)a, e->b.name, profile, e->b.pos[0], e->b.pos[1], e->b.pos[2], e->b.life, (unsigned long long)vt,
        in_guest(vt) ? (unsigned long long)*(const uint64_t *)(uintptr_t)(vt + 8 * 160) : 0ull, (unsigned long long)dm,
        (unsigned long long)dmvt, in_guest(dmvt) ? (unsigned long long)*(const uint64_t *)(uintptr_t)(dmvt + 8 * 22) : 0ull,
        in_guest(dmvt) ? (unsigned long long)*(const uint64_t *)(uintptr_t)(dmvt + 8 * 47) : 0ull, r[BRK_SWORD],
        r[BRK_LARGESWORD], r[BRK_ARROW], r[BRK_BOMB], e->b.lightning);
}

static void break_note(uint64_t a) {  // (opacity hook, any thread)
    static int on = -1, log_on = -1;
    if (on < 0) on = !getenv("SAVAGEWIND_LINK_NO_BREAK"), log_on = env_int("SAVAGEWIND_LINK_BREAK_LOG", 0);
    if (!on || !g_servo_actor || a == g_servo_actor) return;
    uint64_t ap = 0, dp = brk_dp(a, &ap);
    if (!dp || !in_guest(*(const uint64_t *)(uintptr_t)(a + ACTOR_DAMAGE_MGR))) return;
    const char *name = actor_name(a);
    if (!name || enemy_name_ok(name) || link_dist(a) > BREAK_DIST) return;
    uint64_t pc = *(const uint64_t *)(uintptr_t)(ap + AP_PROFILE_CSTR);
    const char *profile = in_guest(pc) ? (const char *)(uintptr_t)pc : "";
    if (strncmp(profile, "MapDynamic", 10)) return;
    const float *m = (const float *)(uintptr_t)(a + ACTOR_MTX);
    int32_t life = *(const int32_t *)(uintptr_t)(a + ACTOR_LIFE);
    AcquireSRWLockExclusive(&g_brk_lock);
    int i;
    for (i = 0; i < g_nbrk && g_brk[i].b.actor != a; i++) {}
    bool fresh = i == g_nbrk;
    if (fresh) {
        if (g_nbrk == BREAK_MAX) { ReleaseSRWLockExclusive(&g_brk_lock); return; }
        Brk *e = &g_brk[g_nbrk++];
        memset(e, 0, sizeof *e);
        e->b.actor = a, e->vt = *(const uint64_t *)(uintptr_t)a, e->ap = ap;
        snprintf(e->b.name, sizeof e->b.name, "%s", name);
        uint32_t nr = *(const uint32_t *)(uintptr_t)(dp + DP_RATE_N);
        uint64_t rp = *(const uint64_t *)(uintptr_t)(dp + DP_RATE_P);
        for (int k = 0; k < BRK_NRATE; k++)
            e->b.rate[k] = k < (int)nr && in_guest(rp) ? *(const float *)(uintptr_t)(rp + 0x20 * k + 0x18) : 0;
        e->b.lightning = *(const uint8_t *)(uintptr_t)(dp + DP_LIGHTNING);
    }
    Brk *e = &g_brk[i];
    e->seen = g_vel_frame_id;
    e->b.pos[0] = m[3], e->b.pos[1] = m[7], e->b.pos[2] = m[11];
    e->b.life = life;
    if (life > e->b.life_max) e->b.life_max = life;
    Brk copy = *e;
    ReleaseSRWLockExclusive(&g_brk_lock);
    if (fresh && log_on) break_first_log(a, &copy, profile);
}

int linkpos_breakables(LinkposBreakable *out, int max) {  // (the ones drawn this or the last frame, life > 0)
    AcquireSRWLockExclusive(&g_brk_lock);
    int n = 0;
    for (int i = 0; i < g_nbrk && n < max; i++)
        if (g_vel_frame_id - g_brk[i].seen <= 1 && g_brk[i].b.life > 0) out[n++] = g_brk[i].b;
    ReleaseSRWLockExclusive(&g_brk_lock);
    return n;
}

// life -= n (clamped at 0) on a breakable drawn this or the last frame and unchanged since its first sight (vtable,
// ActorParam, Breakable, not being deleted); only inside linkpos_frame (sora_frame). The life after it, -1 = refused.
int linkpos_break_hit(uint64_t actor, int n) {
    static int log_on = -1;
    if (log_on < 0) log_on = env_int("SAVAGEWIND_LINK_BREAK_LOG", 0);
    if (!g_in_frame || n <= 0) return -1;
    int after = -1;
    AcquireSRWLockExclusive(&g_brk_lock);
    for (int i = 0; i < g_nbrk; i++) {
        Brk *e = &g_brk[i];
        if (e->b.actor != actor) continue;
        uint64_t ap = 0;
        if (g_vel_frame_id - e->seen <= 1 && *(const uint64_t *)(uintptr_t)actor == e->vt &&
            *(const uint8_t *)(uintptr_t)(actor + BASEPROC_STATE) != 3 && brk_dp(actor, &ap) && ap == e->ap) {
            int32_t *life = (int32_t *)(uintptr_t)(actor + ACTOR_LIFE);
            int before = *life;
            *life = before > n ? before - n : 0;
            after = e->b.life = *life;
            if (log_on) LOG("[linkpos] break hit %llx %s: life %d -> %d\n", (unsigned long long)actor, e->b.name, before, after);
        }
        break;
    }
    ReleaseSRWLockExclusive(&g_brk_lock);
    return after;
}

static void break_frame(void) {  // (game thread, once a frame: drop stale entries)
    AcquireSRWLockExclusive(&g_brk_lock);
    for (int i = 0; i < g_nbrk;)
        if (g_vel_frame_id - g_brk[i].seen > BREAK_STALE) g_brk[i] = g_brk[--g_nbrk];
        else i++;
    ReleaseSRWLockExclusive(&g_brk_lock);
}

// ------------------------------------------------------------------ world (s122: Sora cuts trees, hits Stasis'd things)
// recon/S122_HELPER_TREES.md + S122_HELPER_STASIS.md (+ S122_HELPER_SITES190.md for the 1.9.0 rows).
// TREES: a cuttable tree is a MapDynamic* actor whose root AI (+0x558) is LumberjackTree (vtable tree_ai_vt; ai+0x8 =
// the actor, +0x240 mode 0 = live cuttable, +0x24c felled). BotW fells it from that AI's update only when a real weapon
// hit (f54 != -1) and life <= 0 meet, so a life write alone does nothing: linkpos_tree_hit counts the hits on the
// tree's life (+0x848, 40) and on the last one writes the fall direction (ai+0x2b8 -> MoveDirection vec3, horizontal,
// away from Sora) and calls the AI's own fell function tree_fell(ai) (game main thread, inside linkpos_frame). The
// trunk, stump, drops and fall are BotW's own next frames.
// STASIS: BotW keeps up to 16 frozen actors in a pool (*stasis_pool: entries at +0x60 + i * 0x320, a BaseProcLink with
// the actor id at +8 (== actor +0x60, -1 free), the StasisInfo at entry +0x10). linkpos_stasis_hit calls BotW's own
// StasisInfo::add(info, actor, &dir, &pos, count, angle, noPush, kind) = what a weapon hit does (newest direction,
// magnitudes add up, the orange arrow grows; Stasis' end launches it). Only for an actor whose damage manager has the
// Stasis override (dm vtable slot 17 == stasis_dm_slot17).
//   SAVAGEWIND_LINK_WORLD_LOG=1   each tree / Stasis object once + every hit
//   SAVAGEWIND_LINK_NO_TREES=1 / SAVAGEWIND_LINK_NO_STASIS=1   no list (sora.c then does nothing)
#include "linkpos_world.h"
#define ACTOR_ROOT_AI 0x558  // (also in the redirect section below; same value)
#define TREE_FELL (SS->tree_fell)
#define TREE_AI_VT (SS->tree_ai_vt)
#define STASIS_ADD (SS->stasis_add)
#define STASIS_POOL (SS->stasis_pool)
#define STASIS_DM_SLOT17 (SS->stasis_dm_slot17)
#define AI_ACTOR 0x8
#define TAI_HELPER 0x38
#define TAI_MODE 0x240
#define TAI_FELLED 0x24c
#define TAI_MOVEDIR 0x2b8
#define ACTOR_FELL_GUARD 0x694  // (tree_fell returns at once while it is not 0)
#define ACTOR_PROC_ID 0x60      // (s32: what a BaseProcLink's +8 holds)
#define SP_ENTRY0 0x60
#define SP_STRIDE 0x320
#define SP_N 16
#define SP_LINK_ID 8
#define SP_INFO 0x10
#define SI_COUNT 0x27c
#define SI_VALID 0x27d
#define SI_VEC 0x280
#define DM_VT_STASIS 17
#define DM_STASIS_CACHE 0x220  // (u64: the StasisInfo BotW's launch / arrow getters read; s122b)
#define TAI_NCHILD 0x28        // (ksys Ai node: u32 child count, u64 child array; same as AI_NCHILD / AI_CHILDREN below)
#define TAI_CHILDREN 0x30
#define TAI_KIDS_MAX 16
#define TREE_MAX 48
#define TREE_DIST 14.0f
#define STOBJ_MAX 16
#define STOBJ_DIST 25.0f
#define WORLD_STALE 60
typedef struct { LinkposTree t; uint64_t vt, ai, seen; } TreeE;
typedef struct { LinkposStasisObj o; uint64_t vt, seen; int slot; } StobjE;
static TreeE g_tree[TREE_MAX];
static int g_ntree;
static StobjE g_stobj[STOBJ_MAX];
static int g_nstobj;
static int32_t g_sp_id[SP_N];  // (the pool's actor ids, read once a frame by world_frame; -1 free)
static volatile int g_sp_any;
static SRWLOCK g_world_lock = SRWLOCK_INIT;
static int g_world_log = -1, g_tree_on, g_stasis_on;

static void world_env(void) {
    if (g_world_log >= 0) return;
    g_tree_on = !getenv("SAVAGEWIND_LINK_NO_TREES"), g_stasis_on = !getenv("SAVAGEWIND_LINK_NO_STASIS");
    g_world_log = env_int("SAVAGEWIND_LINK_WORLD_LOG", 0);
}

// (s123) the LumberjackTree node: +0x558 itself, or (GUESS, s122b worlddiag "trees 0") a child / grandchild of it when
// +0x558 is a RootAi wrapper. Matching vtable AND ai+8 == the actor.
static bool tree_is_ai(uint64_t n, uint64_t a) {
    return in_guest(n) && *(const uint64_t *)(uintptr_t)n == TREE_AI_VT && *(const uint64_t *)(uintptr_t)(n + AI_ACTOR) == a;
}
static uint64_t tree_find_ai(uint64_t node, uint64_t a, int depth) {
    if (!in_guest(node)) return 0;
    if (tree_is_ai(node, a)) return node;
    if (depth >= 2) return 0;
    uint32_t cnt = *(const uint32_t *)(uintptr_t)(node + TAI_NCHILD);
    uint64_t kids = *(const uint64_t *)(uintptr_t)(node + TAI_CHILDREN);
    if (!cnt || cnt > 64 || !in_guest(kids)) return 0;
    for (uint32_t k = 0; k < cnt && k < TAI_KIDS_MAX; k++) {
        uint64_t r = tree_find_ai(*(const uint64_t *)(uintptr_t)(kids + 8ull * k), a, depth + 1);
        if (r) return r;
    }
    return 0;
}
static uint64_t tree_ai(uint64_t a) {  // its LumberjackTree AI if it is a live cuttable tree (0: not one)
    if (!TREE_AI_VT) return 0;
    uint64_t ai = tree_find_ai(*(const uint64_t *)(uintptr_t)(a + ACTOR_ROOT_AI), a, 0);
    if (!ai) return 0;
    if (*(const uint8_t *)(uintptr_t)(ai + TAI_MODE) != 0 || *(const uint8_t *)(uintptr_t)(ai + TAI_FELLED)) return 0;
    return ai;
}

// (s123) first TREE_DBG_MAX MapDynamic actors with "Tree" in the name that tree_ai rejects: their +0x558 node and its
// children (vtables, actor at +8, child counts) so one run shows where LumberjackTree sits. Always on (stops itself).
#define TREE_DBG_MAX 12
static volatile LONG g_tree_dbg_n;
static uint64_t g_tree_dbg_seen[TREE_DBG_MAX];
static void tree_dbg(uint64_t a) {
    uint64_t ap = *(const uint64_t *)(uintptr_t)(a + ACTOR_PARAM);
    uint64_t pc = in_guest(ap) ? *(const uint64_t *)(uintptr_t)(ap + AP_PROFILE_CSTR) : 0;
    if (!in_guest(pc) || strncmp((const char *)(uintptr_t)pc, "MapDynamic", 10)) return;
    const char *name = actor_name(a);
    if (!name || !strstr(name, "Tree")) return;
    for (int i = 0; i < TREE_DBG_MAX; i++) if (g_tree_dbg_seen[i] == a) return;
    LONG k = InterlockedIncrement(&g_tree_dbg_n) - 1;
    if (k >= TREE_DBG_MAX) return;
    g_tree_dbg_seen[k] = a;
    char b[640];
    uint64_t n = *(const uint64_t *)(uintptr_t)(a + ACTOR_ROOT_AI);
    int len = snprintf(b, sizeof b, "[linkpos] treedbg %s %llx (tree_ai_vt %llx): +0x558 %llx", name, (unsigned long long)a,
                       (unsigned long long)TREE_AI_VT, (unsigned long long)n);
    if (in_guest(n)) {
        uint32_t cnt = *(const uint32_t *)(uintptr_t)(n + TAI_NCHILD);
        uint64_t kids = *(const uint64_t *)(uintptr_t)(n + TAI_CHILDREN);
        len += snprintf(b + len, sizeof b - (size_t)len, " vt %llx actor %llx kids %u", (unsigned long long)*(const uint64_t *)(uintptr_t)n,
                        (unsigned long long)*(const uint64_t *)(uintptr_t)(n + AI_ACTOR), cnt);
        for (uint32_t c = 0; in_guest(kids) && cnt <= 64 && c < cnt && c < 4 && len < (int)sizeof b - 96; c++) {
            uint64_t ch = *(const uint64_t *)(uintptr_t)(kids + 8ull * c);
            if (!in_guest(ch)) { len += snprintf(b + len, sizeof b - (size_t)len, " | c%u %llx", c, (unsigned long long)ch); continue; }
            len += snprintf(b + len, sizeof b - (size_t)len, " | c%u %llx vt %llx actor %llx kids %u", c, (unsigned long long)ch,
                            (unsigned long long)*(const uint64_t *)(uintptr_t)ch, (unsigned long long)*(const uint64_t *)(uintptr_t)(ch + AI_ACTOR),
                            *(const uint32_t *)(uintptr_t)(ch + TAI_NCHILD));
        }
    }
    LOG("%s\n", b);
}

static void tree_note(uint64_t a) {  // (opacity hook, any thread)
    world_env();
    if (!g_tree_on || !TREE_FELL || !g_servo_actor || a == g_servo_actor || link_dist(a) > TREE_DIST) return;
    uint64_t ai = tree_ai(a);
    if (!ai) { if (g_tree_dbg_n < TREE_DBG_MAX) tree_dbg(a); return; }
    uint64_t ap = *(const uint64_t *)(uintptr_t)(a + ACTOR_PARAM);
    uint64_t pc = in_guest(ap) ? *(const uint64_t *)(uintptr_t)(ap + AP_PROFILE_CSTR) : 0;
    if (!in_guest(pc) || strncmp((const char *)(uintptr_t)pc, "MapDynamic", 10)) return;
    const char *name = actor_name(a);
    if (!name) return;
    const float *m = (const float *)(uintptr_t)(a + ACTOR_MTX);
    int32_t life = *(const int32_t *)(uintptr_t)(a + ACTOR_LIFE);
    AcquireSRWLockExclusive(&g_world_lock);
    int i;
    for (i = 0; i < g_ntree && g_tree[i].t.actor != a; i++) {}
    bool fresh = i == g_ntree;
    if (fresh) {
        if (g_ntree == TREE_MAX) { ReleaseSRWLockExclusive(&g_world_lock); return; }
        TreeE *e = &g_tree[g_ntree++];
        memset(e, 0, sizeof *e);
        e->t.actor = a, e->vt = *(const uint64_t *)(uintptr_t)a, e->ai = ai;
        snprintf(e->t.name, sizeof e->t.name, "%s", name);
    }
    TreeE *e = &g_tree[i];
    e->seen = g_vel_frame_id;
    e->t.pos[0] = m[3], e->t.pos[1] = m[7], e->t.pos[2] = m[11];
    e->t.life = life;
    if (life > e->t.life_max) e->t.life_max = life;
    ReleaseSRWLockExclusive(&g_world_lock);
    if (fresh && g_world_log)
        LOG("[linkpos] tree: %llx %s at %.2f %.2f %.2f life %d | ai %llx helper %llx movedir %llx guard %d | dm %llx\n",
            (unsigned long long)a, name, m[3], m[7], m[11], life, (unsigned long long)ai,
            (unsigned long long)*(const uint64_t *)(uintptr_t)(ai + TAI_HELPER),
            (unsigned long long)*(const uint64_t *)(uintptr_t)(ai + TAI_MOVEDIR), *(const int32_t *)(uintptr_t)(a + ACTOR_FELL_GUARD),
            (unsigned long long)*(const uint64_t *)(uintptr_t)(a + ACTOR_DAMAGE_MGR));
}

int linkpos_trees(LinkposTree *out, int max) {  // (the ones drawn this or the last frame, still standing)
    AcquireSRWLockExclusive(&g_world_lock);
    int n = 0;
    for (int i = 0; i < g_ntree && n < max; i++)
        if (g_vel_frame_id - g_tree[i].seen <= 1 && g_tree[i].t.life > 0) out[n++] = g_tree[i].t;
    ReleaseSRWLockExclusive(&g_world_lock);
    return n;
}

// life -= n on a listed tree (drawn this or the last frame, same vtable / AI, not being deleted); at <= 0 the fall
// (direction dx, dz = horizontal, away from Sora). Only inside linkpos_frame. Life after (0 = felled), -1 = refused.
int linkpos_tree_hit(uint64_t actor, int n, float dx, float dz) {
    world_env();
    if (!g_in_frame || n <= 0 || !TREE_FELL) return -1;
    uint64_t ai = 0;
    int before = -1, after = -1;
    char name[48] = "";
    AcquireSRWLockExclusive(&g_world_lock);
    for (int i = 0; i < g_ntree; i++) {
        TreeE *e = &g_tree[i];
        if (e->t.actor != actor) continue;
        if (g_vel_frame_id - e->seen <= 1 && *(const uint64_t *)(uintptr_t)actor == e->vt &&
            *(const uint8_t *)(uintptr_t)(actor + BASEPROC_STATE) != 3 && tree_ai(actor) == e->ai) {
            ai = e->ai;
            int32_t *life = (int32_t *)(uintptr_t)(actor + ACTOR_LIFE);
            before = *life;
            after = before > n ? before - n : 0;
            *life = after > 0 ? after : 1;  // (0 only once the fall really started, below)
            e->t.life = after;
            memcpy(name, e->t.name, sizeof name);
        }
        break;
    }
    ReleaseSRWLockExclusive(&g_world_lock);
    if (!ai) return -1;
    if (after > 0) {
        if (g_world_log) LOG("[linkpos] tree hit %llx %s: life %d -> %d\n", (unsigned long long)actor, name, before, after);
        return after;
    }
    Cpu *c = kernel_current_cpu();
    int guard = *(const int32_t *)(uintptr_t)(actor + ACTOR_FELL_GUARD);
    uint64_t mv = *(const uint64_t *)(uintptr_t)(ai + TAI_MOVEDIR);
    if (!c || !dispatch_lookup(TREE_FELL) || guard || !*(const uint64_t *)(uintptr_t)(ai + TAI_HELPER)) {
        LOG("[linkpos] tree fell %llx %s refused (%s)\n", (unsigned long long)actor, name, !c ? "no guest cpu" :
            guard ? "actor +0x694 set" : !dispatch_lookup(TREE_FELL) ? "no function" : "no AI helper");
        return 1;
    }
    float l = sqrtf(dx * dx + dz * dz);
    if (in_guest(mv) && l > 1e-4f) {
        float *d = (float *)(uintptr_t)mv;
        d[0] = dx / l, d[1] = 0, d[2] = dz / l;
    }
    Cpu save = *c;
    c->x[0] = ai;
    guest_call(c, TREE_FELL);
    *c = save;
    bool felled = *(const uint8_t *)(uintptr_t)(ai + TAI_FELLED) != 0;
    if (felled) *(int32_t *)(uintptr_t)(actor + ACTOR_LIFE) = 0;
    LOG("[linkpos] tree %llx %s: life %d -> 0, fell %s (dir %.2f %.2f%s)\n", (unsigned long long)actor, name, before,
        felled ? "STARTED" : "did not start", l > 1e-4f ? dx / l : 0, l > 1e-4f ? dz / l : 0, in_guest(mv) ? "" : ", no MoveDirection");
    return felled ? 0 : 1;
}

static uint64_t stasis_pool(void) {
    if (!STASIS_POOL) return 0;
    uint64_t p = *(const uint64_t *)(uintptr_t)STASIS_POOL;
    return in_guest(p) ? p : 0;
}

static void stasis_note(uint64_t a) {  // (opacity hook, any thread: only actors whose id is in the pool right now)
    world_env();
    if (!g_stasis_on || !g_sp_any || !STASIS_ADD || !g_servo_actor || a == g_servo_actor) return;
    int32_t id = *(const int32_t *)(uintptr_t)(a + ACTOR_PROC_ID);
    int slot = -1;
    for (int k = 0; k < SP_N; k++) if (g_sp_id[k] != -1 && g_sp_id[k] == id) { slot = k; break; }
    if (slot < 0 || link_dist(a) > STOBJ_DIST) return;
    uint64_t pool = stasis_pool();
    if (!pool) return;
    const uint8_t *info = (const uint8_t *)(uintptr_t)(pool + SP_ENTRY0 + (uint64_t)slot * SP_STRIDE + SP_INFO);
    const float *v = (const float *)(info + SI_VEC), *m = (const float *)(uintptr_t)(a + ACTOR_MTX);
    const char *name = actor_name(a);
    AcquireSRWLockExclusive(&g_world_lock);
    int i;
    for (i = 0; i < g_nstobj && g_stobj[i].o.actor != a; i++) {}
    bool fresh = i == g_nstobj;
    if (fresh) {
        if (g_nstobj == STOBJ_MAX) { ReleaseSRWLockExclusive(&g_world_lock); return; }
        StobjE *e = &g_stobj[g_nstobj++];
        memset(e, 0, sizeof *e);
        e->o.actor = a, e->vt = *(const uint64_t *)(uintptr_t)a;
        snprintf(e->o.name, sizeof e->o.name, "%s", name ? name : "?");
    }
    StobjE *e = &g_stobj[i];
    e->seen = g_vel_frame_id, e->slot = slot;
    e->o.pos[0] = m[3], e->o.pos[1] = m[7], e->o.pos[2] = m[11];
    e->o.hits = info[SI_COUNT];
    e->o.stored = info[SI_VALID] ? sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) : 0;
    ReleaseSRWLockExclusive(&g_world_lock);
    if (fresh && g_world_log) {
        uint64_t dm = *(const uint64_t *)(uintptr_t)(a + ACTOR_DAMAGE_MGR), dmvt = in_guest(dm) ? *(const uint64_t *)(uintptr_t)dm : 0;
        LOG("[linkpos] stasis object: %llx %s id %d pool slot %d at %.2f %.2f %.2f | dm %llx slot17 %llx (want %llx)\n",
            (unsigned long long)a, name ? name : "?", id, slot, m[3], m[7], m[11], (unsigned long long)dm,
            in_guest(dmvt) ? (unsigned long long)*(const uint64_t *)(uintptr_t)(dmvt + 8 * DM_VT_STASIS) : 0ull,
            (unsigned long long)STASIS_DM_SLOT17);
    }
}

int linkpos_stasis_objs(LinkposStasisObj *out, int max) {  // (Stasis'd now, drawn this or the last frame)
    AcquireSRWLockExclusive(&g_world_lock);
    int n = 0;
    for (int i = 0; i < g_nstobj && n < max; i++)
        if (g_vel_frame_id - g_stobj[i].seen <= 1) out[n++] = g_stobj[i].o;
    ReleaseSRWLockExclusive(&g_world_lock);
    return n;
}

int linkpos_stasis_hit(uint64_t actor, float dx, float dy, float dz, int kind, int flags) {
    world_env();
    if (!g_in_frame || !in_guest(actor) || !STASIS_ADD) return -1;
    int slot = -1;
    uint64_t vt = 0;
    AcquireSRWLockExclusive(&g_world_lock);
    for (int i = 0; i < g_nstobj; i++)
        if (g_stobj[i].o.actor == actor) { if (g_vel_frame_id - g_stobj[i].seen <= 1) slot = g_stobj[i].slot, vt = g_stobj[i].vt; break; }
    ReleaseSRWLockExclusive(&g_world_lock);
    uint64_t pool = stasis_pool();
    if (slot < 0 || !pool || *(const uint64_t *)(uintptr_t)actor != vt || *(const uint8_t *)(uintptr_t)(actor + BASEPROC_STATE) == 3)
        return -1;
    uint64_t ent = pool + SP_ENTRY0 + (uint64_t)slot * SP_STRIDE;
    if (*(const int32_t *)(uintptr_t)(ent + SP_LINK_ID) != *(const int32_t *)(uintptr_t)(actor + ACTOR_PROC_ID)) return -1;  // (re-check now)
    uint64_t dm = *(const uint64_t *)(uintptr_t)(actor + ACTOR_DAMAGE_MGR), dmvt = in_guest(dm) ? *(const uint64_t *)(uintptr_t)dm : 0;
    if (!in_guest(dmvt) || *(const uint64_t *)(uintptr_t)(dmvt + 8 * DM_VT_STASIS) != STASIS_DM_SLOT17) {
        static int said;
        if (said++ < 8) LOG("[linkpos] stasis hit %llx refused: its damage manager has no Stasis override\n", (unsigned long long)actor);
        return -1;
    }
    Cpu *c = kernel_current_cpu();
    if (!c || !dispatch_lookup(STASIS_ADD)) return -1;
    uint64_t info = ent + SP_INFO;
    // (s122b/s123) BotW's launch getter (dm vslot 14) and arrow ratio (vslot 13) read the StasisInfo through dm +0x220,
    // which only BotW's own hit entry (vslot 17) fills: set it to this pool entry's info first (as the DLL's st_cache).
    if (*(const uint64_t *)(uintptr_t)(dm + DM_STASIS_CACHE) != info) {
        LOG("[linkpos] stasis: dm %llx cache %llx -> info %llx (slot %d)\n", (unsigned long long)dm,
            (unsigned long long)*(const uint64_t *)(uintptr_t)(dm + DM_STASIS_CACHE), (unsigned long long)info, slot);
        *(uint64_t *)(uintptr_t)(dm + DM_STASIS_CACHE) = info;
    }
    const float *v = (const float *)(uintptr_t)(info + SI_VEC), *m = (const float *)(uintptr_t)(actor + ACTOR_MTX);
    float before = *(const uint8_t *)(uintptr_t)(info + SI_VALID) ? sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) : 0;
    Cpu save = *c;
    uint64_t sp = (c->sp - 0x80) & ~0xfull;  // (scratch below the live guest sp: dir at +0, pos at +0x10)
    float *g = (float *)(uintptr_t)sp;
    memset(g, 0, 0x40);
    g[0] = dx, g[1] = dy, g[2] = dz, g[4] = m[3], g[5] = m[7], g[6] = m[11];
    c->sp = sp;
    c->x[0] = info, c->x[1] = actor, c->x[2] = sp, c->x[3] = sp + 0x10;
    c->x[4] = flags & 1, c->x[5] = (flags >> 1) & 1, c->x[6] = (flags >> 2) & 1, c->x[7] = (uint32_t)kind;
    guest_call(c, STASIS_ADD);
    *c = save;
    float after = *(const uint8_t *)(uintptr_t)(info + SI_VALID) ? sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) : 0;
    LOG("[linkpos] stasis hit %llx (slot %d, kind %d, flags %d): stored %.2f -> %.2f, hits %d, vec %.2f %.2f %.2f\n",
        (unsigned long long)actor, slot, kind, flags, before, after, *(const uint8_t *)(uintptr_t)(info + SI_COUNT), v[0], v[1], v[2]);
    return 1;
}

static void world_frame(void) {  // (game thread, once a frame: the pool's ids, drop stale entries)
    world_env();
    uint64_t pool = g_stasis_on && STASIS_ADD ? stasis_pool() : 0;
    int any = 0;
    for (int k = 0; k < SP_N; k++) {
        g_sp_id[k] = pool ? *(const int32_t *)(uintptr_t)(pool + SP_ENTRY0 + (uint64_t)k * SP_STRIDE + SP_LINK_ID) : -1;
        any |= g_sp_id[k] != -1;
    }
    g_sp_any = any;
    AcquireSRWLockExclusive(&g_world_lock);
    for (int i = 0; i < g_ntree;)
        if (g_vel_frame_id - g_tree[i].seen > WORLD_STALE) g_tree[i] = g_tree[--g_ntree];
        else i++;
    for (int i = 0; i < g_nstobj;)
        if (g_vel_frame_id - g_stobj[i].seen > WORLD_STALE) g_stobj[i] = g_stobj[--g_nstobj];
        else i++;
    ReleaseSRWLockExclusive(&g_world_lock);
}

// ------------------------------------------------------------------ fire world (s59: Sora's Fire lights BotW things)
// recon/S59_HELPER_FIRE.md. Three BotW entry points (Ghidra addresses - 0x100000000), all guest calls ONLY inside
// linkpos_frame (sora_frame: the game main thread, between two frames - linkpos_raycast's window), -1 elsewhere:
//  - ignite: chem = actor vslot 97 getChemicalStuff (+0x308: [actor+0x6a8] holder -> its first chemical), then
//    f_7000fddd24(chem, 0, 2, 0, 1, 0) = what FireBurnBase::handleMessage does on BotW's ignite message 0x800002b and
//    what the enemies' fire reaction (f_7000a73ec4 case 2) does: chemical state -> 2 (burning) now (its own update
//    f_7000fdccb8 runs inside). The actor's AI then lights itself (FireBurnBase calc: chem+0xc0 == 2 -> burn on), the
//    burning chemical emits its material's burn element (chemi_fire), an enemy's damage manager starts BotW's burn
//    (f_7000a74188: DamageParam Burnable, BurnDamage). It skips by itself a material without attribute bit 0
//    (stone, iron) or a wet chemical (+0xbe bit 0).
//  - spark: f_70012f24a0(&{u64 owner 0, x, y, z, radius, life}) = the Flint action's calc (f_70005452b8: one per
//    contact point; Item_Ore_I radius 1.0, life 5): a fire-and-forget 'spark' element (temperature 1000, type 8).
//  - burst: f_7001318da4(&{x, y, z, u8 1}) = ElementLightningFire::create (lightning strike, f_7001318080): a
//    fire-and-forget 'lightning_fire' element (temperature 10000, fire_level 3, 15 frames) - grass fires.
// The fire-object list (opacity hook): non-enemy actors named like a fire object (FireWood, CookSet, Torch, Candle,
// Lamp, IvyBurn) with a chemical holder (+0x6a8), within FIRE_DIST u of Link.
//   SAVAGEWIND_LINK_FIRE_LOG=1    each fire object once + every ignite / spark / burst
//   SAVAGEWIND_LINK_FIRE_ANY=1    any non-enemy actor with a chemical (trees, boxes, ... : all burnable ones burn)
//   SAVAGEWIND_LINK_NO_FIRE=1     no list, every call refused (sora.c then does nothing)
#include "linkpos_fire.h"
#define ACTOR_CHEMICAL 0x6a8
#define VT_GET_CHEMICAL 97
#define CHEM_MATERIAL 0x20      // (chm material params: attribute u32 at +0x1f0, bit 0 = burnable)
#define CHEM_MAT_ATTR 0x1f0
#define CHEM_STATE 0xc0         // (u8 now: 2 = burning; +0xc1 = the requested one)
#define FIRE_IGNITE (SS->fire_ignite)
#define FIRE_SPARK (SS->fire_spark)
#define FIRE_BURST (SS->fire_burst)
#define FIRE_MAX 48
#define FIRE_DIST 20.0f
typedef struct { LinkposFireObj o; uint64_t vt, seen; } FireObj;
static FireObj g_fire[FIRE_MAX];
static int g_nfire;
static SRWLOCK g_fire_lock = SRWLOCK_INIT;
static int g_fire_on = -1, g_fire_any, g_fire_log;

static void fire_env(void) {
    if (g_fire_on >= 0) return;
    g_fire_any = env_int("SAVAGEWIND_LINK_FIRE_ANY", 0), g_fire_log = env_int("SAVAGEWIND_LINK_FIRE_LOG", 0);
    g_fire_on = !getenv("SAVAGEWIND_LINK_NO_FIRE");
}

static bool fire_name_ok(const char *n) {
    static const char *const K[] = {"FireWood", "CookSet", "Torch", "Candle", "Lamp", "IvyBurn"};
    for (size_t i = 0; i < sizeof K / sizeof *K; i++) if (strstr(n, K[i])) return true;
    return false;
}

static void fire_note(uint64_t a) {  // (opacity hook, any thread)
    fire_env();
    if (!g_fire_on || !g_servo_actor || a == g_servo_actor || is_link_equip(a)) return;
    if (!in_guest(*(const uint64_t *)(uintptr_t)(a + ACTOR_CHEMICAL))) return;
    const char *name = actor_name(a);
    if (!name || enemy_name_ok(name) || !(g_fire_any || fire_name_ok(name)) || link_dist(a) > FIRE_DIST) return;
    const float *m = (const float *)(uintptr_t)(a + ACTOR_MTX);
    AcquireSRWLockExclusive(&g_fire_lock);
    int i;
    for (i = 0; i < g_nfire && g_fire[i].o.actor != a; i++) {}
    bool fresh = i == g_nfire;
    if (fresh) {
        if (g_nfire == FIRE_MAX) {  // (full: take the stalest slot)
            i = 0;
            for (int k = 1; k < g_nfire; k++) if (g_fire[k].seen < g_fire[i].seen) i = k;
        } else g_nfire++;
        memset(&g_fire[i], 0, sizeof *g_fire);
        g_fire[i].o.actor = a, g_fire[i].vt = *(const uint64_t *)(uintptr_t)a;
        snprintf(g_fire[i].o.name, sizeof g_fire[i].o.name, "%s", name);
    }
    g_fire[i].seen = g_vel_frame_id;
    g_fire[i].o.pos[0] = m[3], g_fire[i].o.pos[1] = m[7], g_fire[i].o.pos[2] = m[11];
    LinkposFireObj copy = g_fire[i].o;
    ReleaseSRWLockExclusive(&g_fire_lock);
    if (fresh && g_fire_log)
        LOG("[linkpos] fire object: %llx %s at %.2f %.2f %.2f (vtable %llx, chemical holder %llx)\n",
            (unsigned long long)a, copy.name, copy.pos[0], copy.pos[1], copy.pos[2],
            (unsigned long long)*(const uint64_t *)(uintptr_t)a,
            (unsigned long long)*(const uint64_t *)(uintptr_t)(a + ACTOR_CHEMICAL));
}

int linkpos_fire_objects(LinkposFireObj *out, int max) {  // (the ones drawn this or the last frame)
    AcquireSRWLockExclusive(&g_fire_lock);
    int n = 0;
    for (int i = 0; i < g_nfire && n < max; i++)
        if (g_vel_frame_id - g_fire[i].seen <= 1) out[n++] = g_fire[i].o;
    ReleaseSRWLockExclusive(&g_fire_lock);
    return n;
}

static bool fire_actor_ok(uint64_t a) {  // a listed fire object or a listed enemy, drawn this or the last frame
    bool ok = false;
    AcquireSRWLockExclusive(&g_fire_lock);
    for (int i = 0; i < g_nfire && !ok; i++)
        ok = g_fire[i].o.actor == a && g_vel_frame_id - g_fire[i].seen <= 1 && *(const uint64_t *)(uintptr_t)a == g_fire[i].vt;
    ReleaseSRWLockExclusive(&g_fire_lock);
    AcquireSRWLockExclusive(&g_enemy_lock);
    for (int i = 0; i < g_nenemy && !ok; i++) ok = g_enemy[i].actor == a && g_vel_frame_id - g_enemy[i].seen <= 1;
    ReleaseSRWLockExclusive(&g_enemy_lock);
    return ok && *(const uint8_t *)(uintptr_t)(a + BASEPROC_STATE) != 3;
}

static Cpu *fire_cpu(const char *what, uint64_t fn) {
    Cpu *c = g_in_frame ? kernel_current_cpu() : NULL;
    fire_env();
    if (g_fire_on && c && in_guest(fn) && dispatch_lookup(fn)) return c;
    static int n;
    if (n++ < 8) LOG("[linkpos] fire %s refused (%s)\n", what, !g_fire_on ? "SAVAGEWIND_LINK_NO_FIRE" : !g_in_frame ?
                     "outside linkpos_frame" : !c ? "no guest cpu" : "no function");
    return NULL;
}

// 1 = burning now, 2 = asked (state not 2 yet), 0 = nothing to do (no chemical, not burnable, burning already), -1 refused
int linkpos_fire_ignite(uint64_t actor) {
    if (!in_guest(actor) || !fire_actor_ok(actor)) return -1;
    uint64_t vt = *(const uint64_t *)(uintptr_t)actor;
    uint64_t getc = in_guest(vt) ? *(const uint64_t *)(uintptr_t)(vt + 8 * VT_GET_CHEMICAL) : 0;
    Cpu *c = fire_cpu("ignite", getc);
    if (!c || !dispatch_lookup(FIRE_IGNITE)) return -1;
    Cpu save = *c;
    c->x[0] = actor;
    guest_call(c, getc);
    uint64_t chem = c->x[0];
    int r = 0, before = -1, attr = -1;
    if (in_guest(chem)) {
        uint64_t mat = *(const uint64_t *)(uintptr_t)(chem + CHEM_MATERIAL);
        before = *(const uint8_t *)(uintptr_t)(chem + CHEM_STATE);
        attr = in_guest(mat) ? (int)*(const uint32_t *)(uintptr_t)(mat + CHEM_MAT_ATTR) : -1;
        if (before != 2 && attr >= 0 && (attr & 1)) {
            *c = save;
            c->x[0] = chem, c->x[1] = 0, c->x[2] = 2, c->x[3] = 0, c->x[4] = 1, c->x[5] = 0;
            guest_call(c, FIRE_IGNITE);
            uint8_t now = *(const uint8_t *)(uintptr_t)(chem + CHEM_STATE), asked = *(const uint8_t *)(uintptr_t)(chem + CHEM_STATE + 1);
            r = now == 2 ? 1 : asked == 2 ? 2 : 0;
        }
    }
    *c = save;
    if (g_fire_log) {
        const char *nm = actor_name(actor);
        LOG("[linkpos] fire ignite %llx %s: chem %llx attr %#x state %d -> %s\n", (unsigned long long)actor, nm ? nm : "?",
            (unsigned long long)chem, attr, before, r == 1 ? "burning" : r == 2 ? "asked" : "no change");
    }
    return r;
}

static int fire_elem(const char *what, uint64_t fn, const void *arg, size_t n) {
    Cpu *c = fire_cpu(what, fn);
    if (!c) return -1;
    Cpu save = *c;
    uint64_t sp = (c->sp - 0x80) & ~0xfull;  // (scratch below the live guest sp, as linkpos_raycast)
    memset((void *)(uintptr_t)sp, 0, 0x40);
    memcpy((void *)(uintptr_t)sp, arg, n);
    c->sp = sp, c->x[0] = sp;
    guest_call(c, fn);
    *c = save;
    return 1;
}

int linkpos_fire_spark(const float p[3], float radius, float life) {
    struct { uint64_t owner; float x, y, z, radius, life; } a = {0, p[0], p[1], p[2], radius, life};
    int r = fire_elem("spark", FIRE_SPARK, &a, sizeof a);
    if (g_fire_log && r > 0) LOG("[linkpos] fire spark at %.2f %.2f %.2f r %.2f life %.1f\n", p[0], p[1], p[2], radius, life);
    return r;
}

int linkpos_fire_burst(const float p[3]) {
    struct { float x, y, z; uint8_t on, pad[3]; } a = {p[0], p[1], p[2], 1, {0}};
    int r = fire_elem("burst", FIRE_BURST, &a, sizeof a);
    if (g_fire_log && r > 0) LOG("[linkpos] fire burst at %.2f %.2f %.2f\n", p[0], p[1], p[2]);
    return r;
}

// ------------------------------------------------------------------ redirect (s56: enemies go for Donald / Goofy)
// recon/S55_HELPER_PARTY_COMBAT.md 2 (a), wip_s55_party_combat/botw_target/BOTW_ENEMY_TARGET.md. Every
// uking::act::Enemy keeps a target record T at enemy+0xc48 (BaseProcLink to the target, its position +0xc60, its
// matrix +0xc6c, an aim point +0xc9c, the last-seen position +0xcb8, a state +0xcc4). T::update f_7000690228 (x0 = T)
// copies the target actor's matrix into it every frame; its only caller is the bl in Enemy vslot 76 f_700046a780
// (shared by every Enemy-derived class), and the AIs walk / turn / swing at +0xc60 (getTargetPos: 523 sites). So the
// hook calls the original, then, for an enemy in the table whose state is 2 (seen) or 4 (lost, timer running), moves
// +0xc60, the matrix translation, the aim point (same offset to the position) and, in state 2, the last-seen position
// to the table's point. The link stays Link: every validity check keeps passing; the ~170 link-based reads (and its
// line-of-sight test, 15 frames) still see Link, and its weapon only really hits him.
//   linkpos_redirect(enemy, pos, frames)  the enemy (an actor of linkpos_enemies' list) aims at pos (BotW units) for
//                                         frames game frames (renewed by calling again; <= 0 = clear); 1 = set
//   linkpos_redirect_clear(enemy)         drop it (0 = all): how many were dropped
//   linkpos_enemy_leaf(enemy, buf, n)     its AI tree path, "root/.../leaf" (BotW's own names, UTF-8: the battle AI's
//                                         attack child is "攻撃"): the number of names, 0 = none / not now; only inside
//                                         linkpos_frame (guest calls), for an enemy drawn this or the last frame
// Entries go on: time up, the enemy's death (die hook), target state 0 after it was moved (it lost Link), mode 0xd
// (+0x14d8: scripted fights force Link), life 0, BaseProc state 3 (being deleted), out of the enemy list (unloaded),
// Link's actor changing or going (area load / loading), a reboot.
//   SAVAGEWIND_LINK_REDIRECT_TEST=1   the nearest Enemy_* within 40 u is sent to a fixed point 3 m beside Link
//                                     (picked once; again after it drops out); its line every 60 frames
//   SAVAGEWIND_LINK_REDIRECT_LOG=1    every entry every 60 frames (state, target, feet, point, AI path)
//   SAVAGEWIND_LINK_NO_REDIRECT=1     leave the hook out
#define TGT_UPDATE (SS->tgt_update)
#define TGT_UPDATE_CALLER (SS->tgt_update_caller)  // (bl at +0x118: one direct call, swgame.dll a9e435a100d6c119)
#define TGT_REC 0xc48
#define TGT_POS 0xc60
#define TGT_MTX_TX 0xc78
#define TGT_MTX_TY 0xc88
#define TGT_MTX_TZ 0xc98
#define TGT_AIM 0xc9c
#define TGT_LAST_SEEN 0xcb8
#define TGT_STATE 0xcc4
#define TGT_KEEP_LAST 0xd68  // (u8 T+0x120: set = T::update leaves last-seen alone in state 2)
#define ENEMY_MODE 0x14d8     // (s32; 0xd = forced Link target, Enemy::updateTarget)
#define ENEMY_MODE_FORCE_LINK 0xd
#define ACTOR_ROOT_AI 0x558   // (ksys::act::Actor mRootAi: zeldaret 1.5.0; checked per call by the node's actor)
#define ACTION_ACTOR 0x8      // (ActionBase: vtable, actor; s16 definition index +0x18: f_7000f6227c)
#define AI_CHILD_IDX 0x1c     // (Ai vslot 26 f_7001423630: u16 index, 0xffff none; u32 count +0x28; children +0x30)
#define AI_NCHILD 0x28
#define AI_CHILDREN 0x30
#define VT_AI_GET_TYPE 27     // (0 = Ai: f_70006a7318 in EnemyBaseFindPlayer's vtable)
#define AI_GET_NAME (SS->ai_get_name)
#define AI_DEPTH_MAX 8
#define REDIR_MAX 8
#define REDIR_FRAMES_MAX 36000
#define REDIR_TEST_RANGE 40.0f
#define REDIR_TEST_SIDE 3.0f
typedef struct { uint64_t enemy, until, applied; float pos[3], tpos[3], feet[3]; int32_t state; bool waited; } Redir;
static Redir g_redir[REDIR_MAX];
static volatile int g_nredir;  // (the hook's fast path reads it without the lock)
static SRWLOCK g_redir_lock = SRWLOCK_INIT;
static uint64_t g_redir_logged[64];  // (the "redirect" line: once per enemy)
static int g_nredir_logged;
static GuestFn g_tgt_update;

static bool redir_first_log(uint64_t e) {  // (under g_redir_lock)
    for (int k = 0; k < g_nredir_logged && k < 64; k++)
        if (g_redir_logged[k] == e) return false;
    g_redir_logged[g_nredir_logged++ % 64] = e;  // (a ring of the last 64 enemies)
    return true;
}

static void tgt_update(Cpu *c) {
    uint64_t t = c->x[0];
    g_tgt_update(c);
    if (!g_nredir) return;
    uint64_t e = t - TGT_REC, applied = 0;
    const char *why = NULL;
    bool first = false, wait = false;
    float from[3], to[3];
    AcquireSRWLockExclusive(&g_redir_lock);
    int i;
    for (i = 0; i < g_nredir && g_redir[i].enemy != e; i++) {}
    if (i == g_nredir) { ReleaseSRWLockExclusive(&g_redir_lock); return; }
    Redir *r = &g_redir[i];
    int32_t st = *(const int32_t *)(uintptr_t)(e + TGT_STATE), mode = *(const int32_t *)(uintptr_t)(e + ENEMY_MODE);
    int32_t life = *(const int32_t *)(uintptr_t)(e + ACTOR_LIFE);
    why = st == 0 && r->applied ? "target state 0 (lost Link)" : mode == ENEMY_MODE_FORCE_LINK ? "mode 0xd (forced Link target)"
        : life <= 0 ? "life 0" : *(const uint8_t *)(uintptr_t)(e + BASEPROC_STATE) == 3 ? "being deleted"
        : g_vel_frame_id >= r->until ? "time up" : NULL;
    if (why) {
        applied = r->applied;
        g_redir[i] = g_redir[--g_nredir];
        ReleaseSRWLockExclusive(&g_redir_lock);
        LOG("[linkpos] redirect %llx cleared: %s (moved %llu frames)\n", (unsigned long long)e, why, (unsigned long long)applied);
        return;
    }
    const float *m = (const float *)(uintptr_t)(e + ACTOR_MTX);
    r->state = st, r->feet[0] = m[3], r->feet[1] = m[7], r->feet[2] = m[11];
    if (st != 2 && st != 4) {
        wait = !r->waited, r->waited = true;
        ReleaseSRWLockExclusive(&g_redir_lock);
        if (wait) LOG("[linkpos] redirect %llx waiting: target state %d (moved only in 2 / 4)\n", (unsigned long long)e, st);
        return;
    }
    float *p = (float *)(uintptr_t)(e + TGT_POS), *aim = (float *)(uintptr_t)(e + TGT_AIM);
    float d[3] = {r->pos[0] - p[0], r->pos[1] - p[1], r->pos[2] - p[2]};
    memcpy(from, p, sizeof from);
    for (int k = 0; k < 3; k++) p[k] += d[k], aim[k] += d[k];
    *(float *)(uintptr_t)(e + TGT_MTX_TX) += d[0];
    *(float *)(uintptr_t)(e + TGT_MTX_TY) += d[1];
    *(float *)(uintptr_t)(e + TGT_MTX_TZ) += d[2];
    if (st == 2 && !*(const uint8_t *)(uintptr_t)(e + TGT_KEEP_LAST)) memcpy((void *)(uintptr_t)(e + TGT_LAST_SEEN), p, 12);
    memcpy(r->tpos, p, sizeof r->tpos), memcpy(to, p, sizeof to);
    if (!r->applied++) first = redir_first_log(e);
    ReleaseSRWLockExclusive(&g_redir_lock);
    if (first) {
        const char *name = actor_name(e);
        LOG("[linkpos] redirect %llx %s: target %.2f %.2f %.2f -> %.2f %.2f %.2f (state %d, feet %.2f %.2f %.2f, game frame %llu)\n",
            (unsigned long long)e, name ? name : "?", from[0], from[1], from[2], to[0], to[1], to[2], st, m[3], m[7], m[11],
            (unsigned long long)g_vel_frame_id);
    }
}

static int redir_drop(uint64_t enemy, const char *why) {  // (enemy 0: all; any thread)
    uint64_t gone[REDIR_MAX];
    int n = 0;
    AcquireSRWLockExclusive(&g_redir_lock);
    for (int i = 0; i < g_nredir;)
        if (!enemy || g_redir[i].enemy == enemy) gone[n++] = g_redir[i].enemy, g_redir[i] = g_redir[--g_nredir];
        else i++;
    ReleaseSRWLockExclusive(&g_redir_lock);
    for (int k = 0; k < n && why; k++) LOG("[linkpos] redirect %llx cleared: %s\n", (unsigned long long)gone[k], why);
    return n;
}

static bool enemy_listed(uint64_t a, uint64_t within) {  // in the enemy list, drawn within that many game frames
    bool ok = false;
    AcquireSRWLockExclusive(&g_enemy_lock);
    for (int i = 0; i < g_nenemy; i++)
        if (g_enemy[i].actor == a) { ok = g_vel_frame_id - g_enemy[i].seen <= within; break; }
    ReleaseSRWLockExclusive(&g_enemy_lock);
    return ok;
}

int linkpos_redirect(uint64_t enemy, const float pos[3], int frames) {
    if (!enemy || !pos) return 0;
    if (frames <= 0) { redir_drop(enemy, "cleared by caller (frames <= 0)"); return 0; }
    static int warned_hook, warned_list, warned_full;
    if (!g_tgt_update) {
        if (!warned_hook++) LOG("[linkpos] redirect refused: no target update hook\n");
        return 0;
    }
    if (!isfinite(pos[0]) || !isfinite(pos[1]) || !isfinite(pos[2])) return 0;
    if (!enemy_listed(enemy, ENEMY_STALE)) {
        if (!warned_list++) LOG("[linkpos] redirect refused: %llx is not in the enemy list\n", (unsigned long long)enemy);
        return 0;
    }
    if (frames > REDIR_FRAMES_MAX) frames = REDIR_FRAMES_MAX;
    AcquireSRWLockExclusive(&g_redir_lock);
    int i;
    for (i = 0; i < g_nredir && g_redir[i].enemy != enemy; i++) {}
    if (i == g_nredir) {
        if (g_nredir == REDIR_MAX) {
            ReleaseSRWLockExclusive(&g_redir_lock);
            if (!warned_full++) LOG("[linkpos] redirect refused: table full (%d)\n", REDIR_MAX);
            return 0;
        }
        memset(&g_redir[i], 0, sizeof g_redir[i]);
        g_redir[i].enemy = enemy;
        g_nredir++;
    }
    memcpy(g_redir[i].pos, pos, sizeof g_redir[i].pos);
    g_redir[i].until = g_vel_frame_id + (uint64_t)frames;
    ReleaseSRWLockExclusive(&g_redir_lock);
    return 1;
}

int linkpos_redirect_clear(uint64_t enemy) { return redir_drop(enemy, "cleared by caller"); }

int linkpos_enemy_leaf(uint64_t enemy, char *buf, int n) {
    if (!buf || n <= 0) return 0;
    buf[0] = 0;
    static int warned, warned_node;
    Cpu *c = g_in_frame ? kernel_current_cpu() : NULL;
    if (!c || !dispatch_lookup(AI_GET_NAME)) {
        if (!warned++) LOG("[linkpos] enemy leaf refused (%s)\n", !c ? (g_in_frame ? "no guest cpu" : "outside linkpos_frame") : "no name function");
        return 0;
    }
    if (!enemy_listed(enemy, 1)) return 0;
    uint64_t node = *(const uint64_t *)(uintptr_t)(enemy + ACTOR_ROOT_AI);
    int depth = 0;
    size_t len = 0;
    Cpu save = *c;                                 // restored below (x, sp, flags, v)
    uint64_t sp = (c->sp - 0x40) & ~0xfull;        // below the live guest sp
    while (depth < AI_DEPTH_MAX && in_guest(node)) {
        if (*(const uint64_t *)(uintptr_t)(node + ACTION_ACTOR) != enemy) {
            if (!warned_node++) LOG("[linkpos] enemy leaf: %llx's AI node %llx (depth %d) has actor %llx: walk stopped\n",
                                    (unsigned long long)enemy, (unsigned long long)node, depth,
                                    (unsigned long long)*(const uint64_t *)(uintptr_t)(node + ACTION_ACTOR));
            break;
        }
        uint64_t vt = *(const uint64_t *)(uintptr_t)node, gt = in_guest(vt) ? *(const uint64_t *)(uintptr_t)(vt + 8 * VT_AI_GET_TYPE) : 0;
        if (!gt || !dispatch_lookup(gt)) break;
        c->sp = sp, c->x[0] = node;
        guest_call(c, AI_GET_NAME);
        uint64_t nm = c->x[0];
        c->sp = sp, c->x[0] = node;
        guest_call(c, gt);
        int32_t type = (int32_t)c->x[0];
        if (len + 2 < (size_t)n) snprintf(buf + len, (size_t)n - len, "%s%s", depth ? "/" : "", in_guest(nm) ? (const char *)(uintptr_t)nm : "?");
        len = strlen(buf), depth++;
        if (type != 0) break;  // (an Action / Behavior: the leaf)
        uint32_t idx = *(const uint16_t *)(uintptr_t)(node + AI_CHILD_IDX), cnt = *(const uint32_t *)(uintptr_t)(node + AI_NCHILD);
        uint64_t kids = *(const uint64_t *)(uintptr_t)(node + AI_CHILDREN);
        if (idx == 0xffff || !cnt || !in_guest(kids)) break;
        node = *(const uint64_t *)(uintptr_t)(kids + 8ull * (idx < cnt ? idx : 0));  // (the game's own clamp)
    }
    *c = save;
    return depth;
}

static void redir_test(uint64_t link) {  // (SAVAGEWIND_LINK_REDIRECT_TEST, game thread)
    static uint64_t enemy, skip, skip_until;
    static float point[3];
    if (enemy && !enemy_listed(enemy, ENEMY_STALE)) LOG("[linkpos] redirect test: %llx gone\n", (unsigned long long)enemy), enemy = 0;
    if (!enemy) {
        float bd = REDIR_TEST_RANGE;
        char name[48] = "";
        AcquireSRWLockExclusive(&g_enemy_lock);
        for (int i = 0; i < g_nenemy; i++)
            if (g_enemy[i].dist >= 0 && g_enemy[i].dist < bd && !strncmp(g_enemy[i].name, "Enemy_", 6) &&
                !(g_enemy[i].actor == skip && g_vel_frame_id < skip_until) && g_vel_frame_id - g_enemy[i].seen <= 1)
                bd = g_enemy[i].dist, enemy = g_enemy[i].actor, snprintf(name, sizeof name, "%s", g_enemy[i].name);
        ReleaseSRWLockExclusive(&g_enemy_lock);
        if (!enemy) return;
        const float *m = (const float *)(uintptr_t)(link + ACTOR_MTX);  // (rotation rows: column 0 = his local x axis)
        float sx = m[0], sz = m[8], sl = sqrtf(sx * sx + sz * sz);
        if (sl < 1e-4f) sx = 1, sz = 0, sl = 1;
        point[0] = m[3] + REDIR_TEST_SIDE * sx / sl, point[1] = m[7], point[2] = m[11] + REDIR_TEST_SIDE * sz / sl;
        LOG("[linkpos] redirect test: %llx %s at %.1f u -> fixed point %.2f %.2f %.2f (%.0f m beside Link at %.2f %.2f %.2f)\n",
            (unsigned long long)enemy, name, bd, point[0], point[1], point[2], REDIR_TEST_SIDE, m[3], m[7], m[11]);
        linkpos_redirect(enemy, point, 30);
        return;
    }
    bool kept = false;
    AcquireSRWLockExclusive(&g_redir_lock);
    for (int i = 0; i < g_nredir; i++) kept |= g_redir[i].enemy == enemy;
    ReleaseSRWLockExclusive(&g_redir_lock);
    if (!kept) {  // (the hook dropped it: no re-add every frame; another one in 10 s)
        LOG("[linkpos] redirect test: %llx dropped, next pick in 600 frames\n", (unsigned long long)enemy);
        skip = enemy, skip_until = g_vel_frame_id + 600, enemy = 0;
        return;
    }
    linkpos_redirect(enemy, point, 30);
}

static void redir_frame(void) {  // (linkpos_frame, game thread, inside g_in_frame)
    static uint64_t link_was = 1;
    static int test = -1, log_on = -1;
    if (test < 0) test = env_int("SAVAGEWIND_LINK_REDIRECT_TEST", 0), log_on = env_int("SAVAGEWIND_LINK_REDIRECT_LOG", 0) || test;
    uint64_t link = servo_body() ? g_servo_actor : 0;
    if (link != link_was) {
        if (link_was != 1) redir_drop(0, link ? "Link's actor changed (area load)" : "no Link (loading)");
        link_was = link;
    }
    if (!g_tgt_update) return;
    if (g_nredir) {  // (the hook never runs for an unloaded enemy: time up and the list's staleness are checked here)
        uint64_t gone[REDIR_MAX];
        int ngone = 0;
        AcquireSRWLockExclusive(&g_redir_lock);
        for (int i = 0; i < g_nredir;)
            if (g_vel_frame_id >= g_redir[i].until + ENEMY_STALE) gone[ngone++] = g_redir[i].enemy, g_redir[i] = g_redir[--g_nredir];
            else i++;
        ReleaseSRWLockExclusive(&g_redir_lock);
        for (int k = 0; k < ngone; k++) LOG("[linkpos] redirect %llx cleared: time up (no target update since)\n", (unsigned long long)gone[k]);
        uint64_t check[REDIR_MAX];
        int ncheck = 0;
        AcquireSRWLockExclusive(&g_redir_lock);
        for (int i = 0; i < g_nredir; i++) check[ncheck++] = g_redir[i].enemy;
        ReleaseSRWLockExclusive(&g_redir_lock);
        for (int k = 0; k < ncheck; k++)
            if (!enemy_listed(check[k], ENEMY_STALE)) redir_drop(check[k], "not in the enemy list (unloaded)");
    }
    if (test && link) redir_test(link);
    if (!log_on || !g_nredir || g_vel_frame_id % 60) return;
    Redir copy[REDIR_MAX];
    int n;
    AcquireSRWLockExclusive(&g_redir_lock);
    n = g_nredir;
    memcpy(copy, g_redir, sizeof *copy * (size_t)n);
    ReleaseSRWLockExclusive(&g_redir_lock);
    for (int k = 0; k < n; k++) {
        const Redir *r = &copy[k];
        char path[160];
        int depth = linkpos_enemy_leaf(r->enemy, path, sizeof path);
        float dx = r->feet[0] - r->pos[0], dz = r->feet[2] - r->pos[2];
        LOG("[linkpos] redirect %llx: state %d, moved %llu frames, %llu left, target %.2f %.2f %.2f, feet %.2f %.2f %.2f "
            "(%.2f u flat from the point), AI %s\n", (unsigned long long)r->enemy, r->state, (unsigned long long)r->applied,
            (unsigned long long)(r->until > g_vel_frame_id ? r->until - g_vel_frame_id : 0), r->tpos[0], r->tpos[1], r->tpos[2],
            r->feet[0], r->feet[1], r->feet[2], sqrtf(dx * dx + dz * dz), depth ? path : "-");
    }
}

static void redir_init(void) {  // (startup, after die_init: before any guest code runs)
    if (getenv("SAVAGEWIND_LINK_NO_REDIRECT")) { LOG("[linkpos] redirect hook off (SAVAGEWIND_LINK_NO_REDIRECT)\n"); return; }
    if (!(g_tgt_update = dispatch_lookup(TGT_UPDATE))) { LOG("[linkpos] no function at %llx: no redirect hook\n", TGT_UPDATE); return; }
    hle_override(TGT_UPDATE, tgt_update, "linkpos enemy target update");  // (indirect calls: none known)
    int n = hook_calls(TGT_UPDATE_CALLER, TGT_UPDATE, tgt_update);
    LOG("[linkpos] redirect hook: target update %llx (+%d of 1 bl site in %llx)%s\n", TGT_UPDATE, n, TGT_UPDATE_CALLER,
        n ? "" : ": NOT hooked, redirects will not move anything");
}

#include "linkpos_esync_block.inc"  // (s168 esync: the six linkpos_esync_* exports, the RootAi / sensor hooks)

// Link's actor matrix (mMtx +0x398, Matrix34f: rotation rows, translation at +0xc / +0x1c / +0x2c = his feet).
bool linkpos_link_mtx(float m[12]) {
    if (!servo_body()) return false;
    memcpy(m, (const void *)(uintptr_t)(g_servo_actor + ACTOR_MTX), 48);
    return true;
}

// (s87c link face) Link's yaw = Sora's facing. Sora calls linkpos_set_link_yaw(yaw) every frame she drives (ground / air);
// linkpos_frame applies it once, right after sora_frame, and clears the request - so a frame without a call (climb,
// swim, event, load, death) leaves Link's own rotation alone. yaw: radians, direction (sin, cos) in BotW x, z - the
// same convention as the puppet's atan2f(m[2], m[10]) (sora.c): the actor matrix is a column-vector RotY(yaw), rows
// (c 0 s) (0 1 0) (-s 0 c). Only a near-upright matrix is touched (slope pitch kept out of it); position untouched.
// GUESS: BotW may rebuild mMtx from its own rotation later in the frame (then this only shows in code that reads the
// matrix after us); the actor's mRotation field is not known on 1.8.2, so it is not written.
// (s87c review) recon/SAVAGE_WIND_RECON.md s19 probe: mScale is its own field (+0x418), so the rows are pure rotation
// (writing unit rows keeps his scale); other candidates NOT written (unknown roles): the mMtx copy at +0x1b18, PlayerBase
// "last world move direction" +0x1dd4 / +0x1dd8. The kept / lost count below says whether BotW rewrites the matrix
// between our writes (lost high = it rebuilds mMtx from its own rotation: then one of those is the next thing to try).
static bool g_yaw_req;
static float g_yaw;
void linkpos_set_link_yaw(float yaw) { g_yaw_req = true, g_yaw = yaw; }

static void yaw_frame(void) {
    static bool wrote;  // (s87c review: last frame wrote yaw_last)
    static float yaw_last;
    static unsigned kept, lost;
    bool req = g_yaw_req;
    g_yaw_req = false;
    if (!req || *(uint32_t *)(uintptr_t)PAUSE_STATE || !servo_body()) { wrote = false; return; }
    int st = state_of(g_servo_actor);
    float *m = (float *)(uintptr_t)(g_servo_actor + ACTOR_MTX);
    if (wrote) {  // (did our last write survive BotW's frame? > 0.01 rad off = rewritten)
        float d = remainderf(atan2f(m[2], m[10]) - yaw_last, 6.2831853f);
        if (fabsf(d) < 0.01f) kept++;
        else lost++;
        if (kept + lost == 300 || (kept + lost) % 3600 == 0)
            LOG("[linkpos] link face: our yaw kept %u / lost %u frames (lost high = BotW rebuilds mMtx)\n", kept, lost);
    }
    wrote = false;
    if (st != LINK_GROUND && st != LINK_AIR) return;  // (his own states keep their facing)
    if (!(g_yaw == g_yaw) || m[5] < 0.9f) return;
    float s = sinf(g_yaw), c = cosf(g_yaw);
    m[0] = c, m[1] = 0, m[2] = s, m[4] = 0, m[5] = 1, m[6] = 0, m[8] = -s, m[9] = 0, m[10] = c;
    wrote = true, yaw_last = g_yaw;
}

void linkpos_frame(void) {
    static int init;
    if (!g_ss.player_info) return;  // (no sites: linkpos_init)
    servo_frame();
    vel_frame();
    cam_frame();
    cam_override_probe();  // (s137 fps: did the camera write survive BotW's last frame?)
    g_in_frame = true;  // (linkpos_raycast allowed)
    { extern void sora_frame(void); sora_frame(); }  // (runtime/sora.c: Sora's movement, before this frame's physics)
    cam_override_apply();  // (s137 fps: the first-person camera write, if the DLL asked for one)
    yaw_frame();  // (s87c link face)
    redir_frame();  // (s56: the redirect table's upkeep + its test; reads AI leaf names, hence inside)
    esync_frame();  // (s168: brain-off table, puppet sensors, name table; guest calls, hence inside)
    g_in_frame = false;
    hide_frame();
    enemy_frame();
    break_frame();
    world_frame();  // (s122 trees / Stasis)
    life_frame();
    watch_frame(g_frame);
    if (!init) {
        init = 1;
        probe_init();
        g_every = env_int("SAVAGEWIND_LINK_LOG_EVERY", 1);
        g_flush = env_int("SAVAGEWIND_LINK_LOG_FLUSH", 60);
        if (g_every < 1) g_every = 1;
        if (g_flush < 1) g_flush = 1;
        const char *path = getenv("SAVAGEWIND_LINK_LOG");
        if (path && *path) {
            g_log = fopen(path, "a");
            if (!g_log) LOG("[linkpos] can't open %s\n", path);
        }
        if (g_log) {
            fprintf(g_log, "# savagewind link position log: frame x y z (PlayerInfo [%#llx] +%#x)\n",
                    (unsigned long long)PLAYER_INFO_PTR, PLAYER_POS);
            fflush(g_log);
            LOG("[linkpos] logging Link's position to %s (every %d frames)\n", path, g_every);
        }
    }
    if (!g_log && !g_ptxt && !g_pbin && !g_isbin && !g_ccbin && !g_bbin && !g_rbin) return;
    uint64_t frame = g_frame++;
    uint64_t pinfo = *(uint64_t *)(uintptr_t)PLAYER_INFO_PTR;
    if (in_guest(pinfo) && (g_ptxt || g_pbin || g_isbin || g_ccbin)) probe_frame(frame, pinfo);
    if (!g_log || frame % (uint64_t)g_every) return;
    if (pinfo != g_last_pinfo) {  // (0 before the game's managers exist, after a reboot; a new one each boot)
        g_last_pinfo = pinfo;
        uint64_t actor = in_guest(pinfo) ? *(uint64_t *)(uintptr_t)(pinfo + PLAYER_ACTOR) : 0;
        fprintf(g_log, "# frame %llu: PlayerInfo %llx (player actor %llx)\n", (unsigned long long)frame,
                (unsigned long long)pinfo, (unsigned long long)actor);
        LOG("[linkpos] PlayerInfo %llx at frame %llu\n", (unsigned long long)pinfo, (unsigned long long)frame);
    }
    if (!in_guest(pinfo)) return;
    const float *p = (const float *)(uintptr_t)(pinfo + PLAYER_POS);
    fprintf(g_log, "%llu %.3f %.3f %.3f\n", (unsigned long long)frame, p[0], p[1], p[2]);
    if (++g_lines % (uint64_t)g_flush == 0) fflush(g_log);
}

RT_ON_RESET(linkpos_reset) {  // in-place reboot: guest memory is wiped; the frame count starts again
    g_last_pinfo = 0;
    redir_drop(0, "reboot");
    // (s111 review) a camera scan still running / waiting holds addresses of the wiped world: drop it (the worker sees
    // the new gen and frees its own result), so a stale result is never applied and the present-count restart cannot
    // leave g_scan_pending stuck
    InterlockedIncrement(&g_scan_gen);
    free(InterlockedExchangePointer((PVOID volatile *)&g_scan_done, NULL));
    g_scan_pending = false;
    if (g_log) { fprintf(g_log, "# reboot\n"); fflush(g_log); }
}

#include "linkpos_api_block.inc"  // (s175 idea 1: sw_api(), at the end: it uses everything above)
