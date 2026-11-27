// sora_net_gear_draw.h (s289 netgear): a friend's BotW melee weapon drawn in his Sora's hand. Included by sora_net_avatar.h
// (after AV / AVS / av_dot_pathn / av_used; sora_wpn.h and sora_net_gear.h are in by then).
// Display only: one draw actor of the mod's own renderer (sora_draw.c, like his body), never a BotW actor - nothing anyone can
// pick up, hit or save, and no inventory is read or written for it.
//   model  data/gear/<actor>.bin + data/gear/gear_pose.bin (1 bone; tools/sorabuild/netgear/gear_bake.py bakes them at install
//          from the user's own romfs, never shipped); slot k's copy has k + 1 "/./" in its paths like his body (the exe dedupes
//          actors by path), loaded the first time he holds that weapon, kept for the session (the exe never unloads one)
//   when   netgear 1 and his NET_GEAR list says a melee weapon is in his hand, his body is Sora (0) or Roxas (7) on a motion
//          slot (no bone pose, no skate rider), the name is a WPN_TAB weapon Sora swings (not keyblade-marked), its bake is
//          there, and a draw actor may be taken (netgearmax weapon models per session; g_of_reserve / AV_RESERVE stay free)
//   where  his grip bone (228; Roxas' 241-bone bake 229) of the pose drawn this frame + wpn_hand_off for his body: the numbers
//          the local Sora's own BotW weapon uses (linkpos.c wpn_model), sora_net_gear_core.h ngear_rows
//   then   the caller hides his Keyblade; the weapon takes his ghost alpha and casts no blob shadow of its own
//   else   nothing new: his Keyblade as before (NF_WEAPON)
#pragma once
#include "sora_net_gear_core.h"

#define NGD_DIR "D:/SoraBotW/data/gear/"
#define NGD_POSE "D:/SoraBotW/data/gear/gear_pose.bin"
#define NGD_PER_SLOT 12      // weapon names one slot tries in a session (more: not drawn, one line)
#define NGD_RETRY_MS 10000   // a missing bake / a refused actor is tried again this much later

typedef struct {
    char name[NET_GEAR_NAME_MAX + 1];
    const WpnDef *def;
    int id;          // 0 not loaded yet, > 0 the draw actor, -1 given up for the session
    int fails;
    uint64_t retry_at;
    bool said;       // (its first miss / refusal is logged, the retries are not)
} NgdAct;
static struct {
    bool tried;
    void (*shadow)(int, float, float);
    int total;  // weapon models loaded this session, all slots (netgearmax)
    struct { NgdAct a[NGD_PER_SLOT]; int n, shown; float gh; bool full_said; float rows[12]; } s[NET_MAX_PEERS];
    struct { uint32_t drawn, nogrip, refused; } st;
    float pose[POSE_BUF_BONES * 12];
} NGD;

// slot k's cache entry for <name> (lazy load); NULL = not drawn (logged once per name)
static NgdAct *ngd_get(int k, const char *name, uint64_t now) {
    if (!NGD.tried) NGD.tried = true, NGD.shadow = (void (*)(int, float, float))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_draw_actor_shadow");
    NgdAct *e = NULL;
    for (int i = 0; i < NGD.s[k].n && !e; i++)
        if (!strcmp(NGD.s[k].a[i].name, name)) e = &NGD.s[k].a[i];
    if (!e) {
        if (NGD.s[k].n >= NGD_PER_SLOT) {
            if (!NGD.s[k].full_said) NGD.s[k].full_said = true, LOG("[net] gear: slot %d tried %d weapons this session: %s and later ones not drawn\n", k + 1, NGD_PER_SLOT, name);
            return NULL;
        }
        e = &NGD.s[k].a[NGD.s[k].n++];
        memset(e, 0, sizeof *e);
        snprintf(e->name, sizeof e->name, "%s", name);
        e->def = wpn_find(name);
        if (!e->def || e->def->keyblade) {
            e->id = -1;
            LOG("[net] gear: slot %d holds %s: not a melee weapon this build draws, his hand as before\n", k + 1, name);
            return NULL;
        }
    }
    if (e->id) return e->id > 0 ? e : NULL;
    if (e->retry_at && now < e->retry_at) return NULL;
    char mp[300], pp[300], dm[600], dp[600];
    snprintf(mp, sizeof mp, "%s%s.bin", sora_path(NGD_DIR), name);
    snprintf(pp, sizeof pp, "%s", sora_path(NGD_POSE));
    if (GetFileAttributesA(mp) == INVALID_FILE_ATTRIBUTES || GetFileAttributesA(pp) == INVALID_FILE_ATTRIBUTES) {
        e->retry_at = now + NGD_RETRY_MS;
        if (!e->said) e->said = true, LOG("[net] gear: %s or %s missing (the install bakes data/gear): slot %d's %s not drawn (looked for again every %d s)\n", mp, pp, k + 1, name, NGD_RETRY_MS / 1000);
        return NULL;
    }
    int cap = AV.cap ? AV.cap() : 0, used = av_used(), res = g_of_reserve > AV_RESERVE ? g_of_reserve : AV_RESERVE;
    if (!cap || NGD.total >= g_net_gear_max || cap - used <= res) {
        e->retry_at = now + NGD_RETRY_MS, NGD.st.refused++;
        if (!e->said) e->said = true, LOG("[net] gear: slot %d's %s not drawn: %d weapon model(s) loaded (netgearmax %d), %d of %d draw actors in use (%d must stay free)\n", k + 1, name,
                                         NGD.total, g_net_gear_max, used, cap, res);
        return NULL;
    }
    av_dot_pathn(mp, dm, sizeof dm, k + 1), av_dot_pathn(pp, dp, sizeof dp, k + 1);
    uint64_t t = GetTickCount64();
    int a = AV.load(dm, dp);
    if (a <= 0) {
        e->retry_at = now + NGD_RETRY_MS;
        if (++e->fails >= 3) e->id = -1;
        LOG("[net] gear: slot %d's %s: load failed (%s)%s\n", k + 1, name, dm, e->id < 0 ? ", given up for this session" : "");
        return NULL;
    }
    NGD.total++;
    if (AV.bones && AV.bones(a) != 1) {  // (s289 review2 nit 4: gear_bake writes ONE bone and ngd_draw passes 12 floats; another file = no draw)
        if (AV.off) AV.off(a);
        e->id = -1;
        LOG("[net] gear: slot %d's %s: %s has %d bones, not the one-bone weapon model gear_bake writes: given up for this session\n", k + 1, name, dm, AV.bones(a));
        return NULL;
    }
    e->id = a;
    if (NGD.shadow) NGD.shadow(a, 0, 0);  // (strength 0 = no disc: his body has its own)
    LOG("[net] gear: slot %d's %s loaded (%llu ms): actor %d (%d weapon model(s), %d of %d draw actors in use)\n", k + 1, name,
        (unsigned long long)(GetTickCount64() - t), a, NGD.total, av_used(), cap);
    return e;
}

