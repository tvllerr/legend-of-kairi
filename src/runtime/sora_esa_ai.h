// s181 R2 mode 3 (`set esyncatk 3`): the copy's attack action runs INSIDE RootAi::calc (the game's own AI phase) via the exe's
// SW_TAP_PUPPET_AI, where the exe skips the frozen brain. Why: from the net tick (mode 2) the copy's calc_ took 70-100 ms per call on
// COPY2 in ~15.6 ms steps (s180, GUESS: it waits on the AI phase). Included by sora_esa.h (uses its helpers; esa_frame calls in here).
// Split of the work:
//   net tick (SW_TAP_PUPPET_ATTACK, game thread): writes the owner's wish into the actor's slot (atomics, NO guest call) and answers
//     1 when a local attack runs or is about to start (the exe then does not restart the owner's slot-0 AS)
//   AI tap (any worker, that enemy's RootAi::calc): try-locks the slot (busy = skip this frame), ONE esx_atk_step per game frame,
//     all guest calls (walk, enter_, calc_, leave_) with that worker's Cpu
//   game thread, exe Cpu, slot try-locked (a worker in a slow calc_ is never waited on for long): the sweep (brain back on / body
//     replaced / unloaded / no AI tap any more), the hot-reload unload, the drain timeout
// Slots never move (64, keyed by actor; a full table evicts an idle one). Host test: runtime/test/esa_test.c (single thread, fake lock).

static bool esa_ai_supported(const EsaCtx *x) { return SW_HAS(x->api, taps_called) && ((x->api->taps_called >> SW_TAP_PUPPET_AI) & 1u); }
static EsaAiSlot *esa_ai_by_actor(EsaCtx *x, uint64_t actor) {
    if (!actor) return NULL;
    for (int i = 0; i < ESA_AI_MAX; i++)
        if (__atomic_load_n(&x->ai[i].actor, __ATOMIC_ACQUIRE) == actor) return &x->ai[i];
    return NULL;
}
static EsaAiSlot *esa_ai_by_hid(EsaCtx *x, uint32_t hid) {
    for (int i = 0; i < ESA_AI_MAX; i++)
        if (x->ai[i].actor && x->ai[i].hid == hid) return &x->ai[i];
    return NULL;
}
// (caller holds s->lk exclusively) a fresh slot for hid / actor; the actor is published last
static void esa_ai_init(EsaAiSlot *s, uint32_t hid, uint64_t actor, uint64_t now) {
    memset(&s->p, 0, sizeof s->p);
    s->p.hid = hid, s->p.actor = actor;
    s->hid = hid, s->tgt = 0, s->running = 0, s->last_step = 0;
    __atomic_store_n(&s->tgt_frame, now, __ATOMIC_RELEASE);
    __atomic_store_n(&s->actor, actor, __ATOMIC_RELEASE);
}
// (caller holds s->lk exclusively) the slot goes back to free; a running attack is forgotten, never leave_'d (its body is gone / reused)
static void esa_ai_free(EsaCtx *x, EsaAiSlot *s) {
    if (s->p.atk_node) esa_add(&x->n.forgot);
    s->p.atk_node = 0, s->running = 0, s->hid = 0;
    __atomic_store_n(&s->actor, 0, __ATOMIC_RELEASE);
}
// s181 review 1: an actor address lives in ONE slot only (a new hid loaded at an old hid's address would else get a 2nd slot and the
// AI tap would step the old one). false = an old slot is busy: try again next tick.
static bool esa_ai_forget_actor(EsaCtx *x, uint64_t actor, const EsaAiSlot *keep) {
    for (int i = 0; i < ESA_AI_MAX; i++) {
        EsaAiSlot *s = &x->ai[i];
        if (s == keep || s->actor != actor) continue;
        if (!esa_lk_tryx(&s->lk)) return false;
        if (s->actor == actor) esa_ai_free(x, s), esa_add(&x->an.replaced);
        esa_lk_xu(&s->lk);
    }
    return true;
}
// net tick only (one thread): a free slot, else the idle one with the oldest wish
static EsaAiSlot *esa_ai_alloc(EsaCtx *x, uint32_t hid, uint64_t actor, uint64_t now) {
    EsaAiSlot *best = NULL;
    for (int i = 0; i < ESA_AI_MAX; i++) {
        EsaAiSlot *s = &x->ai[i];
        if (!s->actor) { best = s; break; }
        if (!s->running && (!best || s->tgt_frame < best->tgt_frame)) best = s;
    }
    if (!best || !esa_lk_tryx(&best->lk)) return NULL;
    if (best->running) { esa_lk_xu(&best->lk); return NULL; }  // (a worker started one between the look and the lock)
    esa_ai_init(best, hid, actor, now);
    esa_lk_xu(&best->lk);
    return best;
}
// the AI tap's walk: its own buffer (several workers walk at once)
static void esa_ai_walk(EsaCtx *x, Cpu *c, uint64_t actor, EsxPupEnt *p) {
    SwAiNode *buf = (SwAiNode *)malloc(sizeof(SwAiNode) * ESA_WALK_MAX);
    if (!buf) { esa_add(&x->an.walk_oom); return; }  // (next frame tries again)
    EsaRes t0 = esa_res();
    int n = SW_HAS(x->api, ai_walk) ? x->api->ai_walk(c, actor, buf, ESA_WALK_MAX) : 0;
    p->atk_walked = 1;
    for (int i = 0; i < n; i++)
        if (esa_vt_is(x, buf[i].vt)) esx_pup_atk_put(p, buf[i].idx, buf[i].node);
    free(buf);
    esa_add(&x->n.walk);
    __atomic_fetch_add(&x->n.walk_nodes, (uint64_t)(n > 0 ? n : 0), __ATOMIC_RELAXED);
    esa_slow("walk (ai)", p->hid, 0, t0);
    if (x->log) LOG("[linkpos] esync attack walk of %llx: %d attack nodes in %d distinct nodes (dll, ai phase)\n", (unsigned long long)actor, p->natk, n);
}

