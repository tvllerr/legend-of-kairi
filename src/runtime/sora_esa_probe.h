// s176 R2 diag: does a Bokoblin's club hit turn on? (DLL only, BotW 1.9.0 addresses, recon/s174/S174_ATK_VPTRS.md)
// The Attack action's "rod attack" helper (embedded at action node + 0x40, vtable 0x7001c35980) runs per frame (its vslot 9,
// 0x7000779e88): if the weapon is usable it asks vslot 16 (+0x80) "is the AS in its hit window?" and, yes -> vslot 15 (+0x78)
// = club hit ON. We hook slots 9 / 16 / 15 on that vtable and count per helper (both games: the owner = what a working hit
// looks like, the puppet = what our runner gives). Live `set esaprobe 1` hooks, `0` unhooks; a line every 2 s per active helper:
//   [esaprobe] helper H (node H-0x40): calc N, window asked N / yes N, club ON N
// calc without "asked" = the helper's first check (weapon not usable / no weapon) said no.
// s178 R2 CLUB SYNC (live `set esaclub 0|1`, default 1; the same hooks, installed while esaclub (+ esync) or esaprobe is on):
//   owner: its own club ON (vslot 15) / OFF ("hold ended" vslot 17 = yes) are queued as esync events (name ES_EV_CLUB, params[0] =
//   ms after its swing start = vslot 8 prep, [1] [2] = the hold's two floats); es_step sends only those of enemies we own.
//   puppet: the event waits until ITS OWN swing (the DLL runner's copy of the attack) is as far in; at the next window ask (vslot 16)
//   / "ended" ask (vslot 17) we answer yes ourselves, so the game's own calc turns the club on / off with its own args (the one-tick
//   AS edge the puppet's ~9/s calc misses: recon/s177/S177_HITWINDOW.md). The key the game passes to vslot 15 is filled from the
//   puppet's own type-3 hold event (D+0x940) when it has one, else only the owner's floats.
#pragma once

#define ESP_VT 0x7001c35980ull
#define ESP_CALC 0x7000779e88ull  // (vslot 9 must hold this, else the probe refuses: other game version)
#define ESP_PREP 0x7000779e5cull  // vslot 8: weapon prep, called by the Attack action's enter_ = swing start (s177)
#define ESP_LEAVE 0x7000779ff8ull // vslot 10: club OFF + release, called by leave_ = swing end (s177)
#define ESP_N 32
#define ESP_RING 32
#define ESC_RING 64
#define ESC_PEND 32
#define ESC_EXPIRE_MS 2500  // a puppet event not used by then is dropped (its swing ended / never came)
#define ESC_EARLY_MS 15     // apply this much before the owner's ms (the puppet's calc runs ~every 110 ms anyway)
#define ESC_OLD_MS 400      // at the puppet's swing start: events that arrived more than this BEFORE it belong to an older swing
// s177 swing timing: one record per swing (prep -> leave): ms of the first yes / club ON after the start, calcs, largest calc gap
typedef struct { uint64_t helper; double len, yes_at, on_at, max_gap; uint32_t calc, ask, yes; } EspSwing;
static struct {
    const SwApi *api;
    int id[5];  // hook ids for slot 9, 16, 15, 8, 10 (-1 = none)
    int on;
    struct { uint64_t helper; uint32_t calc, ask, yes, club; uint32_t seen[4];
             int in_swing; double t0, last_calc, max_gap, yes_at, on_at; uint32_t c0, a0, y0; uint32_t hid; } h[ESP_N];
    EspSwing ring[ESP_RING];
    uint32_t ring_w, ring_r;  // (writer: guest threads under ring_lock; reader: esp_tick)
    SRWLOCK ring_lock;
    int id17;   // s178 club sync: slot 17 ("hold ended" ask)
    int hooked; // hooks in (esaprobe or esaclub)
} ESP = {.id = {-1, -1, -1, -1, -1}, .id17 = -1};
typedef struct { uint32_t hid, at_ms, f0, f1; uint8_t on; } EscEv;
static struct {
    int on;      // live esaclub
    SRWLOCK lock;
    EscEv ring[ESC_RING]; uint32_t w, r;   // owner: hook threads -> esync tick
    struct { EscEv e; uint64_t recv; int used; } pend[ESC_PEND];  // puppet: from the owner, waiting for our own swing
    uint32_t q_on, q_off, rx_on, rx_off, forced_on, natural_on, forced_off, expired, keyfill, nokey, ring_full, pend_full, nohid, stale;
    uint32_t seen[6];
    uint64_t next_log;
} ESC = {.on = 1};
static double esp_ms(void) {
    static LARGE_INTEGER f;
    LARGE_INTEGER c;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
}