// this frame's weapon for slot k (before his Keyblade is set): id = his body's draw actor, slot / clock = the motion about to be
// drawn. Non-NULL = it is drawn this frame (rows ready); NULL = his hand as before.
static NgdAct *ngd_pick(int k, int body, int id, int slot, float clock, uint64_t now) {
    if (!g_net_gear || (body != 0 && body != 7) || !AV.slot_pose || !AV.world || !AV.bones) return NULL;
    const char *nm = net_gear_hand(k, now);
    if (!nm) return NULL;
    NgdAct *e = ngd_get(k, nm, now);
    if (!e) return NULL;
    int nb = AV.bones(id), grip = nb == 241 ? 229 : 228;  // (sora_draw.c GRIP_BONE_OF)
    float off[8];
    wpn_hand_off(e->def, body == 7, off);
    if (nb <= grip || nb > POSE_BUF_BONES || !AV.slot_pose(id, slot, clock, NGD.pose) || !ngear_rows(NGD.pose + grip * 12, off, g_k, NGD.s[k].rows)) {
        NGD.st.nogrip++;
        return NULL;
    }
    return e;
}

// after his body's pose: the weapon at his hand (g from ngd_pick; NULL = none: a weapon shown last frame goes off)
static void ngd_draw(int k, const NgdAct *g, const float feet[3], float yaw, float gh) {
    int was = NGD.s[k].shown, id = g && g->id > 0 ? g->id : 0;
    if (was > 0 && was != id) AV.off(was);
    if (id) {
        if (id != was || gh != NGD.s[k].gh) AV.ghost(id, gh), NGD.s[k].gh = gh;
        AV.world(id, NGD.s[k].rows, feet, yaw, 0.01f * g_k);
        NGD.st.drawn++;
    }
    NGD.s[k].shown = id;
}

static void ngd_off(int k) {
    if (NGD.s[k].shown > 0 && AV.off) AV.off(NGD.s[k].shown);
    NGD.s[k].shown = 0;
}

static void ngd_diag(void) {
    LOG("[net] gear draw: %d weapon model(s) loaded (netgearmax %d), draw frames %u, no grip %u, refused %u\n", NGD.total, g_net_gear_max, NGD.st.drawn, NGD.st.nogrip,
        NGD.st.refused);
    for (int k = 0; k < NET_MAX_PEERS; k++)
        for (int i = 0; i < NGD.s[k].n; i++) {
            const NgdAct *e = &NGD.s[k].a[i];
            if (e->id > 0) LOG("[net]   slot %d %s: actor %d%s\n", k + 1, e->name, e->id, NGD.s[k].shown == e->id ? " (shown)" : "");
            else LOG("[net]   slot %d %s: %s\n", k + 1, e->name, e->id < 0 ? "given up" : "not loaded (missing bake / no free actor: tried again)");
        }
}