// (caller holds s->lk exclusively; worker thread, c = its Cpu) one step per game frame
static void esa_ai_step(EsaCtx *x, Cpu *c, EsaAiSlot *s) {
    uint64_t now = x->api->frame_id();
    if (s->last_step == now) return;  // (RootAi::calc twice in one frame: one step)
    s->last_step = now;
    esa_add(&x->an.steps);
    EsxPupEnt *p = &s->p;
    uint64_t tg = s->tgt, tf = __atomic_load_n(&s->tgt_frame, __ATOMIC_ACQUIRE);
    uint16_t act = (uint16_t)(tg & 0xffff);
    uint8_t chg0 = (uint8_t)((tg >> 16) & 0xff);
    int alive = (int)((tg >> 24) & 1);
    if (now - tf > ESA_STALE) {  // (the net tick stopped updating it: like mode 2's stale rule; a body back at this address walks again)
        if (p->atk_node) esa_end(x, c, p, chg0, "no puppet update");
        p->atk_walked = 0, p->natk = 0;
        s->running = 0;
        return;
    }
    int on = x->mode == 3 && x->pending < 0 && !x->ai_dying;
    if (!on) alive = 0;  // (draining: leave what runs)
    if (on && act && !p->atk_walked) esa_ai_walk(x, c, s->actor, p);
    uint64_t want = on && act ? esx_pup_atk_node(p, act) : 0;
    int step = esx_atk_step(p, act, want != 0, chg0, now, ESA_CAP, ESA_GAP, alive);
    int capped = p->atk_node && now - p->atk_frame > ESA_CAP;
    p->atk_seen = now;
    esa_apply(x, c, p, s->hid, s->actor, act, want, step, capped, chg0, alive, now);
    s->running = p->atk_node != 0;
}

// SW_TAP_PUPPET_AI: the exe calls it from RootAi::calc of a frozen puppet (any worker)
static void esa_ai_tap(EsaCtx *x, Cpu *c, uint64_t actor) {
    if (!c || x->ai_dying || !x->api) return;
    esa_add(&x->an.taps);
    EsaAiSlot *s = esa_ai_by_actor(x, actor);
    if (!s) { esa_add(&x->an.noslot); return; }
    if (!esa_lk_tryx(&s->lk)) { esa_add(&x->an.busy); return; }
    if (!x->ai_dying && s->actor == actor) esa_ai_step(x, c, s);
    esa_lk_xu(&s->lk);
}
static void esa_ai_tap_fn(Cpu *c, uint64_t actor, uint64_t root_ai) { (void)root_ai; esa_ai_tap(&g_esa, c, actor); }

// the net tick in mode 3 (game thread): no guest calls
static int esa_ai_net(EsaCtx *x, uint32_t hid, uint64_t actor, const NetEsEnt *t, int alive) {
    uint64_t now = x->api->frame_id();
    EsaAiSlot *s = esa_ai_by_hid(x, hid);
    if (!esa_ai_forget_actor(x, actor, s)) { esa_add(&x->an.net_busy); return 0; }
    if (s && s->actor != actor) {  // (the body was replaced: its nodes went with it; never leave_ them)
        if (!esa_lk_tryx(&s->lk)) { esa_add(&x->an.net_busy); return 0; }
        if (s->p.atk_node) esa_add(&x->n.forgot);
        esa_ai_init(s, hid, actor, now);
        esa_lk_xu(&s->lk);
        esa_add(&x->an.replaced);
    }
    if (!s) {
        if (x->pending >= 0) return 0;  // (leaving mode 3: no new slots)
        if (!(s = esa_ai_alloc(x, hid, actor, now))) { esa_add(&x->an.full); return 0; }
    }
    s->tgt = (uint64_t)t->action | (uint64_t)t->anim_chg[0] << 16 | (uint64_t)(alive != 0) << 24;
    __atomic_store_n(&s->tgt_frame, now, __ATOMIC_RELEASE);
    int ret = s->running;
    if (!ret && x->pending < 0 && t->action && esa_lk_trys(&s->lk)) {
        // would the AI tap ENTER now? then the exe must not restart the owner's slot-0 AS (the attack's enter_ starts its own)
        uint64_t w = esx_pup_atk_node(&s->p, t->action);
        int st = esx_atk_step(&s->p, t->action, w != 0, t->anim_chg[0], now, ESA_CAP, ESA_GAP, alive);
        esa_lk_su(&s->lk);
        ret = st == ESX_ATK_STEP_ENTER || st == ESX_ATK_STEP_SWAP;
        if (ret) esa_add(&x->an.predict);
    }
    return ret;
}