static int esp_slot(uint64_t helper) {  // (worker threads: claim a free row with a CAS on the key)
    for (int i = 0; i < ESP_N; i++) {
        uint64_t k = __atomic_load_n(&ESP.h[i].helper, __ATOMIC_ACQUIRE);
        if (k == helper) return i;
        if (!k) {
            uint64_t z = 0;
            if (__atomic_compare_exchange_n(&ESP.h[i].helper, &z, helper, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) return i;
            if (z == helper) return i;
        }
    }
    return -1;
}
// the map HashId of the helper's enemy (helper+8 = its action node, node+8 = the actor, actor+0x7c8 -> +0x58), cached per row
static uint32_t esp_hid(int i, uint64_t helper) {
    if (ESP.h[i].hid) return ESP.h[i].hid;
    uint64_t node = 0, actor = 0, mo = 0;
    uint32_t hid = 0;
    const SwApi *a = ESP.api;
    if (a->read(helper + 8, &node, 8) && a->read(node + 8, &actor, 8) && a->read(actor + 0x7c8, &mo, 8) && a->read(mo + 0x58, &hid, 4)) ESP.h[i].hid = hid;
    return hid;
}
static uint64_t esp_actor(uint64_t helper) {
    uint64_t node = 0, actor = 0;
    return ESP.api->read(helper + 8, &node, 8) && ESP.api->read(node + 8, &actor, 8) ? actor : 0;
}
static void esc_push(uint32_t hid, int on, double at, uint32_t f0, uint32_t f1) {  // owner side (any thread)
    if (!hid) { __atomic_add_fetch(&ESC.nohid, 1, __ATOMIC_RELAXED); return; }
    AcquireSRWLockExclusive(&ESC.lock);
    if (ESC.w - ESC.r < ESC_RING) {
        EscEv e = {hid, at > 0 ? (uint32_t)at : 0, f0, f1, (uint8_t)(on != 0)};
        ESC.ring[ESC.w++ % ESC_RING] = e;
        if (on) ESC.q_on++; else ESC.q_off++;
    } else ESC.ring_full++;
    ReleaseSRWLockExclusive(&ESC.lock);
}
// puppet side (hook thread): a matured event of kind `on` for hid at swing time `el` ms -> used (1) or not (0)
static int esc_take(uint32_t hid, int on, double el, uint32_t *f0, uint32_t *f1) {
    int got = 0;
    AcquireSRWLockExclusive(&ESC.lock);
    for (int k = 0; k < ESC_PEND; k++)
        if (ESC.pend[k].used && ESC.pend[k].e.hid == hid && ESC.pend[k].e.on == on && el + ESC_EARLY_MS >= (double)ESC.pend[k].e.at_ms) {
            if (f0) *f0 = ESC.pend[k].e.f0, *f1 = ESC.pend[k].e.f1;
            ESC.pend[k].used = 0, got = 1;
            break;
        }
    ReleaseSRWLockExclusive(&ESC.lock);
    return got;
}
// the puppet's swing started: events that came in long before it are from an older swing
static void esc_swing_start(uint32_t hid) {
    uint64_t now = GetTickCount64();
    AcquireSRWLockExclusive(&ESC.lock);
    for (int k = 0; k < ESC_PEND; k++)
        if (ESC.pend[k].used && ESC.pend[k].e.hid == hid && ESC.pend[k].recv + ESC_OLD_MS < now) ESC.pend[k].used = 0, ESC.stale++;
    ReleaseSRWLockExclusive(&ESC.lock);
}
// the puppet's own type-3 hold event (ASList slot 0 bank 0, layer data D+0x940, 32 x 0x20, count D+0xf7) -> key ptr + floats
static int esc_fill_key(uint64_t actor, uint64_t key, uint32_t f0, uint32_t f1) {
    const SwApi *a = ESP.api;
    uint64_t asl = 0, slots = 0, banks = 0, d = 0;
    int ok = 0;
    if (actor && a->read(actor + 0x560, &asl, 8) && asl && a->read(asl + 0xc0, &slots, 8) && slots && a->read(slots + 0x28, &banks, 8) && banks &&
        a->read(banks, &d, 8) && d) {
        uint8_t n = 0;
        if (a->read(d + 0xf7, &n, 1))
            for (int i = 0; i < (n > 32 ? 32 : n) && !ok; i++) {
                uint8_t ent[0x20];
                if (!a->read(d + 0x940 + (uint64_t)i * 0x20, ent, sizeof ent)) break;
                uint16_t type;
                memcpy(&type, ent, 2);
                if (type != 3) continue;
                ok = a->write(key + 8, ent + 0x10, 8) && a->write(key + 0x10, ent + 0x18, 8);  // (name SafeString at +8 -> its char* at +0x10; floats +0x18 / +0x1c)
            }
    }
    if (!ok) a->write(key + 0x10, &f0, 4), a->write(key + 0x14, &f1, 4);
    return ok;
}
static void esp_calc(Cpu *c, int id, void *u) {
    (void)u;
    int i = esp_slot(c->x[0]);
    if (i >= 0) {
        __atomic_add_fetch(&ESP.h[i].calc, 1, __ATOMIC_RELAXED);
        if (ESP.h[i].in_swing) {  // (one helper = one enemy's node: its calls come from one thread at a time)
            double t = esp_ms(), g = t - ESP.h[i].last_calc;
            if (g > ESP.h[i].max_gap) ESP.h[i].max_gap = g;
            ESP.h[i].last_calc = t;
        }
    }
    ESP.api->hook_orig(c, id);
}
static void esp_ask(Cpu *c, int id, void *u) {
    (void)u;
    uint64_t helper = c->x[0], key = c->x[1];
    int i = esp_slot(helper);
    ESP.api->hook_orig(c, id);
    if (i >= 0 && ESC.on && ESP.h[i].in_swing) {  // s178 club sync, puppet side (the owner never has pending events)
        uint32_t hid = esp_hid(i, helper), f0 = 0, f1 = 0;
        double el = esp_ms() - ESP.h[i].t0;
        if (c->x[0] & 1) { if (hid && esc_take(hid, 1, el + 1e6, NULL, NULL)) __atomic_add_fetch(&ESC.natural_on, 1, __ATOMIC_RELAXED); }
        else if (hid && key && esc_take(hid, 1, el, &f0, &f1)) {
            if (esc_fill_key(esp_actor(helper), key, f0, f1)) __atomic_add_fetch(&ESC.keyfill, 1, __ATOMIC_RELAXED);
            else __atomic_add_fetch(&ESC.nokey, 1, __ATOMIC_RELAXED);
            c->x[0] = 1;  // (the game's calc now calls vslot 15 = club ON itself)
            __atomic_add_fetch(&ESC.forced_on, 1, __ATOMIC_RELAXED);
        }
    }
    if (i >= 0) {
        __atomic_add_fetch(&ESP.h[i].ask, 1, __ATOMIC_RELAXED);
        if (c->x[0] & 1) {
            __atomic_add_fetch(&ESP.h[i].yes, 1, __ATOMIC_RELAXED);
            if (ESP.h[i].in_swing && ESP.h[i].yes_at < 0) ESP.h[i].yes_at = esp_ms() - ESP.h[i].t0;
        }
    }
}
static void esp_club(Cpu *c, int id, void *u) {
    (void)u;
    int i = esp_slot(c->x[0]);  // (vslot 15: s0 = 1.0f, x0 = the helper: Ghidra lists the float first)
    if (i >= 0 && ESC.on && ESP.h[i].in_swing) {  // s178: owner side (a puppet's forced ON lands here too: es_step drops it, we do not own it)
        uint32_t f[2] = {0, 0};
        ESP.api->read(c->x[2] + 0x10, f, 8);  // (the key vslot 16 filled: the hold's two floats)
        esc_push(esp_hid(i, c->x[0]), 1, esp_ms() - ESP.h[i].t0, f[0], f[1]);
    }
    if (i >= 0) {
        __atomic_add_fetch(&ESP.h[i].club, 1, __ATOMIC_RELAXED);
        if (ESP.h[i].in_swing && ESP.h[i].on_at < 0) ESP.h[i].on_at = esp_ms() - ESP.h[i].t0;
    }
    ESP.api->hook_orig(c, id);
}
static void esp_prep(Cpu *c, int id, void *u) {  // swing start
    (void)u;
    int i = esp_slot(c->x[0]);
    if (i >= 0) {
        double t = esp_ms();
        ESP.h[i].t0 = t, ESP.h[i].last_calc = t, ESP.h[i].max_gap = 0, ESP.h[i].yes_at = -1, ESP.h[i].on_at = -1;
        ESP.h[i].c0 = ESP.h[i].calc, ESP.h[i].a0 = ESP.h[i].ask, ESP.h[i].y0 = ESP.h[i].yes;
        ESP.h[i].in_swing = 1;
        if (ESC.on) { uint32_t hid = esp_hid(i, c->x[0]); if (hid) esc_swing_start(hid); }
    }
    ESP.api->hook_orig(c, id);
}
static void esp_leave(Cpu *c, int id, void *u) {  // swing end: one record into the ring
    (void)u;
    int i = esp_slot(c->x[0]);
    if (i >= 0 && ESP.h[i].in_swing) {
        EspSwing r = {ESP.h[i].helper, esp_ms() - ESP.h[i].t0, ESP.h[i].yes_at, ESP.h[i].on_at, ESP.h[i].max_gap,
                      ESP.h[i].calc - ESP.h[i].c0, ESP.h[i].ask - ESP.h[i].a0, ESP.h[i].yes - ESP.h[i].y0};
        ESP.h[i].in_swing = 0;
        AcquireSRWLockExclusive(&ESP.ring_lock);
        if (ESP.ring_w - ESP.ring_r < ESP_RING) ESP.ring[ESP.ring_w++ % ESP_RING] = r;
        ReleaseSRWLockExclusive(&ESP.ring_lock);
    }
    ESP.api->hook_orig(c, id);
}

// s178 vslot 17 = "type-3 hold ENDED this tick" (yes -> the calc calls aa5c10 = club OFF)
static void esp_end(Cpu *c, int id, void *u) {
    (void)u;
    uint64_t helper = c->x[0];
    int i = esp_slot(helper);
    ESP.api->hook_orig(c, id);
    if (i < 0 || !ESC.on || !ESP.h[i].in_swing) return;
    uint32_t hid = esp_hid(i, helper);
    double el = esp_ms() - ESP.h[i].t0;
    if (c->x[0] & 1) {
        esc_push(hid, 0, el, 0, 0);                       // owner: its club OFF (a puppet's own OFF is dropped by es_step)
        if (hid) esc_take(hid, 0, el + 1e6, NULL, NULL);  // (puppet: its own OFF came first, the owner's is not needed)
    } else if (hid && esc_take(hid, 0, el, NULL, NULL)) {
        c->x[0] = 1;
        __atomic_add_fetch(&ESC.forced_off, 1, __ATOMIC_RELAXED);
    }
}

static bool esp_bind(void) {
    if (!ESP.api) {
        const SwApi *(*get)(uint32_t) = (const SwApi *(*)(uint32_t))(void *)GetProcAddress(GetModuleHandleA(NULL), SW_API_EXPORT);
        ESP.api = get ? get(1) : NULL;
    }
    return SW_HAS(ESP.api, vt_hook) && SW_HAS(ESP.api, read) && SW_HAS(ESP.api, write);
}
// hooks in while esaprobe or esaclub wants them (both share the rod-attack helper slots: a 2nd vt_hook on a slot would replace the 1st)
static void esp_hooks(int want) {
    const SwApi *a = ESP.api;
    if (!want) {
        if (!ESP.hooked) return;
        for (int k = 0; k < 5; k++) if (ESP.id[k] >= 0) a->vt_unhook(ESP.id[k]), ESP.id[k] = -1;
        if (ESP.id17 >= 0) a->vt_unhook(ESP.id17), ESP.id17 = -1;
        ESP.hooked = 0;
        LOG("[esaprobe] rod-attack helper hooks off\n");
        return;
    }
    if (ESP.hooked) return;
    uint64_t s9 = 0, s8 = 0, s10 = 0;
    if (!a->read(ESP_VT + 9 * 8, &s9, 8) || s9 != ESP_CALC) {
        static int told;
        if (!told++) LOG("[esaprobe] vtable %llx slot 9 = %llx, not %llx: not this game version, rod-attack hooks off\n", (unsigned long long)ESP_VT,
                         (unsigned long long)s9, (unsigned long long)ESP_CALC);
        return;
    }
    memset(ESP.h, 0, sizeof ESP.h);
    ESP.id[0] = a->vt_hook(ESP_VT, 9, esp_calc, NULL, "esaprobe calc");
    ESP.id[1] = a->vt_hook(ESP_VT, 16, esp_ask, NULL, "esaprobe window");
    ESP.id[2] = a->vt_hook(ESP_VT, 15, esp_club, NULL, "esaprobe club on");
    ESP.id17 = a->vt_hook(ESP_VT, 17, esp_end, NULL, "esaclub hold ended");
    if (a->read(ESP_VT + 8 * 8, &s8, 8) && s8 == ESP_PREP) ESP.id[3] = a->vt_hook(ESP_VT, 8, esp_prep, NULL, "esaprobe swing start");
    if (a->read(ESP_VT + 10 * 8, &s10, 8) && s10 == ESP_LEAVE) ESP.id[4] = a->vt_hook(ESP_VT, 10, esp_leave, NULL, "esaprobe swing end");
    ESP.ring_w = ESP.ring_r = 0;
    ESP.hooked = 1;
    LOG("[esaprobe] rod-attack helper hooks on: %d %d %d %d, swing timing %d %d (slot 8 %llx, 10 %llx) on vtable %llx (probe %d, club sync %d)\n", ESP.id[0], ESP.id[1],
        ESP.id[2], ESP.id17, ESP.id[3], ESP.id[4], (unsigned long long)s8, (unsigned long long)s10, (unsigned long long)ESP_VT, ESP.on, ESC.on);
    if (ESC.on && (ESP.id[3] < 0 || ESP.id[4] < 0)) LOG("[esaclub] WARNING: no swing start / end hook (slot 8 / 10 differ): club sync cannot time its events\n");
}
static void esp_set(int on) {
    if (!esp_bind()) { LOG("[esaprobe] no sw_api hooks in this exe\n"); return; }
    ESP.on = on != 0;
    LOG("[esaprobe] %s\n", ESP.on ? "on" : "off");
}
static void esc_set(int on) {
    ESC.on = on != 0;
    LOG("[esaclub] club ON / OFF sync %s\n", ESC.on ? "on (owner sends its swing's club ON / OFF, the copy turns its club on / off at the same point)" : "off");
}
// esync glue: the owner's queued club events (main thread, inside es_step)
static int esc_take_extra(EsEvent *out, int max) {
    int n = 0;
    AcquireSRWLockExclusive(&ESC.lock);
    while (n < max && ESC.r != ESC.w) {
        EscEv e = ESC.ring[ESC.r++ % ESC_RING];
        memset(&out[n], 0, sizeof out[n]);
        out[n].hid = e.hid, out[n].on = e.on, out[n].name = ES_EV_CLUB, out[n].params[0] = e.at_ms, out[n].params[1] = e.f0, out[n].params[2] = e.f1;
        n++;
    }
    ReleaseSRWLockExclusive(&ESC.lock);
    return n;
}
// esync glue: a club event from the owner of a puppet loaded here
static void esc_on_extra(const EsEvent *ev, uint64_t now) {
    if (!ESC.on) return;
    AcquireSRWLockExclusive(&ESC.lock);
    int k = -1;
    for (int j = 0; j < ESC_PEND && k < 0; j++) if (!ESC.pend[j].used) k = j;
    if (k < 0) {  // full: the oldest gives way
        k = 0;
        for (int j = 1; j < ESC_PEND; j++) if (ESC.pend[j].recv < ESC.pend[k].recv) k = j;
        ESC.pend_full++;
    }
    EscEv e = {ev->hid, ev->params[0], ev->params[1], ev->params[2], ev->on};
    ESC.pend[k].e = e, ESC.pend[k].recv = now, ESC.pend[k].used = 1;
    if (ev->on) ESC.rx_on++; else ESC.rx_off++;
    ReleaseSRWLockExclusive(&ESC.lock);
}

// main thread, once per net tick: a line per helper that changed in the last 2 s
static void esp_tick(uint64_t now) {
    static uint64_t next;
    if (esp_bind()) {
        esp_hooks(ESP.on || (ESC.on && g_net_esync));
        NETES.c.take_extra = ESC.on && ESP.hooked ? esc_take_extra : NULL;
        NETES.c.on_extra = esc_on_extra;
    }
    uint64_t t = GetTickCount64();
    AcquireSRWLockExclusive(&ESC.lock);
    for (int k = 0; k < ESC_PEND; k++) if (ESC.pend[k].used && t - ESC.pend[k].recv > ESC_EXPIRE_MS) ESC.pend[k].used = 0, ESC.expired++;
    ReleaseSRWLockExclusive(&ESC.lock);
    if (ESC.on && ESP.hooked && now >= ESC.next_log) {
        ESC.next_log = now + 2000;
        uint32_t v[6] = {ESC.q_on, ESC.q_off, ESC.rx_on, ESC.rx_off, ESC.forced_on, ESC.forced_off};
        if (memcmp(v, ESC.seen, sizeof v)) {
            LOG("[esaclub] owner queued ON %u / OFF %u; copy got ON %u / OFF %u, forced ON %u (key from own event %u, floats only %u), own ON %u, "
                "forced OFF %u; expired %u, older swing %u, no hid %u, full %u/%u t %llu ms\n", v[0], v[1], v[2], v[3], v[4], ESC.keyfill, ESC.nokey,
                ESC.natural_on, v[5], ESC.expired, ESC.stale, ESC.nohid, ESC.ring_full, ESC.pend_full, (unsigned long long)now);
            memcpy(ESC.seen, v, sizeof v);
        }
    }
    if (!ESP.on) return;
    for (;;) {  // (s177: finished swings, as they come)
        EspSwing r;
        AcquireSRWLockExclusive(&ESP.ring_lock);
        bool have = ESP.ring_r != ESP.ring_w;
        if (have) r = ESP.ring[ESP.ring_r++ % ESP_RING];
        ReleaseSRWLockExclusive(&ESP.ring_lock);
        if (!have) break;
        LOG("[esaprobe] swing helper %llx: %.0f ms long, calc %u (biggest gap %.0f ms), asked %u, yes %u at %.0f ms, club ON at %.0f ms (-1 = never) t %llu ms\n",
            (unsigned long long)r.helper, r.len, r.calc, r.max_gap, r.ask, r.yes, r.yes_at, r.on_at, (unsigned long long)now);
    }
    if (now < next) return;
    next = now + 2000;
    for (int i = 0; i < ESP_N; i++) {
        uint64_t h = __atomic_load_n(&ESP.h[i].helper, __ATOMIC_ACQUIRE);
        if (!h) continue;
        uint32_t v[4] = {ESP.h[i].calc, ESP.h[i].ask, ESP.h[i].yes, ESP.h[i].club};
        if (!memcmp(v, ESP.h[i].seen, sizeof v)) continue;
        LOG("[esaprobe] helper %llx (node %llx): calc %u, window asked %u / yes %u, club ON %u (+%u calc, +%u yes, +%u ON in 2 s) t %llu ms\n",
            (unsigned long long)h, (unsigned long long)(h - 0x40), v[0], v[1], v[2], v[3], v[0] - ESP.h[i].seen[0], v[2] - ESP.h[i].seen[2],
            v[3] - ESP.h[i].seen[3], (unsigned long long)now);
        memcpy(ESP.h[i].seen, v, sizeof v);
    }
}
