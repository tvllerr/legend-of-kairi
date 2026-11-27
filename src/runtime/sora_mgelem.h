// sora_mgelem.h: (mgelem, PR #61; s288 port) Sora's Blizzard hits freeze and Thunder hits shock enemies through BotW's own element
// reaction, as an Ice / Shock Arrow does (enemies BotW never freezes / shocks ignore it). Fire already burns them (s59 fire_world).
// Key g_mgelem: compiled default 1 (s288 port: was 0 until the user played it, then "Turn it on"), env SAVAGEWIND_SORA_MGELEM, live "set mgelem 0/1".
// 0 = no call at all (spell_hit is exactly the old one). Per-hit log lines: g_mgelem_log (live "set mgelemlog 0/1", default 1).
// The exe side is linkpos.c linkpos_enemy_element (the react(this, el) call 1.8.2 0x7000a73ec4 / 1.9.0 0x7000b873d4, sora_sites.h
// elem_react; another game version = 0 = refused there). Donald's spells are not changed.
// Co-op (ESYNC): an enemy that is a PUPPET here (another game owns its brain) gets no BotW reaction here, the same rule as the
// s194 netreact KH2 reaction (its owner would not see it, and the puppet's body is moved from the owner's samples). Not sent to
// the owner either (the port adds no net message): friends see the damage, not the ice / shock. A duel opponent is never
// in spell_hit's list (spells reach him through du_spell, sora_duel.h), so a friend is never frozen by this.
// (s289 fix 3a) spell_hit calls mgelem_hit BEFORE ah_hit, and ah_hit is where the ESYNC hit claim is made (net_esync_note_hit ->
// es_note_hit): so the FIRST Blizzard / Thunder on an enemy a friend's game runs still saw his puppet here and sent nothing (only a
// later cast, after that claim, froze / shocked it). Live "set mgelemclaim 1" (compiled default 0 = exactly the old calls): on a
// puppet, mgelem_hit makes that same hit claim FIRST (the same call ah_hit makes, the same hid). A hit that qualifies makes the enemy
// ours before net_esync_note_hit returns (es_claim -> es_assign: owner = me; and the exe is told brain(1) in the same call:
// linkpos_esync_brain -> esx_resolve drops its RootAi hook at once), so no net round trip is needed: the re-test says "ours" and
// this very cast freezes / shocks it. ah_hit's own note right after is then a no-op (owner == me: es_hit_claim_ok NO), its netreact
// question says "local" as for any claiming hit (s195b), so the KH2 reaction (ah_hit_ex) plays here AFTER the BotW one, the same
// order as in single player. Only `net diag`'s "hits noted" counts such a hit twice (once here, once in ah_hit).
// A hit that does NOT qualify stays a puppet hit (no reaction, as before), with the reason in the log line: above all the owner
// still hitting it (HOT: his newest sample says he hit it within ES_HIT_STICKY_MS = 1.5 s) - an enemy a friend keeps hitting can
// never be frozen / shocked by us, the first cast or any later one; also eshit 0, a claim of it refused / yielded lately, owner silent.
// Single player / esync off: mgelem_puppet_ is false at once, the new code never runs (byte-for-byte the old calls, key on or off).
// Needs (sora.c, before this include): KH_SPELLS / SPELL_*, ah_actor_hid (sora_airhold.h), net_react_es / nr_puppet (sora_net_react.h),
// (s289) net_esync_note_hit / net_esync_hit_wanted / es_find / es_hit_claim_ok / es_banned / NETES (sora_net_esync.h glue). A host test
// that includes this file needs sora_net.h (net_glue_test.c does) plus stubs for KH_SPELLS / SPELL_*, ah_actor_hid and the
// GetProcAddress lookup of linkpos_enemy_element.
#pragma once
static int g_mgelem = 1;  /* (s288: Claude tested freeze + shock in game; user: "Turn it on") */
static int g_mgelem_log = 1;
static int g_mgelem_claim = 1;  // (s289 fix 3a) live "set mgelemclaim 0/1": the hit claim first on a puppet (above); 0 = the old way. (s290: passed the two-copy lab, sb/s290/LAB_RESULTS.md; the user: "Flip the passes ON" -> default ON)
static bool mgelem_puppet_(uint64_t actor) {  // this enemy's brain runs on another game (ESYNC on in a room)
    if (!net_react_esync_on()) return false;
    bool local = false;
    int owner = -1, brain = -1;
    uint32_t hid = ah_actor_hid(actor);
    bool found = hid && net_react_es(hid, &local, &owner, &brain);
    return nr_puppet(true, found, local, owner, NET.me, brain);
}
// (s289 fix 3a) why a hit on a puppet did not claim it (the log line only): es_hit_claim_ok's tests, right after the note
static const char *mgelem_noclaim_why_(uint32_t hid) {
    EsEnt *e = hid ? es_find(&NETES.c, hid) : NULL;
    uint64_t now = GetTickCount64();
    if (!e) return "not in the enemy-sync table";
    if (!net_esync_hit_wanted()) return "enemy sync not running (net off)";
    if (!NETES.c.hit_on) return "eshit 0";
    if (es_hit_claim_ok(&NETES.c, e, now) == ES_HC_HOT) return "the owner still hits it (HOT)";
    if (es_banned(e, now)) return "a claim of it was refused / yielded lately";
    return "owner silent or not in the player list";
}
// (s289 fix 3a) mgelemclaim 1 and the enemy is a puppet here: ah_hit's hit claim made HERE first. true = it is ours now (the
// re-test says so) and this cast's reaction goes out; false = still a puppet: nothing, as before. One log line either way.
static bool mgelem_claim_(int kind, uint64_t actor, int el) {
    uint32_t hid = ah_actor_hid(actor);
    bool took = hid && net_esync_note_hit(hid) && !mgelem_puppet_(actor);
    if (!g_mgelem_log) return took;
    if (took)
        LOG("[sora] mgelem: %s hit %llx: hit claim made here first (hid=%08x was another game's, now ours): BotW %s here\n",
            KH_SPELLS[kind].name, (unsigned long long)actor, hid, el == 3 ? "freeze" : "shock");
    else
        LOG("[sora] mgelem: %s hit %llx: puppet here (another game owns it): no BotW %s reaction (no hit claim: %s)\n",
            KH_SPELLS[kind].name, (unsigned long long)actor, el == 3 ? "freeze" : "shock", mgelem_noclaim_why_(hid));
    return took;
}
// spell_hit, right after its damage went out: Blizzard -> BotW ice (3), Thunder -> BotW electric (4)
static void mgelem_hit(int kind, uint64_t actor) {
    if (!g_mgelem || (kind != SPELL_BLIZZARD && kind != SPELL_THUNDER)) return;
    int el = kind == SPELL_BLIZZARD ? 3 : 4;
    if (mgelem_puppet_(actor)) {
        if (g_mgelem_claim) {  // (s289 fix 3a) the hit claim first; not taken = a puppet hit, as below
            if (!mgelem_claim_(kind, actor, el)) return;
        } else {
            if (g_mgelem_log) LOG("[sora] mgelem: %s hit %llx: puppet here (another game owns it): no BotW %s reaction\n", KH_SPELLS[kind].name,
                                  (unsigned long long)actor, el == 3 ? "freeze" : "shock");
            return;
        }
    }
    static int (*elem)(uint64_t, int);
    static bool looked;
    if (!looked) {
        looked = true, elem = (int (*)(uint64_t, int))(void *)GetProcAddress(GetModuleHandleA(NULL), "linkpos_enemy_element");
        if (!elem) LOG("[sora] mgelem: this exe has no linkpos_enemy_element (NEEDS EXE RELINK): no freeze / shock\n");
    }
    int r = elem ? elem(actor, el) : -1;
    if (g_mgelem_log)
        LOG("[sora] mgelem: %s hit %llx -> BotW %s reaction %s\n", KH_SPELLS[kind].name, (unsigned long long)actor,
            el == 3 ? "freeze" : "shock", r > 0 ? "sent" : r == 0 ? "not possible (no reaction object)" : "refused");
}
// (s289 fix 3a) live "set mgelemclaim 0/1" (sora.c's set chain, next to mgelem / mgelemlog); one line when it changes. true = our key
static bool mgelem_live_set(const char *key, int v) {
    if (strcmp(key, "mgelemclaim")) return false;
    if (g_mgelem_claim == (v != 0)) return true;  // (a live rewrite re-applies every line: no new line)
    g_mgelem_claim = v != 0;
    LOG("[sora] live: mgelemclaim %d (%s)\n", g_mgelem_claim,
        g_mgelem_claim ? "a Blizzard / Thunder hit on an enemy another game runs makes the hit claim first: a claimed enemy freezes / shocks on that first cast"
                       : "the old order: the hit claim in ah_hit, after the BotW reaction was skipped on a puppet");
    return true;
}