// game thread, inside linkpos_frame (every net tick): attacks no AI tap will end any more
static void esa_ai_sweep(EsaCtx *x) {
    const SwApi *api = x->api;
    if (!api || !SW_HAS(api, esync_actor) || !SW_HAS(api, frame_id) || !SW_HAS(api, in_frame) || !api->in_frame()) return;
    uint64_t now = api->frame_id();
    for (int i = 0; i < ESA_AI_MAX; i++) {
        EsaAiSlot *s = &x->ai[i];
        if (!s->actor) continue;
        uint64_t a = 0;
        int pup = 0, forget = 0;
        const char *why = NULL;
        if (!api->esync_actor(s->hid, &a, &pup)) {
            if (now - s->tgt_frame <= 60) continue;  // (like mode 2: a body we cannot find is only forgotten, never leave_'d)
            forget = 1;
        } else if (a != s->actor) forget = 1;
        else if (!s->running) continue;
        else if (!pup) why = "puppet gone / brain back";
        else if (now - s->tgt_frame > ESA_STALE && now - s->last_step > ESA_STALE) why = "no puppet update, no ai tap";
        else continue;
        if (!esa_lk_tryx(&s->lk)) continue;  // (a worker is in it: next tick)
        if (forget) {  // (s181 review 1: the slot is freed too, so its address can never shadow a new body's slot)
            if (s->p.atk_node) esa_add(&x->an.sweep_forgot);
            esa_ai_free(x, s);
        } else if (s->p.atk_node) esa_end(x, NULL, &s->p, 0, why), esa_add(&x->an.sweep_left);
        s->running = s->p.atk_node != 0;
        esa_lk_xu(&s->lk);
    }
}
// game thread: try the lock for up to ms (a worker may sit in a slow calc_; never wait forever: it may wait on this thread)
static bool esa_ai_lock_wait(EsaAiSlot *s, int ms) {
    for (int i = 0;; i++) {
        if (esa_lk_tryx(&s->lk)) return true;
        if (i >= ms) return false;
        esa_sleep_ms(1);
    }
}
// s181 review 2: leaving mode 3 commits only when every slot was locked once (no worker inside enter_ / calc_ with an old "on") and
// none runs. One try per slot (game thread never waits here): busy = not quiet, next tick tries again.
static bool esa_ai_quiet(EsaCtx *x) {
    for (int i = 0; i < ESA_AI_MAX; i++) {
        EsaAiSlot *s = &x->ai[i];
        if (!s->actor) continue;
        if (!esa_lk_tryx(&s->lk)) return false;
        int r = s->running;
        esa_lk_xu(&s->lk);
        if (r) return false;
    }
    return true;
}
// drop every running mode-3 attack without leave_ (the drain timed out)
static void esa_ai_drop(EsaCtx *x) {
    for (int i = 0; i < ESA_AI_MAX; i++) {
        EsaAiSlot *s = &x->ai[i];
        if (!s->actor || !s->running || !esa_ai_lock_wait(s, 50)) continue;
        s->p.atk_node = 0, s->running = 0;
        esa_lk_xu(&s->lk);
    }
}
// this DLL copy goes away (hot reload): no more AI taps into it, then leave what runs with the exe Cpu
static void esa_ai_unload(EsaCtx *x) {
    x->ai_dying = 1;
    if (SW_HAS(x->api, tap_set)) x->api->tap_set(SW_TAP_PUPPET_AI, NULL);
    bool inf = SW_HAS(x->api, in_frame) && x->api->in_frame();
    for (int i = 0; i < ESA_AI_MAX; i++) {
        EsaAiSlot *s = &x->ai[i];
        if (!s->actor) continue;
        if (!esa_ai_lock_wait(s, 200)) { LOG("[esa] unload: slot hid=%08x busy for 200 ms, attack forgotten\n", s->hid); continue; }
        if (s->p.atk_node) {
            if (inf) esa_end(x, NULL, &s->p, 0, "dll unload");
            else s->p.atk_node = 0;
        }
        s->running = 0;
        esa_lk_xu(&s->lk);
    }
}
