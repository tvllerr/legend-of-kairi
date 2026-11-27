// sora_net_world.h (s290 networld): the room's world flags. User (2026-11-26, his answers to the s289 plan, sb/s289/MORNING.md
// "HIS ANSWERS" 4): "build networld the way PR #294 does it": on a join the UNION of both saves (each game sends its flag bits, each
// game sets what it lacks; a flag is never unset); a flag that carries over gives its item too (Korok seed / Spirit Orb / rune /
// champion ability / heart container through the pouch, the PR's way; the Korok's own cutscene when it is within 60 m); the shop
// sold-outs + quest steps (the PR's online_mod.py: Bootup.pack gamedata + QuestProduct); and the main session's guard (his OK):
// each player's save is copied before the first union in a room.
// Ported after Savage Wind PR #294 "Online mode" by byluki, MIT (runtime/online_actors.c quests_update / give_item / give_heart /
// korok_event / slate_group / slate_rune, tools/online_mod.py's quests.txt list: its families and rules are theirs). The list build
// at runtime, the hostless wire, the snapshots + deltas, the Blood Moon bookkeeping without a host and the save copy are ours.
// MULTIPLAYER IS MELTY ONLY: NET_WORLD rides the room's reliable events (sora_melty.cpp's default case); on UDP it does nothing.
//
// Today (VERIFIED from the code, s290): every game keeps its own world. A chest opened, a Korok found, a shrine cleared, an item
// taken in one game stays as it was in the others (netloot (s289) shares only live drops and map items taken while together).
//
// THE LIST (no install step): the PR's install tool wrote quests.txt from the game's own Pack/Bootup.pack (GameData/gamedata.ssarc:
// the bool_data / revival_bool_data .bgdata BYMLs) and Pack/TitleBG.pack (Quest/QuestProduct.sbquestpack). This DLL reads the
// same two files itself, from Savage Wind's own extracted RomFS (the exe's g_romfs_dir), on a worker thread the first time the key
// is on, and applies the same rules (nwd_family): every game with the same 1.9.0 data builds the same list, sorted by hash. Its
// fingerprint (fp) goes in every packet: a friend whose list differs is refused whole (a log line says so).
//   families (NWDF_*, `set networldfam <bits>`, default NWDF_DEFAULT = all but NWDF_MORE):
//     STORY    the Great Plateau's slate (IsGet_PortableUnit ...), the towers (MapTower_*), the runes + slate upgrades (PR STORY_FLAGS)
//     SHRINE   Open_ / Enter_ / Clear_DungeonNNN (a Clear gives the Spirit Orb)
//     BEAST    the Divine Beasts / Ganon: Clear_Remains*, IsGet_Obj_HeroSoul_* (the ability), Get_HeartUtuwa_* (a heart), GameClear ...
//     KOROK    MainField_Npc_HiddenKorok* (the seed, or its own cutscene within 60 m)
//     CHEST    *_TBox_* (opened in one game = opened in the others; what it held is the opener's)
//     WORLD    revival flags <MainField|AocField|MainFieldDungeon|CDungeon>_<Item|Obj|Weapon|Barrel|Kibako>_<HashId>: items taken,
//              ores / objects broken, barrels / crates (gone in the others the next time they load that place; a Blood Moon brings
//              them back). (PR: any first word; ours: only the 4 maps, so the Compendium's IsNewPictureBook_Item_* stays out)
//     QUEST    QuestProduct's Main / Sub / Mini steps' NextFlag + their bool dependency flags
//     SHOP     *_SoldOut, GemShop_Open, Mans_Clothes_Shop (the PR's)
//     MORE     the PR's other shared flags (NPC talks / errands, LinkTag switches, walls, pots, beehives ...): OFF by default
//   Left out (the plan's rule): the Compendium, items got (IsGet_* other than the above), cutscenes seen (IsPlayed_* other than the
//   above), enemy dead flags (killsync / netloot own those).
//
// THE UNION (no host): every 2 s each game reads its own flags (GameDataMgr's bool buffer, read only, through the exe's sw_api).
// It sends its whole set as PAGEs on a join / an ask / after a Blood Moon here, and SETs for flags that became set by its own
// player since. Each game keeps each friend's set (his last pages + his SETs since) and sets, the game's own way (EventFlagONAction's
// path: loot's loot_gdt_handle + loot_gdt_set_bool rows), the flags of the union it lacks: at most NWD_APPLY_MAX a pass, never while
// paused, loading or in a cutscene. A flag is never unset by us. What comes with a flag (PR quests_update): the slate into the key
// items (only once the finder's Demo700_0 is over: IsPlayed_Demo700_0 in the union), a rune / upgrade item, the paraglider
// (FindDungeon_Finish), a Korok seed (or its Demo017_0 within 60 m), the Spirit Orb (Clear_Dungeon*), the champion's ability
// (IsGet_Obj_HeroSoul_*), a heart container (Get_HeartUtuwa_*). An item that would not fit the pouch: the flag waits (PR: lost).
//
// THE BLOOD MOON RULE (PR "a Blood Moon here"): after one here (sora_world_bloodmoon_count + 1, or 64+ of our revival flags clear in
// one pass), the revival flags the friends' sets still have are HELD (not set here again): they came from before it. One is set
// again only after it left every friend's set (their own Blood Moon: their pages after it) and a friend takes it anew.
//
// THE SAVE GUARD (main session, his OK): before the first union in a room, this game copies the save slot it is on (the newest
// game_data.sav read or written: the exe's fs_save_event ring; old exe: the newest by file time) to
//   <the folder that holds the save root>/sora-networld-backups/<YYYY-MM-DD_HH-MM-SS>_slot<N>/   (Default profile: <data root>/...)
// once per room join, never into an existing folder (another name then), and logs the path. The union waits until the copy is done
// and is refused for that room if it failed. (A union marks shrines / towers / Divine Beasts done for good once the game saves.)
// (s290 revfix) Per slot, not only per room join: a read of another save than the copied one (Load from the pause menu) makes the
// union wait until that slot is copied too; with the exe's event ring the slot is never guessed by file time (no read yet = wait);
// a save write of the slot during its copy = copied again into a new folder (bounded); a failed copy is tried again 30 s later; a
// save unchanged since this code's newest copy of that slot makes no new folder (nothing is ever deleted). See "the save guard".
//
// Live keys (DLL side, sora_net.h net_live_set):
//   set networld 0|1      compiled default 0 = today exactly: nothing read, built, sent or set; a received NET_WORLD is dropped
//                         and counted. 1 = the rules above in a Melty room with a live friend.
//   set networldlog 0|1   one line per event (a friend's set, each flag set here and what came with it, a held one, a refusal)
//   set networldfam <n>   the families shared (NWDF_* bits, default 255 = all but MORE (256)); both ways (sent and taken)
//   "net diag" gives one [networld] line either way.
// Exe side: the (s290 networld) rows at the end of sora_sites.h (pouch get / can-get, the special get (hearts), the event start +
// its metadata init / name / free, the Korok's event manager) + s289 loot's loot_gdt_handle / loot_gdt_set_bool need an EXE RELINK;
// an exe without them says so in the key's line and networld does nothing (no flag without its item). VERIFIED with Ghidra on the
// 1.9.0 main: each is the function / pointer the game's own code uses for it (EventIncreasePorchItemAction 0x7001187e3c calls
// 0x700120af80 / 0x700120af98 / 0x7001211520; the Korok's trigger 0x70008f682c reads 0x7001e3a638 and calls 0x7001112508 /
// 0x7001112794 / 0x700110d500 / 0x70011128b4; EventFlagONAction 0x7001187758 = loot's two rows). Everything else is this DLL.
//
// Pure core first (host test runtime/test/networld_test.c); the glue (NET, LOG, sw_api) under NET_WORLD_GLUE.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#ifdef _WIN32
#include <direct.h>
#endif

#include "sora_net_proto.h"
#include "sora_net_loot.h"  // (lt_crc32, lt_gdt_find: loot's GameData lookup, read only)

enum { NWDF_STORY = 1, NWDF_SHRINE = 2, NWDF_BEAST = 4, NWDF_KOROK = 8, NWDF_CHEST = 16, NWDF_WORLD = 32, NWDF_QUEST = 64, NWDF_SHOP = 128, NWDF_MORE = 256 };
#define NWDF_COUNT 9
#define NWDF_ALL 511
#define NWDF_DEFAULT 255
static const char *const NWDF_TXT[NWDF_COUNT] = {"story", "shrine", "beast", "korok", "chest", "world", "quest", "shop", "more"};
#define NWD_PEERS 4           // friends' sets kept (>= NET_MAX_PEERS, checked in the glue)
#define NWD_NAME_MAX 63       // a flag name longer than this is left out (the longest kept is 49)
#define NWD_MAPS_N 4

// ---------------------------------------------------------------- little readers (the Switch files are little endian)
static inline uint16_t nwd_le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static inline uint32_t nwd_le24(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16; }
static inline uint32_t nwd_le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static inline uint32_t nwd_be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | (uint32_t)p[3]; }
static inline uint32_t nwd_crc32_upd(uint32_t c, const uint8_t *d, size_t n) {  // (crc32, as lt_crc32: c starts at 0)
    uint32_t h = ~c;
    for (size_t i = 0; i < n; i++) {
        h ^= d[i];
        for (int b = 0; b < 8; b++) h = h >> 1 ^ (0xEDB88320u & (0u - (h & 1)));
    }
    return ~h;
}

// Yaz0: -> the unpacked size (0 = not Yaz0)
static inline size_t nwd_yaz0_size(const uint8_t *s, size_t n) { return s && n >= 16 && !memcmp(s, "Yaz0", 4) ? nwd_be32(s + 4) : 0; }
// -> true = d[0..dn) filled; false = cut short / a back reference before the start
static inline bool nwd_yaz0_unpack(const uint8_t *s, size_t n, uint8_t *d, size_t dn) {
    size_t si = 16, di = 0;
    if (!s || !d || n < 16) return false;
    while (di < dn) {
        if (si >= n) return false;
        uint8_t code = s[si++];
        for (int b = 0; b < 8 && di < dn; b++, code = (uint8_t)(code << 1)) {
            if (code & 0x80) {
                if (si >= n) return false;
                d[di++] = s[si++];
                continue;
            }
            if (si + 2 > n) return false;
            uint8_t b1 = s[si], b2 = s[si + 1];
            si += 2;
            size_t dist = ((size_t)(b1 & 0xf) << 8 | b2) + 1, len = b1 >> 4;
            if (!len) {
                if (si >= n) return false;
                len = (size_t)s[si++] + 0x12;
            } else
                len += 2;
            if (dist > di) return false;
            for (size_t k = 0; k < len && di < dn; k++, di++) d[di] = d[di - dist];
        }
    }
    return true;
}

// SARC (little endian only): the header + its file table (+ names) must be in d[0..n); a file's bytes are at data_off + beg
typedef struct { const uint8_t *d; size_t n; uint32_t data_off; uint16_t nodes; size_t sfat, names; } NwdSarc;
static inline bool nwd_sarc_open(NwdSarc *s, const uint8_t *d, size_t n) {
    memset(s, 0, sizeof *s);
    if (!d || n < 0x20 || memcmp(d, "SARC", 4) || nwd_le16(d + 4) != 0x14 || d[6] != 0xFF || d[7] != 0xFE) return false;
    s->d = d, s->n = n, s->data_off = nwd_le32(d + 12);
    size_t f = 0x14;
    if (f + 12 > n || memcmp(d + f, "SFAT", 4)) return false;
    s->nodes = nwd_le16(d + f + 6), s->sfat = f + 12;
    size_t t = s->sfat + 16 * (size_t)s->nodes;
    if (t + 8 > n || memcmp(d + t, "SFNT", 4)) return false;
    s->names = t + 8;
    return true;
}
// node i -> its name (NULL: none / not in the buffer), *beg / *end its bytes after data_off
static inline const char *nwd_sarc_node(const NwdSarc *s, int i, uint32_t *beg, uint32_t *end) {
    if (!s || !s->d || i < 0 || i >= s->nodes) return NULL;
    const uint8_t *e = s->d + s->sfat + 16 * (size_t)i;
    uint32_t attr = nwd_le32(e + 4);
    *beg = nwd_le32(e + 8), *end = nwd_le32(e + 12);
    if (*end < *beg || !(attr >> 24)) return NULL;
    size_t no = s->names + 4 * (size_t)(attr & 0xffffff);
    if (no >= s->n || !memchr(s->d + no, 0, s->n - no)) return NULL;
    return (const char *)s->d + no;
}
// the file called `name` -> true, *beg / *end its bytes after data_off
static inline bool nwd_sarc_find(const NwdSarc *s, const char *name, uint32_t *beg, uint32_t *end) {
    for (int i = 0; s && i < s->nodes; i++) {
        const char *nm = nwd_sarc_node(s, i, beg, end);
        if (nm && !strcmp(nm, name)) return true;
    }
    return false;
}

// BYML v2+ (little endian "YB"): a node = (type, value); containers are offsets in d
enum { NWDB_STR = 0xA0, NWDB_ARR = 0xC0, NWDB_HASH = 0xC1, NWDB_STRTAB = 0xC2 };
typedef struct { const uint8_t *d; size_t n; uint32_t keys, strs; uint8_t root_t; uint32_t root; } NwdByml;
static inline bool nwd_byml_tab_ok(const NwdByml *b, uint32_t o) {
    return o && o + 4 <= b->n && b->d[o] == NWDB_STRTAB && o + 4 + 4 * ((size_t)nwd_le24(b->d + o + 1) + 1) <= b->n;
}
static inline bool nwd_byml_open(NwdByml *b, const uint8_t *d, size_t n) {
    memset(b, 0, sizeof *b);
    if (!d || n < 16 || d[0] != 'Y' || d[1] != 'B' || nwd_le16(d + 2) < 2 || nwd_le16(d + 2) > 7) return false;
    b->d = d, b->n = n, b->keys = nwd_le32(d + 4), b->strs = nwd_le32(d + 8), b->root = nwd_le32(d + 12);
    if (!nwd_byml_tab_ok(b, b->keys) || (b->strs && !nwd_byml_tab_ok(b, b->strs)) || b->root + 4 > n) return false;
    b->root_t = d[b->root];
    return b->root_t == NWDB_ARR || b->root_t == NWDB_HASH;
}
static inline const char *nwd_byml_tab_str(const NwdByml *b, uint32_t tab, uint32_t i) {
    if (!tab || i >= nwd_le24(b->d + tab + 1)) return NULL;
    uint32_t o = nwd_le32(b->d + tab + 4 + 4 * (size_t)i);
    if ((size_t)tab + o >= b->n || !memchr(b->d + tab + o, 0, b->n - tab - o)) return NULL;
    return (const char *)b->d + tab + o;
}
// a container's count (0 = not that type / out of the buffer)
static inline uint32_t nwd_byml_count(const NwdByml *b, uint8_t t, uint32_t o, uint8_t want) {
    if (t != want || (size_t)o + 4 > b->n || b->d[o] != want) return 0;
    uint32_t c = nwd_le24(b->d + o + 1);
    size_t need = want == NWDB_HASH ? 4 + 8 * (size_t)c : 4 + (((size_t)c + 3) & ~(size_t)3) + 4 * (size_t)c;
    return (size_t)o + need <= b->n ? c : 0;
}
static inline bool nwd_byml_arr_at(const NwdByml *b, uint32_t o, uint32_t i, uint8_t *t, uint32_t *v) {
    uint32_t c = nwd_le24(b->d + o + 1);
    if (i >= c) return false;
    *t = b->d[o + 4 + i], *v = nwd_le32(b->d + o + 4 + ((c + 3) & ~3u) + 4 * (size_t)i);
    return true;
}
static inline bool nwd_byml_get(const NwdByml *b, uint8_t t, uint32_t o, const char *key, uint8_t *vt, uint32_t *v) {
    uint32_t c = nwd_byml_count(b, t, o, NWDB_HASH);
    for (uint32_t i = 0; i < c; i++) {
        const uint8_t *e = b->d + o + 4 + 8 * (size_t)i;
        const char *k = nwd_byml_tab_str(b, b->keys, nwd_le24(e));
        if (k && !strcmp(k, key)) { *vt = e[3], *v = nwd_le32(e + 4); return true; }
    }
    return false;
}
static inline const char *nwd_byml_get_str(const NwdByml *b, uint8_t t, uint32_t o, const char *key) {
    uint8_t vt = 0;
    uint32_t v = 0;
    return nwd_byml_get(b, t, o, key, &vt, &v) && vt == NWDB_STR ? nwd_byml_tab_str(b, b->strs, v) : NULL;
}

// ---------------------------------------------------------------- the families (after the PR's online_mod.py rules)
static inline bool nwd_pre(const char *s, const char *p) { return !strncmp(s, p, strlen(p)); }
static inline bool nwd_digits(const char *s) {
    if (!*s) return false;
    for (; *s; s++)
        if (*s < '0' || *s > '9') return false;
    return true;
}
// the PR's "^<p>\w*_" (extra 0) / "^<p>\w+_" (extra 1): starts with p, and a '_' comes at least `extra` letters after it
static inline bool nwd_pre_us(const char *s, const char *p, size_t extra) {
    size_t l = strlen(p);
    if (strncmp(s, p, l) || strlen(s) < l + extra) return false;
    return strchr(s + l + extra, '_') != NULL;
}
static const char *const NWD_STORY[] = {
    "IsGet_PortableUnit", "PortableUnit_PowerOn", "OP_DRC", "Open_FirstDoor", "Open_Door", "IsPlayed_Demo700_0", "IsPlayed_Demo701_0",
    "IsPlayed_Demo164_0", "IsPlayed_Demo166_0", "MapTower_DemoFirst", "MapTower_07_Demo", "IsGet_Obj_RemoteBomb",
    "IsGet_Obj_RemoteBombCube", "IsGet_Obj_RemoteBombLv2", "IsGet_Obj_RemoteBombCube2", "IsGet_Obj_Magnetglove", "IsGet_Obj_StopTimer",
    "IsGet_Obj_StopTimerLv2", "IsGet_Obj_IceMaker", "IsGet_Obj_Camera", "IsGet_Obj_PictureBook", "IsGet_Obj_SheikSensor",
    "IsStart_Obj_SheikSensor", "IsGet_Obj_SheikSensorLv2", "IsGet_Obj_SheikPadLv2", "Hateno_SheikPad_PowerUp_Finish",
    "IsPlayed_Demo032_0", "IsGet_Obj_Motorcycle"};
static inline bool nwd_is_story(const char *n) {
    if (nwd_pre(n, "MapTower_") && strlen(n) == 11 && nwd_digits(n + 9)) {  // (MapTower_01 .. MapTower_15)
        int t = atoi(n + 9);
        return t >= 1 && t <= 15;
    }
    for (size_t i = 0; i < sizeof NWD_STORY / sizeof *NWD_STORY; i++)
        if (!strcmp(n, NWD_STORY[i])) return true;
    return false;
}
static inline bool nwd_is_shrine(const char *n) {
    const char *r = nwd_pre(n, "Clear_Dungeon") ? n + 13 : nwd_pre(n, "Open_Dungeon") ? n + 12 : nwd_pre(n, "Enter_Dungeon") ? n + 13 : NULL;
    return r && nwd_digits(r);
}
static const char *const NWD_ELEM[4] = {"Wind", "Water", "Fire", "Electric"};
static inline bool nwd_is_beast(const char *n) {
    if (!strcmp(n, "GameClear")) return true;
    char b[64];
    for (int i = 0; i < 4; i++) {
        snprintf(b, sizeof b, "IsHyruleCastlePGanon%sDead", NWD_ELEM[i]);
        if (!strcmp(n, b)) return true;
        snprintf(b, sizeof b, "Die_PGanon%s", NWD_ELEM[i]);
        if (!strcmp(n, b)) return true;
        snprintf(b, sizeof b, "Get_HeartUtuwa_%s", NWD_ELEM[i]);
        if (!strcmp(n, b)) return true;
        snprintf(b, sizeof b, "IsGet_App_%sRelic", NWD_ELEM[i]);
        if (!strcmp(n, b)) return true;
        snprintf(b, sizeof b, "%s_Relic_", NWD_ELEM[i]);
        if (nwd_pre(n, b) && n[strlen(b)]) return true;
    }
    static const char *const souls[4] = {"Rito", "Zora", "Goron", "Gerudo"};
    for (int i = 0; i < 4; i++) {
        snprintf(b, sizeof b, "IsGet_Obj_HeroSoul_%s", souls[i]);
        if (!strcmp(n, b)) return true;
    }
    return nwd_pre(n, "Clear_Remains") && n[13];
}
static const char *const NWD_MAPS[NWD_MAPS_N] = {"MainField", "AocField", "MainFieldDungeon", "CDungeon"};  // (loot's LT_FLAG_MAPS)
static inline bool nwd_is_world(const char *n) {
    const char *u = strchr(n, '_');
    if (!u || strlen(n) >= 48) return false;
    bool map = false;
    for (int i = 0; i < NWD_MAPS_N; i++) map = map || ((size_t)(u - n) == strlen(NWD_MAPS[i]) && !strncmp(n, NWD_MAPS[i], (size_t)(u - n)));
    if (!map) return false;
    static const char *const kinds[5] = {"Item_", "Obj_", "Weapon_", "Barrel_", "Kibako_"};
    for (int i = 0; i < 5; i++)
        if (nwd_pre(u + 1, kinds[i])) return true;
    return false;
}
static inline bool nwd_is_shop(const char *n) {
    size_t l = strlen(n);
    return (l > 8 && !strcmp(n + l - 8, "_SoldOut")) || !strcmp(n, "GemShop_Open") || !strcmp(n, "Mans_Clothes_Shop");
}
static inline bool nwd_is_more(const char *n) {
    if (strlen(n) >= 48) return false;
    static const struct { const char *p; uint8_t extra; } pre[] = {
        {"Remains", 1}, {"FinalTrial", 0}, {"Npc", 0}, {"NPC", 0}, {"MiniGame", 0}, {"Kakariko", 0}, {"HatenoMini", 0}, {"HatenoNPC", 0},
        {"Rito", 0}, {"Gerudo", 0}, {"UMiiVillage", 0}, {"FironeMini", 0}, {"TreasureHunt", 0}, {"SunazarashiRace", 0}, {"MamonoShop", 0}};
    for (size_t i = 0; i < sizeof pre / sizeof *pre; i++)
        if (nwd_pre_us(n, pre[i].p, pre[i].extra)) return true;
    // ^[A-Za-z0-9-]+_(kind)_
    const char *u = strchr(n, '_');
    if (!u || u == n) return false;
    for (const char *c = n; c < u; c++)
        if (!((*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '-')) return false;
    const char *k = u + 1;
    static const char *const tags[] = {"LinkTag_", "LinkTagAnd_", "LinkTagOr_", "LinkTagNAnd_", "LinkTagNOr_", "LinkTagCount_", "LinkTagXOr_",
                                       "KibakoDesert_", "KibakoZora_"};
    for (size_t i = 0; i < sizeof tags / sizeof *tags; i++)
        if (nwd_pre(k, tags[i])) return true;
    static const char *const kinds[] = {"WallCrack", "IceWall", "Pot", "BeeHome", "OreDrop", "TreasureSpot", "FldObj", "TwnObj", "DgnObj", "PutRupee", "Necklace"};
    for (size_t i = 0; i < sizeof kinds / sizeof *kinds; i++)
        if (nwd_pre_us(k, kinds[i], 0)) return true;
    return false;
}
// a bool flag's family (0 = not shared). rev = it is in revival_bool_data; quest = QuestProduct names it
static inline int nwd_family(const char *n, bool rev, bool quest) {
    if (!n || !n[0] || strlen(n) > NWD_NAME_MAX) return 0;
    if (nwd_is_story(n)) return NWDF_STORY;
    if (nwd_is_shrine(n)) return NWDF_SHRINE;
    if (nwd_is_beast(n)) return NWDF_BEAST;
    if (nwd_pre(n, "MainField_Npc_HiddenKorok")) return NWDF_KOROK;
    if (strstr(n, "_TBox_")) return NWDF_CHEST;
    if (rev && nwd_is_world(n)) return NWDF_WORLD;
    if (quest) return NWDF_QUEST;
    if (nwd_pre(n, "IsPlayed") || nwd_pre(n, "IsGet")) return 0;
    if (nwd_is_shop(n)) return NWDF_SHOP;
    if (nwd_is_more(n)) return NWDF_MORE;
    return 0;
}

// what comes with a flag set here (PR quests_update)
enum { NWDG_NONE = 0, NWDG_ITEM, NWDG_SEED, NWDG_ORB, NWDG_HEART };
static const char *const NWD_RUNES[] = {"IsGet_Obj_RemoteBomb", "IsGet_Obj_RemoteBombCube", "IsGet_Obj_Magnetglove", "IsGet_Obj_StopTimer",
                                       "IsGet_Obj_IceMaker", "IsGet_Obj_Camera", "IsGet_Obj_RemoteBombLv2", "IsGet_Obj_RemoteBombCube2",
                                       "IsGet_Obj_StopTimerLv2", "IsGet_Obj_SheikSensor", "IsGet_Obj_SheikSensorLv2", "IsGet_Obj_SheikPadLv2",
                                       "IsGet_Obj_Motorcycle"};  // (PR slate_rune: the item = the name after "IsGet_")
// -> NWDG_*, *item = the pouch item's name (NWDG_ITEM / SEED / ORB; a pointer into n or a literal)
static inline int nwd_gift(const char *n, const char **item) {
    *item = NULL;
    if (!n) return NWDG_NONE;
    if (!strcmp(n, "IsGet_PortableUnit")) return *item = "Obj_DRStone_Get", NWDG_ITEM;  // (the slate in the key items)
    for (size_t i = 0; i < sizeof NWD_RUNES / sizeof *NWD_RUNES; i++)
        if (!strcmp(n, NWD_RUNES[i])) return *item = n + 6, NWDG_ITEM;
    if (!strcmp(n, "FindDungeon_Finish")) return *item = "PlayerStole2", NWDG_ITEM;  // (the plateau done: the king's paraglider)
    if (nwd_pre(n, "MainField_Npc_HiddenKorok")) return *item = "Obj_KorokNuts", NWDG_SEED;
    if (nwd_pre(n, "Clear_Dungeon")) return *item = "Obj_DungeonClearSeal", NWDG_ORB;
    if (nwd_pre(n, "IsGet_Obj_HeroSoul_")) return *item = n + 6, NWDG_ITEM;
    if (nwd_pre(n, "Get_HeartUtuwa_")) return NWDG_HEART;
    return NWDG_NONE;
}
// the slate's own flags wait until the finder's Demo700_0 is over (PR slate_group)
static inline bool nwd_slate_group(const char *n) {
    return !strcmp(n, "IsGet_PortableUnit") || !strcmp(n, "PortableUnit_PowerOn") || !strcmp(n, "OP_DRC") || !strcmp(n, "Open_FirstDoor") ||
           !strcmp(n, "IsPlayed_Demo700_0");
}
// a Korok flag's HashId (the number after the last '_'), 0 = none; *fly = a flying one's
static inline uint32_t nwd_korok_id(const char *n, bool *fly) {
    if (fly) *fly = n && strstr(n, "KorokFly") != NULL;
    const char *u = n ? strrchr(n, '_') : NULL;
    if (!u || !nwd_digits(u + 1) || strlen(u + 1) > 10) return 0;
    unsigned long long v = strtoull(u + 1, NULL, 10);
    return v <= 0xffffffffull ? (uint32_t)v : 0;
}

// ---------------------------------------------------------------- the list
typedef struct { uint32_t hash, name; uint16_t fam; uint8_t rev, pad; } NwdEnt;  // name = its offset in names
typedef struct {
    NwdEnt *e;
    uint32_t n, fp;
    char *names;
    size_t names_n;
    uint32_t fam_n[NWDF_COUNT], bool_n, quest_n, quest_bool, dup, clash, gdt_files;
} NwdList;
enum { NWDE_OK = 0, NWDE_GDT, NWDE_NOFLAGS, NWDE_MEM, NWDE_BIG };
static const char *const NWDE_TXT[] = {"ok", "gamedata.ssarc is not a SARC of bgdata files", "no bool flags in it", "out of memory", "more than 65535 flags"};
static inline void nwd_list_free(NwdList *L) {
    if (!L) return;
    free(L->e), free(L->names);
    memset(L, 0, sizeof *L);
}
typedef struct { const char *name; uint32_t hash; uint8_t rev; } NwdRaw;
static int nwd_raw_cmp(const void *a, const void *b) {
    const NwdRaw *x = (const NwdRaw *)a, *y = (const NwdRaw *)b;
    if (x->hash != y->hash) return x->hash < y->hash ? -1 : 1;
    return strcmp(x->name, y->name);
}
static int nwd_u32_cmp(const void *a, const void *b) {
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return x < y ? -1 : x > y;
}
static inline bool nwd_raw_push(NwdRaw **v, size_t *n, size_t *cap, const char *name, uint8_t rev) {
    if (*n == *cap) {
        size_t c = *cap ? *cap * 2 : 4096;
        NwdRaw *p = (NwdRaw *)realloc(*v, c * sizeof *p);
        if (!p) return false;
        *v = p, *cap = c;
    }
    (*v)[*n].name = name, (*v)[*n].hash = lt_crc32(name), (*v)[*n].rev = rev, (*n)++;
    return true;
}
// QuestProduct's names (Main / Sub / Mini: each step's NextFlag, each bool dependency flag) -> their hashes, sorted; the count
static inline uint32_t *nwd_quest_hashes(const uint8_t *q, size_t qn, uint32_t *count) {
    *count = 0;
    NwdByml b;
    if (!q || !nwd_byml_open(&b, q, qn) || b.root_t != NWDB_ARR) return NULL;
    uint32_t nq = nwd_byml_count(&b, NWDB_ARR, b.root, NWDB_ARR), n = 0, cap = 1024;
    uint32_t *h = (uint32_t *)malloc(cap * sizeof *h);
    for (uint32_t i = 0; h && i < nq; i++) {
        uint8_t t, st, dt;
        uint32_t o, so, dof;
        if (!nwd_byml_arr_at(&b, b.root, i, &t, &o) || t != NWDB_HASH) continue;
        const char *ty = nwd_byml_get_str(&b, t, o, "Type");
        if (!ty || (strcmp(ty, "Main") && strcmp(ty, "Sub") && strcmp(ty, "Mini"))) continue;
        for (int pass = 0; pass < 2; pass++) {
            if (!nwd_byml_get(&b, t, o, pass ? "StepDependencyFlags" : "Steps", &st, &so)) continue;
            uint32_t c = nwd_byml_count(&b, st, so, NWDB_ARR);
            for (uint32_t j = 0; j < c; j++) {
                if (!nwd_byml_arr_at(&b, so, j, &dt, &dof) || dt != NWDB_HASH) continue;
                const char *f = nwd_byml_get_str(&b, dt, dof, pass ? "DependencyFlag" : "NextFlag");
                if (pass) {
                    const char *ft = nwd_byml_get_str(&b, dt, dof, "DependencyFlagType");
                    if (!ft || strcmp(ft, "bool")) f = NULL;
                }
                if (!f || !f[0]) continue;
                if (n == cap) {
                    uint32_t *p = (uint32_t *)realloc(h, 2 * cap * sizeof *h);
                    if (!p) { free(h); return NULL; }
                    h = p, cap *= 2;
                }
                h[n++] = lt_crc32(f);
            }
        }
    }
    if (h) qsort(h, n, sizeof *h, nwd_u32_cmp);
    *count = n;
    return h;
}
static inline bool nwd_has_hash(const uint32_t *h, uint32_t n, uint32_t x) {
    uint32_t lo = 0, hi = n;
    while (lo < hi) {
        uint32_t m = lo + (hi - lo) / 2;
        if (h[m] < x) lo = m + 1;
        else hi = m;
    }
    return lo < n && h[lo] == x;
}
// gdt = GameData/gamedata.ssarc UNPACKED (a SARC of .bgdata BYMLs); quest = Quest/QuestProduct.sbquestpack unpacked (NULL = no
// QUEST family). -> NWDE_OK, L filled (nwd_list_free). Every bool flag of the families, once, sorted by its hash (crc32 of its name,
// the game's own: measured equal to every flag's HashValue in 1.9.0); two names with one hash: both left out (L->clash)
static inline int nwd_list_build(const uint8_t *gdt, size_t gn, const uint8_t *quest, size_t qn, NwdList *L) {
    memset(L, 0, sizeof *L);
    NwdSarc s;
    if (!nwd_sarc_open(&s, gdt, gn) || s.data_off > gn) return NWDE_GDT;
    NwdRaw *raw = NULL;
    size_t nr = 0, cap = 0;
    for (int i = 0; i < s.nodes; i++) {
        uint32_t beg, end;
        const char *fn = nwd_sarc_node(&s, i, &beg, &end);
        if (!fn || !strstr(fn, "bool_data") || (size_t)s.data_off + end > gn) continue;
        bool rev = strstr(fn, "revival_bool_data") != NULL;
        NwdByml b;
        if (!nwd_byml_open(&b, gdt + s.data_off + beg, end - beg)) continue;
        uint8_t t;
        uint32_t o;
        if (!nwd_byml_get(&b, b.root_t, b.root, "bool_data", &t, &o)) continue;
        uint32_t c = nwd_byml_count(&b, t, o, NWDB_ARR);
        L->gdt_files++;
        for (uint32_t j = 0; j < c; j++) {
            uint8_t et;
            uint32_t eo;
            if (!nwd_byml_arr_at(&b, o, j, &et, &eo) || et != NWDB_HASH) continue;
            const char *nm = nwd_byml_get_str(&b, et, eo, "DataName");
            if (!nm) continue;
            L->bool_n++;
            if (!nwd_raw_push(&raw, &nr, &cap, nm, rev)) { free(raw); return NWDE_MEM; }
        }
    }
    if (!nr) { free(raw); return NWDE_NOFLAGS; }
    uint32_t nq = 0, *qh = nwd_quest_hashes(quest, qn, &nq);
    L->quest_n = nq;
    qsort(raw, nr, sizeof *raw, nwd_raw_cmp);
    L->e = (NwdEnt *)malloc(nr * sizeof *L->e);
    size_t ncap = 1 << 20;
    L->names = (char *)malloc(ncap);
    if (!L->e || !L->names) { free(raw), free(qh), nwd_list_free(L); return NWDE_MEM; }
    for (size_t i = 0; i < nr;) {
        size_t j = i + 1;
        uint8_t rev = raw[i].rev;
        bool clash = false;
        for (; j < nr && raw[j].hash == raw[i].hash; j++) {
            if (strcmp(raw[j].name, raw[i].name)) clash = true;
            else L->dup++;
            rev |= raw[j].rev;
        }
        bool inq = nwd_has_hash(qh, nq, raw[i].hash);
        int fam = clash ? 0 : nwd_family(raw[i].name, rev != 0, inq);
        if (clash) L->clash++;
        if (inq && !clash) L->quest_bool++;
        if (fam) {
            size_t l = strlen(raw[i].name) + 1;
            if (L->names_n + l > ncap) {
                char *p = (char *)realloc(L->names, ncap * 2);
                if (!p) { free(raw), free(qh), nwd_list_free(L); return NWDE_MEM; }
                L->names = p, ncap *= 2;
            }
            memcpy(L->names + L->names_n, raw[i].name, l);
            NwdEnt *e = &L->e[L->n++];
            e->hash = raw[i].hash, e->name = (uint32_t)L->names_n, e->fam = (uint16_t)fam, e->rev = rev, e->pad = 0;
            L->names_n += l;
            for (int b = 0; b < NWDF_COUNT; b++)
                if (fam >> b & 1) L->fam_n[b]++;
        }
        i = j;
    }
    free(raw), free(qh);
    if (L->n > NET_WORLD_COUNT_MAX) { nwd_list_free(L); return NWDE_BIG; }
    if (!L->n) { nwd_list_free(L); return NWDE_NOFLAGS; }
    uint32_t fp = 0;
    for (uint32_t i = 0; i < L->n; i++) {
        uint8_t b6[6] = {(uint8_t)L->e[i].hash, (uint8_t)(L->e[i].hash >> 8), (uint8_t)(L->e[i].hash >> 16), (uint8_t)(L->e[i].hash >> 24),
                         (uint8_t)L->e[i].fam, (uint8_t)(L->e[i].fam >> 8)};
        fp = nwd_crc32_upd(fp, b6, 6);
    }
    L->fp = fp ? fp : 1;
    return NWDE_OK;
}
static inline const char *nwd_name(const NwdList *L, uint32_t i) { return L && i < L->n ? L->names + L->e[i].name : ""; }
static inline int nwd_index_of(const NwdList *L, uint32_t hash) {  // -> the entry, -1 none
    uint32_t lo = 0, hi = L ? L->n : 0;
    while (lo < hi) {
        uint32_t m = lo + (hi - lo) / 2;
        if (L->e[m].hash < hash) lo = m + 1;
        else hi = m;
    }
    return L && lo < L->n && L->e[lo].hash == hash ? (int)lo : -1;
}

// ---------------------------------------------------------------- bit sets + the union (no host)
static inline bool nwd_bit(const uint8_t *b, uint32_t i) { return b[i >> 3] >> (i & 7) & 1; }
static inline void nwd_bset(uint8_t *b, uint32_t i) { b[i >> 3] |= (uint8_t)(1u << (i & 7)); }
static inline void nwd_bclr(uint8_t *b, uint32_t i) { b[i >> 3] &= (uint8_t)~(1u << (i & 7)); }
static inline uint32_t nwd_popc(const uint8_t *b, size_t nb) {
    uint32_t c = 0;
    for (size_t i = 0; i < nb; i++) c += (uint32_t)__builtin_popcount(b[i]);
    return c;
}
typedef struct {
    uint32_t n;
    size_t nb;
    uint8_t *rev, *famok;               // (the list's: revival flags; the families shared now)
    uint8_t *mine, *prev, *held, *applied, *failed;
    uint8_t *known[NWD_PEERS], *stage[NWD_PEERS];
    bool on[NWD_PEERS], have[NWD_PEERS];  // (he shares; his whole set came)
    uint16_t stage_gen[NWD_PEERS], stage_got[NWD_PEERS];
    bool passed;                        // (a whole pass of our own flags is in mine)
} NwdCore;
static inline void nwd_core_free(NwdCore *c) {
    if (!c) return;
    free(c->rev), free(c->famok), free(c->mine), free(c->prev), free(c->held), free(c->applied), free(c->failed);
    for (int k = 0; k < NWD_PEERS; k++) free(c->known[k]), free(c->stage[k]);
    memset(c, 0, sizeof *c);
}
static inline bool nwd_core_init(NwdCore *c, const NwdList *L, int fams) {
    memset(c, 0, sizeof *c);
    if (!L || !L->n) return false;
    c->n = L->n, c->nb = (L->n + 7) / 8;
    uint8_t **all[] = {&c->rev, &c->famok, &c->mine, &c->prev, &c->held, &c->applied, &c->failed};
    for (size_t i = 0; i < sizeof all / sizeof *all; i++)
        if (!(*all[i] = (uint8_t *)calloc(1, c->nb))) { nwd_core_free(c); return false; }
    for (int k = 0; k < NWD_PEERS; k++)
        if (!(c->known[k] = (uint8_t *)calloc(1, c->nb)) || !(c->stage[k] = (uint8_t *)calloc(1, c->nb))) { nwd_core_free(c); return false; }
    for (uint32_t i = 0; i < L->n; i++)
        if (L->e[i].rev) nwd_bset(c->rev, i);
    for (uint32_t i = 0; i < L->n; i++)
        if (L->e[i].fam & fams) nwd_bset(c->famok, i);
    return true;
}
static inline void nwd_core_fams(NwdCore *c, const NwdList *L, int fams) {
    if (!c->famok) return;
    memset(c->famok, 0, c->nb);
    for (uint32_t i = 0; i < L->n && i < c->n; i++)
        if (L->e[i].fam & fams) nwd_bset(c->famok, i);
}
static inline void nwd_core_peer_reset(NwdCore *c, int k) {
    if (k < 0 || k >= NWD_PEERS || !c->known[k]) return;
    memset(c->known[k], 0, c->nb), memset(c->stage[k], 0, c->nb);
    c->on[k] = c->have[k] = false, c->stage_gen[k] = 0, c->stage_got[k] = 0;
}
// a friend's page: -> 1 = his whole set is now known (the last page of that gen came), 0 = taken, -1 = refused
static inline int nwd_core_page(NwdCore *c, int k, uint16_t gen, int page, int pages, const uint8_t *bits, int nbytes) {
    if (k < 0 || k >= NWD_PEERS || !c->known[k] || pages != net_world_pages(c->n) || page < 0 || page >= pages ||
        nbytes != net_world_page_bytes(c->n, page))
        return -1;
    if (c->stage_gen[k] != gen || !c->stage_got[k]) memset(c->stage[k], 0, c->nb), c->stage_gen[k] = gen, c->stage_got[k] = 0;
    memcpy(c->stage[k] + (size_t)page * (NET_WORLD_PAGE_BITS / 8), bits, (size_t)nbytes);
    if (c->n & 7) c->stage[k][c->nb - 1] &= (uint8_t)((1u << (c->n & 7)) - 1);  // (no bits past the list)
    c->stage_got[k] |= (uint16_t)(1u << page);
    if (c->stage_got[k] != (uint16_t)((1u << pages) - 1)) return 0;
    memcpy(c->known[k], c->stage[k], c->nb);  // (his whole set now: what his Blood Moon cleared leaves it)
    c->have[k] = true, c->stage_got[k] = 0;
    return 1;
}
// a friend's SET: -> how many of them were new to us of him (indices are checked by net_world_fits first)
static inline int nwd_core_set(NwdCore *c, int k, const uint16_t *idx, int n) {
    if (k < 0 || k >= NWD_PEERS || !c->known[k]) return 0;
    int got = 0;
    for (int i = 0; i < n; i++)
        if (idx[i] < c->n && !nwd_bit(c->known[k], idx[i])) nwd_bset(c->known[k], idx[i]), got++;
    return got;
}
static inline bool nwd_core_union_has(const NwdCore *c, uint32_t i) {
    for (int k = 0; k < NWD_PEERS; k++)
        if (c->on[k] && c->known[k] && nwd_bit(c->known[k], i)) return true;
    return false;
}
static inline void nwd_core_union(const NwdCore *c, uint8_t *u) {
    memset(u, 0, c->nb);
    for (int k = 0; k < NWD_PEERS; k++)
        if (c->on[k] && c->known[k])
            for (size_t b = 0; b < c->nb; b++) u[b] |= c->known[k][b];
}
// a whole pass of our own flags is in mine: -> our own new ones (set since the last pass, not by us from a friend) into out (max),
// *cleared = how many of our revival flags cleared since. The first pass gives none (our pages carry it).
static inline int nwd_core_pass_done(NwdCore *c, uint16_t *out, int max, int *total, uint32_t *cleared) {
    int n = 0, t = 0;
    uint32_t cl = 0;
    for (size_t b = 0; b < c->nb; b++) {
        uint8_t nw = c->mine[b] & (uint8_t)~c->prev[b], own = nw & (uint8_t)~c->applied[b] & c->famok[b];
        cl += (uint32_t)__builtin_popcount(c->prev[b] & (uint8_t)~c->mine[b] & c->rev[b]);
        c->applied[b] &= (uint8_t)~nw;  // (one we set from a friend, now seen set: ours like any other from here)
        if (!c->passed) continue;
        for (int i = 0; own && i < 8; i++)
            if (own >> i & 1) {
                if (n < max) out[n++] = (uint16_t)(8 * b + (size_t)i);
                t++;
            }
    }
    memcpy(c->prev, c->mine, c->nb);
    if (!c->passed) cl = 0;
    c->passed = true;
    if (total) *total = t;
    if (cleared) *cleared = cl;
    return n;
}
// a Blood Moon here: what the friends' sets still have of the revival flags (from before it) is held off here -> how many
static inline uint32_t nwd_core_moon(NwdCore *c, uint8_t *tmp) {
    nwd_core_union(c, tmp);
    uint32_t h = 0;
    for (size_t b = 0; b < c->nb; b++) {
        uint8_t x = tmp[b] & c->rev[b] & (uint8_t)~c->mine[b];
        h += (uint32_t)__builtin_popcount(x & (uint8_t)~c->held[b]);
        c->held[b] |= x;
    }
    return h;
}
// held ones no friend has any more (his own Blood Moon cleared it there too) are free again -> how many
static inline uint32_t nwd_core_release(NwdCore *c, uint8_t *tmp) {
    nwd_core_union(c, tmp);
    uint32_t r = 0;
    for (size_t b = 0; b < c->nb; b++) {
        uint8_t keep = c->held[b] & tmp[b];
        r += (uint32_t)__builtin_popcount(c->held[b] & (uint8_t)~keep);
        c->held[b] = keep;
    }
    return r;
}
// what the union has and we lack (families shared, not held, not set by us already, not failed) -> up to max indices, *total all
static inline int nwd_core_lack(const NwdCore *c, uint8_t *tmp, uint32_t *out, int max, uint32_t *total) {
    nwd_core_union(c, tmp);
    int n = 0;
    uint32_t t = 0;
    for (size_t b = 0; b < c->nb; b++) {
        uint8_t x = tmp[b] & (uint8_t)~c->mine[b] & c->famok[b] & (uint8_t)~c->held[b] & (uint8_t)~c->applied[b] & (uint8_t)~c->failed[b];
        for (int i = 0; x && i < 8; i++)
            if (x >> i & 1) {
                if (n < max) out[n++] = (uint32_t)(8 * b + (size_t)i);
                t++;
            }
    }
    if (total) *total = t;
    return n;
}
// our set for the pages: mine, only the families shared
static inline void nwd_core_ours(const NwdCore *c, uint8_t *out) {
    for (size_t b = 0; b < c->nb; b++) out[b] = c->mine[b] & c->famok[b];
}

// ---------------------------------------------------------------- the save guard (copy before the first union in a room)
// (s290 revfix, the review's F1 / F2 / F4 + its FYI "no retry") The gate is per slot now, not per room join:
//   cur      the slot the game is on: its newest READ (kind 0) in this run; no read yet = the slot it writes (a new game). With an
//            exe that has the event ring the slot is never guessed by file time: no read and no write yet = the union waits for
//            one, however long (old exe without the ring: the newest by file time, as before).
//   ok       the union may be applied only while cur is COVERED: it is the slot of the done copy (run), or a slot the game wrote
//            while the state it saved came from a covered slot (an autosave of the union'd game, the read back after a save). A
//            read of any other slot (the pause menu's Load of another save) makes ok false at once: that slot is copied first.
//   torn     a write / written out of the slot being copied, from the copy's start to NWD_SETTLE_MS after its end (or the
//            worker saw game_data.sav's size / time change): the copy is kept (never deleted) and the slot is copied again into a
//            NEW folder once the write is over. NWD_COPY_TRIES copies of a slot in a room that did not end clean (torn or failed)
//            and that slot gets no union in that room. A failed copy is tried again NWD_RETRY_MS later.
//   same     (F4) the newest folder this code made for that slot holds exactly the slot's files byte for byte: no new folder,
//            that one is the copy (a Melty connection that drops and comes back makes no new folder while the save is unchanged).
//            Nothing is ever deleted.
enum { NWDB_NONE = 0, NWDB_RUN, NWDB_DONE, NWDB_FAIL, NWDB_CHECK };  // (CHECK: the copy ended, late save events are waited for)
enum { NWDJ_OK = 0, NWDJ_TORN, NWDJ_FAIL };                           // (the worker's result)
enum { NWDA_NONE = 0, NWDA_COPY, NWDA_CLEAN, NWDA_TORN, NWDA_GIVEUP, NWDA_NOSAVE };  // (nwd_gate_step: what to do / what happened)
#define NWD_GATE_COVER 8         // slots kept as covered (BotW: 5 autosaves + 1 save; a ninth = copied again when read: the safe side)
#define NWD_SAVE_WAIT_MS 20000   // a save being written: the copy waits up to this long
#define NWD_SETTLE_MS 2000       // GUESS: after the copy, a save event of that slot this late still makes it torn (the exe logs the
                                 // event right after the game's first write: microseconds; 2 s is wide)
#define NWD_COPY_TRIES 4         // GUESS: unclean copies (torn / failed) of a slot in a room before it is given up there
#define NWD_RETRY_MS 30000       // a failed copy: tried again this much later (the review's FYI 2)
typedef struct {
    uint32_t room, done_room;
    int st;
    char path[600], why[200];
    char cur[600];                         // the slot the game is on ("" none known yet)
    bool cur_read;                         // cur came from a read (else from a write: a new game)
    char run[600];                         // the slot of the copy under way / of the last copy
    char cover[NWD_GATE_COVER][600];       // slots written while the game's state came from a covered slot
    int ncover;
    bool torn, same;                       // the copy under way / just ended: a save write of its slot meanwhile; an old folder reused
    int tries;                             // unclean copies of run in room
    uint64_t end_at;                       // when the last copy ended
    bool writing;                          // a write under way (the newest event was a first write)
    uint64_t write_at;
} NwdGate;
static inline bool nwd_same_dir(const char *a, const char *b) {  // (the exe's own paths: '/' or '\\', any case, a trailing slash)
    if (!a || !b) return false;
    size_t la = strlen(a), lb = strlen(b);
    while (la > 1 && (a[la - 1] == '/' || a[la - 1] == '\\')) la--;
    while (lb > 1 && (b[lb - 1] == '/' || b[lb - 1] == '\\')) lb--;
    if (!la || la != lb) return false;
    for (size_t i = 0; i < la; i++) {
        char x = a[i] == '\\' ? '/' : a[i], y = b[i] == '\\' ? '/' : b[i];
        if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
        if (y >= 'A' && y <= 'Z') y = (char)(y + 32);
        if (x != y) return false;
    }
    return true;
}
static inline bool nwd_gate_covers(const NwdGate *g, const char *dir) {
    if (!dir || !dir[0]) return false;
    if (nwd_same_dir(g->run, dir)) return true;
    for (int i = 0; i < g->ncover; i++)
        if (nwd_same_dir(g->cover[i], dir)) return true;
    return false;
}
static inline void nwd_gate_cover_add(NwdGate *g, const char *dir) {
    if (!dir || !dir[0] || g->ncover >= NWD_GATE_COVER) return;
    for (int i = 0; i < g->ncover; i++)
        if (nwd_same_dir(g->cover[i], dir)) return;
    snprintf(g->cover[g->ncover++], sizeof g->cover[0], "%s", dir);
}
// the union may be applied now (a copy of this slot is done in this room)
static inline bool nwd_gate_ok(const NwdGate *g, uint32_t room) {
    return room && g->st == NWDB_DONE && g->done_room == room && nwd_gate_covers(g, g->cur);
}
static inline bool nwd_gate_failed(const NwdGate *g, uint32_t room) { return g->st == NWDB_FAIL && g->done_room == room; }
static inline bool nwd_gate_busy(const NwdGate *g) { return g->st == NWDB_RUN || g->st == NWDB_CHECK; }
// a save event of the exe's ring (kind 0 first read, 1 first write, 2 written out) of the slot folder dir
static inline void nwd_gate_event(NwdGate *g, int kind, const char *dir, uint64_t now) {
    if (!dir || !dir[0]) return;
    g->writing = kind == 1;
    if (kind == 1) g->write_at = now;
    if (kind == 0) {
        snprintf(g->cur, sizeof g->cur, "%s", dir), g->cur_read = true;
        return;
    }
    // (the game's state came from cur: a covered slot, or the one being copied -> what it writes now is covered too)
    bool from = (nwd_gate_busy(g) && nwd_same_dir(g->cur, g->run)) || (g->st == NWDB_DONE && nwd_gate_covers(g, g->cur));
    if (nwd_gate_busy(g) && nwd_same_dir(dir, g->run)) g->torn = true;  // (F2: the copy may be torn or mixed)
    if (from && !nwd_same_dir(dir, g->run)) nwd_gate_cover_add(g, dir);
    if (!g->cur_read) snprintf(g->cur, sizeof g->cur, "%s", dir);  // (no read in this run yet: a new game, the slot it writes)
}
// the worker ended (NWDJ_*): path = the copy (or the old folder reused), why = the failure
static inline void nwd_gate_job(NwdGate *g, int res, bool same, const char *path, const char *why, uint64_t now) {
    if (g->st != NWDB_RUN) return;
    g->end_at = now ? now : 1;
    if (res == NWDJ_FAIL) {
        g->st = NWDB_FAIL, g->done_room = g->room, g->tries++, g->path[0] = 0;
        snprintf(g->why, sizeof g->why, "%s", why ? why : "?");
        return;
    }
    snprintf(g->path, sizeof g->path, "%s", path ? path : "");
    g->same = same;
    if (res == NWDJ_TORN) g->torn = true;
    g->st = NWDB_CHECK;
}
// one look a tick, after the events: -> NWDA_*. friends = a friend's set came (a copy is made only then). NWDA_COPY = copy g->run now
static inline int nwd_gate_step(NwdGate *g, uint32_t room, uint64_t now, bool friends) {
    if (g->st == NWDB_RUN) return NWDA_NONE;
    if (g->st == NWDB_CHECK) {
        if (g->torn) {
            g->torn = false, g->tries++;
            if (g->tries >= NWD_COPY_TRIES) {
                g->st = NWDB_FAIL, g->done_room = g->room, g->end_at = now ? now : 1;
                snprintf(g->why, sizeof g->why, "the game wrote that save during each of %d copies", g->tries);
                return NWDA_GIVEUP;
            }
            g->st = NWDB_NONE;  // (copied again, into a new folder, once the write is over)
            return NWDA_TORN;
        }
        if (now - g->end_at < NWD_SETTLE_MS) return NWDA_NONE;
        g->st = NWDB_DONE, g->done_room = g->room, g->tries = 0;
        return NWDA_CLEAN;
    }
    if (!room || nwd_gate_ok(g, room) || !friends) return NWDA_NONE;
    if (!g->cur[0]) return NWDA_NOSAVE;
    bool again = g->room == room && nwd_same_dir(g->cur, g->run);  // (the slot of the last try, in this room)
    if (g->st == NWDB_FAIL && g->done_room == room && again && (g->tries >= NWD_COPY_TRIES || now - g->end_at < NWD_RETRY_MS)) return NWDA_NONE;
    if (g->writing && now - g->write_at < NWD_SAVE_WAIT_MS) return NWDA_NONE;
    if (!again) g->tries = 0;
    if (g->st == NWDB_DONE && nwd_gate_covers(g, g->cur)) nwd_gate_cover_add(g, g->run);  // (a new room on a covered slot: the chain goes on)
    else g->ncover = 0;  // (another save: what was covered came from the old one)
    snprintf(g->run, sizeof g->run, "%s", g->cur);
    g->st = NWDB_RUN, g->room = room, g->torn = g->same = false, g->path[0] = g->why[0] = 0;
    return NWDA_COPY;
}

static inline bool nwd_isdir(const char *p) {
    struct stat st;
    return p && !stat(p, &st) && S_ISDIR(st.st_mode);
}
static inline bool nwd_exists(const char *p) {
    struct stat st;
    return p && !stat(p, &st);
}
static inline bool nwd_mkdir(const char *p) {
#ifdef _WIN32
    return _mkdir(p) == 0;
#else
    return mkdir(p, 0777) == 0;
#endif
}
// a save slot's folder ".../save/save_0/3" -> the backups' folder (beside the save root: ".../sora-networld-backups") and the slot
// number; false = not a slot folder (no save_0/<digits> at its end)
static inline bool nwd_backup_root(const char *slot_dir, char *out, size_t cap, int *slot) {
    char d[600];
    size_t l = slot_dir ? strlen(slot_dir) : 0;
    if (!l || l >= sizeof d) return false;
    memcpy(d, slot_dir, l + 1);
    while (l > 1 && (d[l - 1] == '/' || d[l - 1] == '\\')) d[--l] = 0;
    char *s1 = NULL;
    for (char *p = d + l; p > d; p--)
        if (p[-1] == '/' || p[-1] == '\\') { s1 = p - 1; break; }
    if (!s1 || !nwd_digits(s1 + 1) || strlen(s1 + 1) > 3) return false;
    *slot = atoi(s1 + 1);
    *s1 = 0;  // (d = .../save/save_0)
    char *s0 = NULL;
    for (char *p = s1; p > d; p--)
        if (p[-1] == '/' || p[-1] == '\\') { s0 = p - 1; break; }
    const char *last = s0 ? s0 + 1 : d;
    if (strcmp(last, "save_0")) return false;
    if (!s0) return snprintf(out, cap, "../sora-networld-backups") < (int)cap;  // ("save_0/3": the save root is the working folder)
    *s0 = 0;  // (d = .../save, the save root)
    char *s2 = NULL;
    for (char *p = s0; p > d; p--)
        if (p[-1] == '/' || p[-1] == '\\') { s2 = p - 1; break; }
    if (!s2) return snprintf(out, cap, "sora-networld-backups") < (int)cap;  // ("save/save_0/3": beside it, in the working folder)
    *s2 = 0;
    return snprintf(out, cap, "%s/sora-networld-backups", d[0] ? d : "/") < (int)cap;
}
static inline long long nwd_copy_file(const char *src, const char *dst) {  // -> bytes, -1 = failed (dst never overwritten)
    if (nwd_exists(dst)) return -1;
    FILE *a = fopen(src, "rb");
    if (!a) return -1;
    FILE *b = fopen(dst, "wb");
    if (!b) { fclose(a); return -1; }
    char buf[65536];
    long long t = 0;
    size_t r;
    bool ok = true;
    while ((r = fread(buf, 1, sizeof buf, a)) > 0)
        if (fwrite(buf, 1, r, b) != r) { ok = false; break; }
    ok = ok && !ferror(a);
    fclose(a);
    if (fclose(b) != 0) ok = false;
    if (ok) t = 0;
    if (ok) {
        struct stat s1, s2;
        ok = !stat(src, &s1) && !stat(dst, &s2) && s1.st_size == s2.st_size;
        t = ok ? (long long)s2.st_size : -1;
    }
    return ok ? t : -1;
}
// copy the slot folder's files into a NEW folder <root>/<stamp>_slot<N>[_2 ..]: -> files copied (game_data.sav among them), -1 =
// failed (why). Never writes into a folder that existed before.
static inline int nwd_backup_slot(const char *slot_dir, const char *stamp, char *path, size_t pcap, long long *bytes, char *why, size_t wcap) {
    char root[600];
    int slot = -1;
    if (bytes) *bytes = 0;
    if (path && pcap) path[0] = 0;
    if (!nwd_isdir(slot_dir)) return snprintf(why, wcap, "the save folder %s is not there", slot_dir ? slot_dir : "(none)"), -1;
    if (!nwd_backup_root(slot_dir, root, sizeof root, &slot)) return snprintf(why, wcap, "%s is not a save_0/<slot> folder", slot_dir), -1;
    if (!nwd_isdir(root) && !nwd_mkdir(root) && !nwd_isdir(root)) return snprintf(why, wcap, "could not make %s", root), -1;
    char dir[700];
    bool made = false;
    for (int k = 1; k < 100 && !made; k++) {
        if (k == 1) snprintf(dir, sizeof dir, "%s/%s_slot%d", root, stamp, slot);
        else snprintf(dir, sizeof dir, "%s/%s_slot%d_%d", root, stamp, slot, k);
        if (nwd_exists(dir)) continue;  // (an earlier backup: never written into)
        made = nwd_mkdir(dir);
        if (!made && !nwd_exists(dir)) return snprintf(why, wcap, "could not make %s", dir), -1;
    }
    if (!made) return snprintf(why, wcap, "no free name for the backup in %s", root), -1;
    if (path) snprintf(path, pcap, "%s", dir);
    DIR *dd = opendir(slot_dir);
    if (!dd) return snprintf(why, wcap, "could not list %s", slot_dir), -1;
    int files = 0;
    bool sav = false;
    struct dirent *de;
    while ((de = readdir(dd))) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        char src[900], dst[900];
        snprintf(src, sizeof src, "%s/%s", slot_dir, de->d_name);
        snprintf(dst, sizeof dst, "%s/%s", dir, de->d_name);
        struct stat st;
        if (stat(src, &st) || !S_ISREG(st.st_mode)) continue;
        long long b = nwd_copy_file(src, dst);
        if (b < 0) { closedir(dd); return snprintf(why, wcap, "could not copy %s to %s", src, dst), -1; }
        files++;
        if (bytes) *bytes += b;
        if (!strcmp(de->d_name, "game_data.sav")) sav = b > 0;
    }
    closedir(dd);
    if (!sav) return snprintf(why, wcap, "no game_data.sav in %s", slot_dir), -1;
    return files;
}
// (s290 revfix F4) a folder name nwd_backup_slot makes: "<YYYY-MM-DD_HH-MM-SS>_slot<N>" or "..._slot<N>_<k>" (k 2..99) -> its slot, k
// (1 = no suffix). Anything else is not ours.
static inline bool nwd_backup_name(const char *n, int *slot, int *k) {
    static const char pat[] = "0000-00-00_00-00-00_slot";
    if (!n) return false;
    for (int i = 0; pat[i]; i++)
        if (pat[i] == '0' ? !(n[i] >= '0' && n[i] <= '9') : n[i] != pat[i]) return false;
    const char *p = n + sizeof pat - 1;
    int s = 0, d = 0, kk = 1;
    while (d < 4 && p[d] >= '0' && p[d] <= '9') s = s * 10 + (p[d] - '0'), d++;
    if (!d || d > 3) return false;
    p += d;
    if (*p == '_') {
        p++, d = kk = 0;
        while (d < 3 && p[d] >= '0' && p[d] <= '9') kk = kk * 10 + (p[d] - '0'), d++;
        if (!d || d > 2 || p[d] || kk < 2) return false;
    } else if (*p)
        return false;
    if (slot) *slot = s;
    if (k) *k = kk;
    return true;
}
static inline int nwd_same_file(const char *a, const char *b) {  // -> 1 the same bytes, 0 not (or unreadable)
    struct stat sa, sb;
    if (stat(a, &sa) || stat(b, &sb) || !S_ISREG(sa.st_mode) || !S_ISREG(sb.st_mode) || sa.st_size != sb.st_size) return 0;
    FILE *x = fopen(a, "rb"), *y = x ? fopen(b, "rb") : NULL;
    int same = x && y;
    static char ba[65536], bb[65536];  // (the worker thread only: one copy job at a time)
    size_t ra, rb;
    while (same && (ra = fread(ba, 1, sizeof ba, x)) > 0) {
        rb = fread(bb, 1, ra, y);
        same = rb == ra && !memcmp(ba, bb, ra);
    }
    if (same && (ferror(x) || fread(bb, 1, 1, y) != 0)) same = 0;
    if (x) fclose(x);
    if (y) fclose(y);
    return same;
}
// the newest folder in root that this code made for that slot: does it hold exactly the slot folder's regular files, byte for
// byte (and nothing else)? -> true + its path, files, bytes
static inline bool nwd_backup_same(const char *slot_dir, const char *root, int slot, char *path, size_t pcap, int *files, long long *bytes) {
    DIR *d = opendir(root);
    if (!d) return false;
    char best[300] = "", dir[900];
    int bestk = 0;
    struct dirent *de;
    while ((de = readdir(d))) {
        int s = -1, k = 0;
        if (!nwd_backup_name(de->d_name, &s, &k) || s != slot || strlen(de->d_name) >= sizeof best) continue;
        int c = best[0] ? strncmp(de->d_name, best, 19) : 1;  // (the stamp: local time, sorts as it reads)
        if (c > 0 || (c == 0 && k > bestk)) snprintf(best, sizeof best, "%s", de->d_name), bestk = k;
    }
    closedir(d);
    if (!best[0]) return false;
    snprintf(dir, sizeof dir, "%s/%s", root, best);
    if (!nwd_isdir(dir)) return false;
    int n = 0, m = 0;
    long long b = 0;
    bool sav = false, ok = true;
    if (!(d = opendir(slot_dir))) return false;
    while (ok && (de = readdir(d))) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        char src[900], dst[1200];
        snprintf(src, sizeof src, "%s/%s", slot_dir, de->d_name);
        struct stat st;
        if (stat(src, &st) || !S_ISREG(st.st_mode)) continue;
        snprintf(dst, sizeof dst, "%s/%s", dir, de->d_name);
        ok = nwd_same_file(src, dst) == 1;
        n++, b += (long long)st.st_size;
        if (!strcmp(de->d_name, "game_data.sav")) sav = true;
    }
    closedir(d);
    if (!ok || !sav) return false;
    if (!(d = opendir(dir))) return false;
    while ((de = readdir(d)))
        if (strcmp(de->d_name, ".") && strcmp(de->d_name, "..")) m++;  // (it holds anything more: not the same)
    closedir(d);
    if (m != n) return false;
    if (path) snprintf(path, pcap, "%s", dir);
    if (files) *files = n;
    if (bytes) *bytes = b;
    return true;
}
// the worker's whole job: the newest copy of that slot is the same (F4: no new folder, *same) or a new copy (nwd_backup_slot).
// *torn = game_data.sav's size or time changed from before the job to after it (F2: a save written meanwhile; the exe's events
// are the other check, nwd_gate_event). -> files, -1 = failed (why)
static inline int nwd_backup_run(const char *slot_dir, const char *stamp, char *path, size_t pcap, long long *bytes, bool *same, bool *torn,
                                 char *why, size_t wcap) {
    char sav[700], root[600];
    struct stat s0, s1;
    int slot = -1, files = 0;
    *same = *torn = false;
    snprintf(sav, sizeof sav, "%s/game_data.sav", slot_dir ? slot_dir : "");
    bool had = !stat(sav, &s0);
    if (!had && nwd_isdir(slot_dir)) return snprintf(why, wcap, "no game_data.sav in %s", slot_dir), -1;  // (no folder made for it)
    if (had && nwd_backup_root(slot_dir, root, sizeof root, &slot) && nwd_backup_same(slot_dir, root, slot, path, pcap, &files, bytes))
        *same = true;
    else
        files = nwd_backup_slot(slot_dir, stamp, path, pcap, bytes, why, wcap);
    if (files > 0) *torn = !had || stat(sav, &s1) || s1.st_size != s0.st_size || s1.st_mtime != s0.st_mtime;
    return files;
}

// ---------------------------------------------------------------- the game files (the list build's worker thread; host test)
// <romfs>/<pack> (a SARC) -> its file `inner`, unpacked when Yaz0 (malloc'd; NULL + why)
static inline uint8_t *nwd_read_packed(const char *pack, const char *inner, size_t *outn, char *why, size_t wcap) {
    *outn = 0;
    FILE *f = fopen(pack, "rb");
    if (!f) return snprintf(why, wcap, "cannot open %s", pack), (uint8_t *)NULL;
    uint8_t h[0x20];
    uint8_t *hd = NULL, *z = NULL, *u = NULL;
    if (fread(h, 1, sizeof h, f) != sizeof h || memcmp(h, "SARC", 4)) { snprintf(why, wcap, "%s is not a SARC", pack); goto out; }
    uint32_t doff = nwd_le32(h + 12);
    if (doff < 0x20 || doff > (64u << 20)) { snprintf(why, wcap, "%s: a bad data offset", pack); goto out; }
    hd = (uint8_t *)malloc(doff);
    NwdSarc s;
    uint32_t beg = 0, end = 0;
    if (!hd || fseek(f, 0, SEEK_SET) || fread(hd, 1, doff, f) != doff || !nwd_sarc_open(&s, hd, doff) || !nwd_sarc_find(&s, inner, &beg, &end) ||
        end - beg > (64u << 20)) {
        snprintf(why, wcap, "%s: no %s in it", pack, inner);
        goto out;
    }
    size_t zn = end - beg;
    z = (uint8_t *)malloc(zn ? zn : 1);
    if (!z || fseek(f, (long)(doff + beg), SEEK_SET) || fread(z, 1, zn, f) != zn) { snprintf(why, wcap, "%s: %s cut short", pack, inner); goto out; }
    size_t un = nwd_yaz0_size(z, zn);
    if (!un) { u = z, z = NULL, *outn = zn; goto out; }
    if (un > (256u << 20) || !(u = (uint8_t *)malloc(un)) || !nwd_yaz0_unpack(z, zn, u, un)) {
        free(u), u = NULL;
        snprintf(why, wcap, "%s: %s does not unpack", pack, inner);
        goto out;
    }
    *outn = un;
out:
    free(hd), free(z);
    fclose(f);
    return u;
}
#ifdef NET_WORLD_GLUE
// ===================== GLUE (sora_net.h: NET, PEER_LIVE, net_dests, net_send_all, g_net_loop, LOG; the room's
// sora_melty_connected; the exe's sw_api + exports) =====================
#include "sw_api.h"
_Static_assert(NWD_PEERS >= NET_MAX_PEERS, "a set for every friend slot");

static bool g_net_world;              // live "set networld" (compiled default 0)
static bool g_net_world_log;          // live "set networldlog"
static int g_net_world_fam = NWDF_DEFAULT;  // live "set networldfam"

#define NWD_PASS_MS 2000      // a look at our own flags every 2 s (PR: every 60 frames)
#define NWD_CHUNK 6000        // flags read a driven frame
#define NWD_RESOLVE 1500      // flags looked up (binary search) a driven frame, the first time
#define NWD_APPLY_MAX 256     // flags set a pass at most (PR: all at once)
#define NWD_GIFT_MAX 12       // of them with an item (pouch / heart / cutscene)
#define NWD_KOROK_R 60.0f     // PR: its cutscene when the Korok is within 60 m
#define NWD_ACTORS 8192

static struct {
    bool tried, sites, said, said_udp, said_save;
    const SwApi *api;
    uint64_t s_gdt, s_gdt_handle, s_gdt_set, s_pause, s_pinfo, s_ss_vt, s_can_get, s_get, s_special, s_ev_start, s_ev_init, s_ev_name,
        s_ev_free, s_korok_mgr;
    bool (*in_event)(void);
    int (*save_ev)(uint64_t *, int *, int *, char *, int);
    const char *const *romfs_dir;
    const char *(*data_root)(void);
    const char *const *save_dir;
    int (*moons_fn)(void);
    // the list (built on a worker thread) + the union
    volatile int job;  // 0 not started, 1 building, 2 built, 3 failed
    NwdList JL;
    char job_why[300], job_path[700];
    uint32_t job_ms;
    bool ready;
    NwdList L;
    NwdCore C;
    uint8_t *tmp, *page_buf;
    uint32_t *lack;
    uint64_t *va;      // each flag's value address (0 = not looked up yet)
    uint8_t *none;     // looked up: not a flag of this game
    uint64_t va_buf, va_arr;
    uint32_t va_cnt;
    // the pass
    uint32_t pos;
    bool in_pass;
    uint64_t next_pass;
    // friends
    uint64_t joined[NET_MAX_PEERS];
    bool join_due, answer_due, pages_due, said_list[NET_MAX_PEERS];
    uint16_t gen, seq;
    // the room + the save guard
    bool in_room;
    uint32_t room;
    NwdGate G;
    volatile int bjob;  // 0 none, 1 copying, 2 done, 3 failed
    char bslot[600], bstamp[32], bpath[700], bwhy[300];
    int bfiles;
    long long bbytes;
    bool bsame, btorn;  // (s290 revfix: the worker's newest copy was the same / game_data.sav changed during the job)
    uint64_t ev_seq;    // (the exe's save events: the gate's own state, nwd_gate_event)
    bool said_fail, said_rearm;
    uint64_t gate_wait_at;
    // the Blood Moon
    int moons;
    bool moons_known, moon_pending;
    uint64_t next_moon, moon_at, pass_start;
    uint32_t moon_held;
    // Korok cutscenes (PR korok_event)
    uint32_t korok_played[16], wait_h[16];
    int korok_np, wait_n[16];
    uint64_t actors[NWD_ACTORS];
    uint32_t cost_last, cost_max;
    struct { uint32_t passes, own_tx, set_tx, page_tx, state_tx, rx_off, rx_bad, rx_early, rx_list, rx_state, rx_page, rx_set, snaps, set_new,
             applied, had, none, refused, gifts, pouch_full, seeds, orbs, hearts, items, koroks, korok_wait, held, released, moons, waits_event,
             waits_gate, waits_moon, slate_wait, not_frame, paused, backups, backup_fail, unsent, resolves, va_reset, backup_same, backup_torn, rearms; } st;
} NWD;

static bool nwd_rd(uint64_t a, void *out, size_t n) { return a && NWD.api && NWD.api->read(a, out, n); }
static uint64_t nwd_q(uint64_t a) { uint64_t v = 0; return nwd_rd(a, &v, 8) ? v : 0; }
static uint32_t nwd_d(uint64_t a) { uint32_t v = 0; return nwd_rd(a, &v, 4) ? v : 0; }
static uint8_t nwd_b(uint64_t a, uint8_t dflt) { uint8_t v = dflt; return nwd_rd(a, &v, 1) ? v : dflt; }

static void nwd_bind(void) {
    if (NWD.tried) return;
    NWD.tried = true;
    HMODULE exe = GetModuleHandleA(NULL);
    const SwApi *(*get)(uint32_t) = (const SwApi *(*)(uint32_t))(void *)GetProcAddress(exe, SW_API_EXPORT);
    const SwApi *a = NWD.api = get ? get(1) : NULL;
    NWD.in_event = (bool (*)(void))(void *)GetProcAddress(exe, "framepace_in_event");
    NWD.save_ev = (int (*)(uint64_t *, int *, int *, char *, int))(void *)GetProcAddress(exe, "fs_save_event");
    NWD.romfs_dir = (const char *const *)(void *)GetProcAddress(exe, "g_romfs_dir");
    NWD.data_root = (const char *(*)(void))(void *)GetProcAddress(exe, "rt_data_root");
    NWD.save_dir = (const char *const *)(void *)GetProcAddress(exe, "g_save_dir");
#ifdef SORA_WORLD_HOOKS  // (sora.c: sora_world.h)
    NWD.moons_fn = sora_world_bloodmoon_count;
#endif
    if (SW_HAS(a, site) && SW_HAS(a, call) && SW_HAS(a, read) && SW_HAS(a, in_frame) && SW_HAS(a, actor_name)) {
        NWD.s_gdt = a->site("gdt_mgr"), NWD.s_gdt_handle = a->site("loot_gdt_handle"), NWD.s_gdt_set = a->site("loot_gdt_set_bool");
        NWD.s_pause = a->site("pause_state"), NWD.s_pinfo = a->site("player_info"), NWD.s_ss_vt = a->site("safestring_vt_got");
        NWD.s_can_get = a->site("world_pouch_can_get"), NWD.s_get = a->site("world_pouch_get"), NWD.s_special = a->site("world_get_special");
        NWD.s_ev_start = a->site("world_evt_start"), NWD.s_ev_init = a->site("world_evt_meta_init");
        NWD.s_ev_name = a->site("world_evt_meta_name"), NWD.s_ev_free = a->site("world_evt_meta_free");
        NWD.s_korok_mgr = a->site("world_korok_evt_mgr");
    }
    NWD.sites = NWD.s_gdt && NWD.s_gdt_handle && NWD.s_gdt_set && NWD.s_pause && NWD.s_pinfo && NWD.s_ss_vt && NWD.s_can_get && NWD.s_get &&
                NWD.s_special && NWD.s_ev_start && NWD.s_ev_init && NWD.s_ev_name && NWD.s_ev_free && NWD.s_korok_mgr && NWD.romfs_dir && NWD.in_event;
}
static const char *nwd_sites_txt(void) {
    return NWD.sites ? "sites ok"
           : !NWD.api ? "this exe has no sw_api: networld does nothing"
           : !NWD.romfs_dir || !NWD.in_event ? "this exe exports no g_romfs_dir / framepace_in_event: networld does nothing"
           : !NWD.s_can_get || !NWD.s_korok_mgr ? "NO world_* sites rows: NEEDS EXE RELINK with the s290 networld sora_sites.h (game 1.9.0 only; "
                                                  "1.8.2 has no rows): networld does nothing"
           : !NWD.s_gdt_handle ? "NO loot_gdt_* rows: NEEDS EXE RELINK with the s289 loot sora_sites.h: networld does nothing"
                               : "a site is missing: networld does nothing";
}

// ---- the list, on a worker thread (file reads + unpacking: ~0.1-0.3 s, never on the game's frame) ----
static DWORD WINAPI nwd_list_job(LPVOID arg) {
    (void)arg;
    ULONGLONG t0 = GetTickCount64();
    char p1[800], p2[800];
    snprintf(p1, sizeof p1, "%s/Pack/Bootup.pack", NWD.job_path), snprintf(p2, sizeof p2, "%s/Pack/TitleBG.pack", NWD.job_path);
    size_t gn = 0, qn = 0;
    char w2[300] = "";
    uint8_t *g = nwd_read_packed(p1, "GameData/gamedata.ssarc", &gn, NWD.job_why, sizeof NWD.job_why);
    uint8_t *q = g ? nwd_read_packed(p2, "Quest/QuestProduct.sbquestpack", &qn, w2, sizeof w2) : NULL;
    int r = g ? nwd_list_build(g, gn, q, qn, &NWD.JL) : -1;
    if (g && r != NWDE_OK) snprintf(NWD.job_why, sizeof NWD.job_why, "%s", NWDE_TXT[r]);
    else if (g && !q) snprintf(NWD.job_why, sizeof NWD.job_why, "no quest steps (%s)", w2);
    free(g), free(q);
    NWD.job_ms = (uint32_t)(GetTickCount64() - t0);
    __atomic_store_n(&NWD.job, r == NWDE_OK ? 2 : 3, __ATOMIC_RELEASE);
    return 0;
}
static void nwd_list_start(void) {
    if (NWD.job || !NWD.romfs_dir || !*NWD.romfs_dir) return;
    const char *rd = *NWD.romfs_dir;
    char test[800];
    snprintf(test, sizeof test, "%s/Pack/Bootup.pack", rd);
    if (!nwd_exists(test) && NWD.data_root && NWD.data_root() && rd[0] != '/' && !(rd[0] && rd[1] == ':'))
        snprintf(NWD.job_path, sizeof NWD.job_path, "%s/%s", NWD.data_root(), rd);
    else
        snprintf(NWD.job_path, sizeof NWD.job_path, "%s", rd);
    NWD.job = 1;
    HANDLE h = CreateThread(NULL, 0, nwd_list_job, NULL, 0, NULL);
    if (!h) { NWD.job = 3, snprintf(NWD.job_why, sizeof NWD.job_why, "no worker thread"); return; }
    CloseHandle(h);
}
// the built list -> the union's state (once)
static void nwd_list_take(void) {
    if (NWD.ready || __atomic_load_n(&NWD.job, __ATOMIC_ACQUIRE) != 2) return;
    NWD.L = NWD.JL, memset(&NWD.JL, 0, sizeof NWD.JL);
    bool ok = nwd_core_init(&NWD.C, &NWD.L, g_net_world_fam);
    NWD.tmp = ok ? (uint8_t *)calloc(1, NWD.C.nb) : NULL;
    NWD.page_buf = ok ? (uint8_t *)calloc(1, NWD.C.nb) : NULL;
    NWD.lack = ok ? (uint32_t *)calloc(NWD_APPLY_MAX, sizeof *NWD.lack) : NULL;
    NWD.va = ok ? (uint64_t *)calloc(NWD.L.n, sizeof *NWD.va) : NULL;
    NWD.none = ok ? (uint8_t *)calloc(1, NWD.C.nb) : NULL;
    if (!ok || !NWD.tmp || !NWD.page_buf || !NWD.lack || !NWD.va || !NWD.none) {
        LOG("[networld] out of memory for the flag list: networld does nothing\n");
        __atomic_store_n(&NWD.job, 3, __ATOMIC_RELEASE);
        return;
    }
    NWD.ready = true, NWD.join_due = true;
    char fams[300] = "";
    for (int b = 0; b < NWDF_COUNT; b++)
        snprintf(fams + strlen(fams), sizeof fams - strlen(fams), "%s%s %u", b ? ", " : "", NWDF_TXT[b], NWD.L.fam_n[b]);
    LOG("[networld] flag list built from %s in %u ms: %u flags shared of %u bool flags (%s); quest names %u (bool %u), %u files, "
        "fingerprint %08x%s%s\n",
        NWD.job_path, NWD.job_ms, NWD.L.n, NWD.L.bool_n, fams, NWD.L.quest_n, NWD.L.quest_bool, NWD.L.gdt_files, NWD.L.fp,
        NWD.job_why[0] ? "; " : "", NWD.job_why);
}

// ---- reading our own flags (GameDataMgr's bool buffer: loot's lookup, cached) ----
static uint64_t nwd_gdt_buf(uint64_t *mgr_out) {
    uint64_t mgr = NWD.s_gdt ? nwd_q(NWD.s_gdt) : 0, pb = mgr ? nwd_q(mgr + 0xbd8) : 0;
    if (mgr_out) *mgr_out = mgr;
    return pb ? nwd_q(pb) : 0;
}
// a chunk of the pass: -> true when the pass is whole
static bool nwd_pass_step(void) {
    uint64_t buf = nwd_gdt_buf(NULL);
    if (!buf) return false;
    uint64_t arr = nwd_q(buf + 0x10);
    uint32_t cnt = nwd_d(buf + 8);
    if (buf != NWD.va_buf || arr != NWD.va_arr || cnt != NWD.va_cnt) {  // (another buffer: every address looked up again)
        if (NWD.va_buf) NWD.st.va_reset++;
        memset(NWD.va, 0, NWD.L.n * sizeof *NWD.va), memset(NWD.none, 0, NWD.C.nb);
        NWD.va_buf = buf, NWD.va_arr = arr, NWD.va_cnt = cnt, NWD.pos = 0;
    }
    int reads = 0, looks = 0;
    while (NWD.pos < NWD.L.n && reads < NWD_CHUNK && looks < NWD_RESOLVE) {
        uint32_t i = NWD.pos++;
        if (!NWD.va[i] && !nwd_bit(NWD.none, i)) {
            looks++, NWD.st.resolves++;
            uint64_t v = lt_gdt_find(nwd_rd, buf, NWD.L.e[i].hash);
            if (v) NWD.va[i] = v;
            else nwd_bset(NWD.none, i);
        }
        reads++;
        if (NWD.va[i] && (nwd_d(NWD.va[i]) & 1)) nwd_bset(NWD.C.mine, i);
        else nwd_bclr(NWD.C.mine, i);
    }
    return NWD.pos >= NWD.L.n;
}

// ---- guest calls (driven frames only: api->in_frame, c = NULL) ----
static bool nwd_call(uint64_t fn, const SwArgs *a, SwRet *r) {
    SwRet t;
    return fn && NWD.api->call(NULL, fn, a, r ? r : &t);
}
// the flag set the game's own way (loot's lt_flag_set by hash: EventFlagONAction's handle + setBool) -> 1 set, 2 was set, 0 none, -1 refused
static int nwd_flag_set(uint32_t h) {
    uint64_t mgr = 0, buf = nwd_gdt_buf(&mgr);
    if (!buf) return -1;
    uint64_t v = lt_gdt_find(nwd_rd, buf, h);
    if (!v) return 0;
    if (nwd_d(v) & 1) return 2;
    SwRet r = {0};
    SwArgs a1 = {.x = {buf, h}, .nx = 6};
    if (!nwd_call(NWD.s_gdt_handle, &a1, &r)) return -1;
    uint32_t handle = (uint32_t)r.x0 | nwd_d(mgr + 0xc28) << 24;
    if (handle == 0xffffffffu) return -1;
    SwArgs a2 = {.x = {mgr, 1, handle}, .nx = 6};
    return nwd_call(NWD.s_gdt_set, &a2, &r) && (r.x0 & 1) ? 1 : -1;
}
// a sead::SafeString {vtable + 0x10, the name} in the call's scratch, x[at] = its address
static bool nwd_ss_args(SwArgs *a, int at, const char *name, uint64_t ss[2]) {
    uint64_t vt = nwd_q(NWD.s_ss_vt);
    if (!vt || !name) return false;
    ss[0] = vt + 0x10, ss[1] = (uint64_t)(uintptr_t)name;
    a->x_scratch = (uint8_t)(1u << at), a->x[at] = 0, a->scratch = 32, a->init = ss, a->init_len = 16, a->nx = 6;
    return true;
}
// the pouch has room for it (PR give_item's can-get: 0x700120af80(name, count))
static bool nwd_pouch_fits(const char *name) {
    uint64_t ss[2];
    SwArgs a = {.x = {0, 1}};
    SwRet r = {0};
    return nwd_ss_args(&a, 0, name, ss) && nwd_call(NWD.s_can_get, &a, &r) && (r.x0 & 1);
}
static bool nwd_pouch_give(const char *name) {  // (PR give_item: 0x700120af98(name, count), as EventIncreasePorchItemAction does)
    uint64_t ss[2];
    SwArgs a = {.x = {0, 1}};
    return nwd_ss_args(&a, 0, name, ss) && nwd_call(NWD.s_get, &a, NULL);
}
static bool nwd_heart_give(void) {  // (PR give_heart: 0x7001211520(0, "Obj_HeartUtuwa_A_01"), the pouch alone doesn't raise the hearts)
    static const char name[] = "Obj_HeartUtuwa_A_01";
    uint64_t ss[2];
    SwArgs a = {.x = {0, 0}};
    return nwd_ss_args(&a, 1, name, ss) && nwd_call(NWD.s_special, &a, NULL);
}
static uint64_t nwd_player(void) {
    uint64_t pi = nwd_q(NWD.s_pinfo);
    return pi ? nwd_q(pi + 0x60) : 0;  // (sora.c link_actor's chain, as loot's)
}
static int nwd_all_actors(uint64_t from, uint64_t *out, int max) {  // (loot's lt_all_actors, PR all_actors)
    uint64_t vt = nwd_q(from);
    if (!vt) return 0;
    int n = 0;
    for (int dir = 0; dir < 2; dir++) {
        uint64_t u = from;
        for (int k = 0; k < 16384 && n < max; k++) {
            if (dir == 1 || k > 0) {
                uint64_t a = nwd_q(u + 0x48);
                if (a && nwd_d(a + 0x60) == nwd_d(u + 0x40)) out[n++] = a;
            }
            uint64_t nx = nwd_q(u + (dir ? 0x10 : 0x18));
            if (!nx || nx - 0x10 == from) break;
            u = nx - 0x10;
            if (nwd_q(u) != vt) break;
        }
    }
    return n;
}
// the Koroks standing within NWD_KOROK_R of Link now (PR korok_event's search, once a pass for all the Korok flags): -> how many
static int nwd_korok_scan(uint32_t *ids, uint64_t *acts, bool *flys, int max) {
    uint64_t player = nwd_player();
    float pm[12];
    if (!player || !nwd_rd(player + 0x398, pm, sizeof pm)) return 0;
    int na = nwd_all_actors(nwd_q(player + 0x68), NWD.actors, NWD_ACTORS), n = 0;
    for (int k = 0; k < na && n < max; k++) {
        uint64_t a = NWD.actors[k];
        const char *nm = NWD.api->actor_name(a);
        if (!nm || strncmp(nm, "Npc_HiddenKorok", 15) || nwd_b(a + 0x64, 3) == 3) continue;  // (3 = being deleted: loot's rule)
        bool fly = !strcmp(nm, "Npc_HiddenKorokFly");
        if (!fly && strcmp(nm, "Npc_HiddenKorokGround")) continue;
        float m[12];
        if (!nwd_rd(a + 0x398, m, sizeof m)) continue;
        float dx = m[3] - pm[3], dy = m[7] - pm[7], dz = m[11] - pm[11];
        if (dx * dx + dy * dy + dz * dz > NWD_KOROK_R * NWD_KOROK_R) continue;
        uint64_t mo = nwd_q(a + 0x7c8);
        uint32_t h1 = mo ? nwd_d(mo + 0x58) : 0, h2 = nwd_d(a + 0x640);  // (ours: linkpos actor_hid's path; the PR's +0x640)
        if (h1 && h1 != 0xffffffffu) ids[n] = h1, acts[n] = a, flys[n] = fly, n++;
        if (n < max && h2 && h2 != h1) ids[n] = h2, acts[n] = a, flys[n] = fly, n++;
    }
    return n;
}
// a Korok a friend found, when its Korok is here: its own cutscene Demo017_0<HiddenKorok_Ground / _Air> on it (PR korok_event: it
// shows, gives the seed and sets the flag) -> true = started
static bool nwd_korok_play(const char *flag, uint64_t actor, bool fly) {
    uint64_t mgr = nwd_q(NWD.s_korok_mgr);
    if (!mgr || !actor) return false;
    static uint8_t meta[0x200] __attribute__((aligned(16)));  // (the event's metadata, 0x198 bytes: init, named, used, freed here)
    memset(meta, 0, sizeof meta);
    SwArgs a = {.x = {(uint64_t)(uintptr_t)meta}, .nx = 6};
    if (!nwd_call(NWD.s_ev_init, &a, NULL)) return false;
    SwArgs b = {.x = {(uint64_t)(uintptr_t)meta, (uint64_t)(uintptr_t)"Demo017_0", (uint64_t)(uintptr_t)(fly ? "HiddenKorok_Air" : "HiddenKorok_Ground"),
                      (uint64_t)(uintptr_t)""},
                .nx = 6};
    nwd_call(NWD.s_ev_name, &b, NULL);
    SwRet r = {0};
    SwArgs c = {.x = {mgr, (uint64_t)(uintptr_t)meta, actor, 0}, .nx = 6};
    bool ok = nwd_call(NWD.s_ev_start, &c, &r) && (r.x0 & 1);
    nwd_call(NWD.s_ev_free, &a, NULL);
    if (g_net_world_log) LOG("[networld] %s: its Korok is here (within %.0f m): its own cutscene Demo017_0 %s\n", flag, NWD_KOROK_R, ok ? "started" : "REFUSED");
    return ok;
}

// ---- the wire out (the room's reliable events; Melty only) ----
static bool nwd_anyone(void) {
    if (g_net_loop || !NET.on || !NET.melty || !sora_melty_connected()) return false;
    for (int k = 0; k < NET_MAX_PEERS; k++)
        if (NET.slot[k].used && NET.slot[k].p.st == PEER_LIVE) return true;
    return false;
}
static void nwd_send(const uint8_t *b, int n) {
    if (n <= 0) { NWD.st.unsent++; return; }
    NetAddr d[NET_DESTS_MAX];
    int nd = net_dests(d, false);
    net_send_all(d, nd, b, n);
}
static void nwd_send_state(bool on, bool ask) {
    if (!NWD.ready) return;
    uint8_t b[NET_WORLD_STATE_LEN];
    nwd_send(b, net_pack_world_state(NET.me, NWD.seq++, on, ask, NWD.L.fp, NWD.L.n, (uint16_t)g_net_world_fam, b, (int)sizeof b));
    NWD.st.state_tx++;
}
static void nwd_send_pages(void) {
    if (!NWD.ready || !NWD.C.passed) return;
    nwd_core_ours(&NWD.C, NWD.page_buf);
    NWD.gen++;
    int pages = net_world_pages(NWD.L.n);
    for (int p = 0; p < pages; p++) {
        uint8_t b[NET_WORLD_PAGE_HEAD + NET_WORLD_PAGE_BITS / 8];
        nwd_send(b, net_pack_world_page(NET.me, NWD.seq++, NWD.L.fp, NWD.gen, p, NWD.L.n, NWD.page_buf + (size_t)p * (NET_WORLD_PAGE_BITS / 8), b,
                                       (int)sizeof b));
        NWD.st.page_tx++;
    }
    if (g_net_world_log) LOG("[networld] our whole set to the room: %u flags (snapshot %u, %d pages)\n", nwd_popc(NWD.page_buf, NWD.C.nb), NWD.gen, pages);
}
static void nwd_send_sets(const uint16_t *idx, int n) {
    for (int at = 0; at < n; at += NET_WORLD_SET_MAX) {
        int c = n - at < NET_WORLD_SET_MAX ? n - at : NET_WORLD_SET_MAX;
        uint8_t b[NET_WORLD_SET_HEAD + 2 * NET_WORLD_SET_MAX];
        nwd_send(b, net_pack_world_set(NET.me, NWD.seq++, NWD.L.fp, idx + at, c, b, (int)sizeof b));
        NWD.st.set_tx++;
    }
}

// ---- receive (sora_net.h net_tick's dispatch): k = the sender's slot ----
static void net_world_recv(int k, const uint8_t *buf, int n, uint64_t now) {
    (void)now;
    if (!g_net_world) { NWD.st.rx_off++; return; }
    static NetWorldMsg m;  // (1 KB: the receive loop runs on one thread)
    uint8_t pl = 0;
    uint16_t sq = 0;
    if (k < 0 || k >= NET_MAX_PEERS || !net_unpack_world(buf, n, &pl, &sq, &m)) { NWD.st.rx_bad++; return; }
    if (!NWD.ready) { NWD.st.rx_early++; return; }  // (our list is not built yet: we ask anew once it is)
    if (!net_world_fits(&m, NWD.L.fp, NWD.L.n)) {
        NWD.st.rx_list++;
        if (!NWD.said_list[k])
            NWD.said_list[k] = true,
            LOG("[networld] %s's flag list is not ours (fingerprint %08x, %u flags; ours %08x, %u): his world flags are refused (another "
                "game version or game data?)\n", NET.slot[k].p.name[0] ? NET.slot[k].p.name : "a friend", m.fp, m.count, NWD.L.fp, NWD.L.n);
        return;
    }
    if (m.op == NWO_STATE) {
        NWD.st.rx_state++;
        if (!m.on) {
            if (NWD.C.on[k] && g_net_world_log) LOG("[networld] %s turned networld off: his set is forgotten here\n", NET.slot[k].p.name);
            nwd_core_peer_reset(&NWD.C, k);
            return;
        }
        NWD.C.on[k] = true;
        if (m.ask) NWD.answer_due = true;
    } else if (m.op == NWO_PAGE) {
        NWD.st.rx_page++, NWD.C.on[k] = true;
        int r = nwd_core_page(&NWD.C, k, m.gen, m.page, m.pages, m.bits, m.nbytes);
        if (r < 0) NWD.st.rx_bad++;
        if (r == 1) {
            NWD.st.snaps++;
            if (g_net_world_log) LOG("[networld] %s's whole set came: %u flags (snapshot %u)\n", NET.slot[k].p.name, nwd_popc(NWD.C.known[k], NWD.C.nb), m.gen);
        }
    } else {
        NWD.st.rx_set++, NWD.C.on[k] = true;
        int got = nwd_core_set(&NWD.C, k, m.idx, m.n);
        NWD.st.set_new += (uint32_t)got;
        if (g_net_world_log && got) LOG("[networld] %s set %d flag(s) in his game (first: %s)\n", NET.slot[k].p.name, got, nwd_name(&NWD.L, m.idx[0]));
    }
}

// ---- the save guard ----
static DWORD WINAPI nwd_backup_job(LPVOID arg) {
    (void)arg;
    bool same = false, torn = false;
    NWD.bfiles = nwd_backup_run(NWD.bslot, NWD.bstamp, NWD.bpath, sizeof NWD.bpath, &NWD.bbytes, &same, &torn, NWD.bwhy, sizeof NWD.bwhy);
    NWD.bsame = same, NWD.btorn = torn;
    __atomic_store_n(&NWD.bjob, NWD.bfiles > 0 ? 2 : 3, __ATOMIC_RELEASE);
    return 0;
}
// the save events since the last look, into the gate (s290 revfix F1: a read of a slot the copy does not cover makes the union
// wait at once; F2: a write of the slot being copied makes the copy torn)
static void nwd_save_events(uint64_t now) {
    if (!NWD.save_ev) return;
    int slot = 0, kind = 0;
    char dir[600];
    while (NWD.save_ev(&NWD.ev_seq, &slot, &kind, dir, (int)sizeof dir)) {
        bool was = nwd_gate_ok(&NWD.G, NWD.room);
        nwd_gate_event(&NWD.G, kind, dir, now);
        if (was && !nwd_gate_ok(&NWD.G, NWD.room)) {
            NWD.st.rearms++;
            LOG("[networld] the game read another save (%s) than the one copied (%s): no union until that one is copied too\n", dir, NWD.G.run);
        }
    }
}
// old exe (no events): the newest game_data.sav under <save dir>/save_0/<0..9>
static bool nwd_newest_slot(char *out, size_t cap) {
    const char *sd = NWD.save_dir && *NWD.save_dir ? *NWD.save_dir : NULL;
    if (!sd) return false;
    long long best = -1;
    for (int s = 0; s < 10; s++) {
        char p[700];
        snprintf(p, sizeof p, "%s/save_0/%d/game_data.sav", sd, s);
        struct stat st;
        if (!stat(p, &st) && (long long)st.st_mtime > best) best = (long long)st.st_mtime, snprintf(out, cap, "%s/save_0/%d", sd, s);
    }
    return best >= 0;
}
static void nwd_gate_tick(uint64_t now) {
    int bj = __atomic_load_n(&NWD.bjob, __ATOMIC_ACQUIRE);  // (first: an event the game logs after the worker's end is drained below)
    nwd_save_events(now);
    if (NWD.G.st == NWDB_RUN && (bj == 2 || bj == 3)) {
        NWD.bjob = 0;
        nwd_gate_job(&NWD.G, bj == 3 ? NWDJ_FAIL : NWD.btorn ? NWDJ_TORN : NWDJ_OK, NWD.bsame, NWD.bpath, NWD.bwhy, now);
        if (bj == 3) {
            NWD.st.backup_fail++;
            LOG("[networld] the save copy FAILED (%s): no union until it is copied (%s)\n", NWD.bwhy,
                NWD.G.tries < NWD_COPY_TRIES ? "tried again in 30 s, into a new folder" : "given up in this room; a new room join tries again");
        }
    }
    bool friend_data = false;  // (a copy only once a friend's set came)
    for (int k = 0; k < NET_MAX_PEERS; k++) friend_data = friend_data || (NWD.C.on[k] && (NWD.C.have[k] || nwd_popc(NWD.C.known[k], NWD.C.nb)));
    static uint64_t next_guess;  // (old exe: no events, the newest by file time, as before; looked at once a second at most)
    if (!NWD.save_ev && friend_data && now >= next_guess && !nwd_gate_busy(&NWD.G) && !nwd_gate_ok(&NWD.G, NWD.room)) {
        char s[600];
        next_guess = now + 1000;
        if (nwd_newest_slot(s, sizeof s)) snprintf(NWD.G.cur, sizeof NWD.G.cur, "%s", s), NWD.G.cur_read = true;
    }
    int a = nwd_gate_step(&NWD.G, NWD.room, now, friend_data);
    if (a == NWDA_NOSAVE) {
        if (!NWD.said_save)
            NWD.said_save = true, LOG("[networld] no save read or written yet in this run: the union waits until the game loads one (never guessed)\n");
    } else if (a == NWDA_CLEAN) {
        if (NWD.G.same) {
            NWD.st.backup_same++;
            LOG("[networld] the save %s is the same as its copy %s: no new copy, the union goes on\n", NWD.G.run, NWD.G.path);
        } else {
            NWD.st.backups++;
            LOG("[networld] save copied before the union: %s -> %s (%d files, %lld bytes)\n", NWD.G.run, NWD.G.path, NWD.bfiles, NWD.bbytes);
        }
    } else if (a == NWDA_TORN) {
        NWD.st.backup_torn++;
        LOG("[networld] the game wrote %s during its copy %s: that copy is kept but may be torn; copied again into a new folder once the "
            "save is written (try %d of %d)\n", NWD.G.run, NWD.G.path[0] ? NWD.G.path : "?", NWD.G.tries + 1, NWD_COPY_TRIES);
    } else if (a == NWDA_GIVEUP) {
        NWD.st.backup_fail++;
        LOG("[networld] the save copy FAILED (%s, last %s): no union in this room (a new room join tries again)\n", NWD.G.why, NWD.G.path);
    } else if (a == NWDA_COPY) {
        SYSTEMTIME t;
        GetLocalTime(&t);
        snprintf(NWD.bstamp, sizeof NWD.bstamp, "%04u-%02u-%02u_%02u-%02u-%02u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
        snprintf(NWD.bslot, sizeof NWD.bslot, "%s", NWD.G.run);
        NWD.bjob = 1;
        HANDLE h = CreateThread(NULL, 0, nwd_backup_job, NULL, 0, NULL);
        if (!h) {
            NWD.bjob = 0, nwd_gate_job(&NWD.G, NWDJ_FAIL, false, NULL, "no worker thread", now), NWD.st.backup_fail++;
            LOG("[networld] the save copy FAILED (no worker thread): no union until it is copied (tried again in 30 s)\n");
            return;
        }
        CloseHandle(h);
        if (g_net_world_log) LOG("[networld] copying the save %s before the union in this room\n", NWD.bslot);
    }
}

// ---- the union here: what the friends have and we lack, set the game's own way + what comes with it ----
static void nwd_apply(uint64_t now) {
    nwd_save_events(now);  // (s290 revfix F1: a save read since the gate's look this tick counts before anything is set)
    if (!nwd_gate_ok(&NWD.G, NWD.room)) { NWD.st.waits_gate++; return; }
    if (NWD.in_event && NWD.in_event()) { NWD.st.waits_event++; return; }  // (never in a cutscene: a Korok's comes from it)
    uint32_t total = 0;
    int n = nwd_core_lack(&NWD.C, NWD.tmp, NWD.lack, NWD_APPLY_MAX, &total);
    int gifts = 0, nk = -1;  // (nk: the Koroks near Link, looked for at the first Korok flag of this pass)
    static uint32_t kid[32];
    static uint64_t kact[32];
    static bool kfly[32];
    int sl = nwd_index_of(&NWD.L, lt_crc32("IsPlayed_Demo700_0"));
    for (int j = 0; j < n; j++) {
        uint32_t i = NWD.lack[j];
        const char *name = nwd_name(&NWD.L, i);
        if (nwd_slate_group(name) && (sl < 0 || !nwd_core_union_has(&NWD.C, (uint32_t)sl))) { NWD.st.slate_wait++; continue; }
        const char *item = NULL;
        int g = nwd_gift(name, &item);
        if (g != NWDG_NONE && gifts >= NWD_GIFT_MAX) continue;  // (the next pass)
        uint32_t h = NWD.L.e[i].hash;
        bool played = false;
        for (int q = 0; g == NWDG_SEED && q < 16; q++) played |= NWD.korok_played[q] == h;
        if (g == NWDG_SEED && !played) {  // (PR: a Korok found by a friend, its Korok near here: its own cutscene (the seed, the flag))
            if (nk < 0) nk = nwd_korok_scan(kid, kact, kfly, 32);
            bool fly = false;
            uint32_t id = nwd_korok_id(name, &fly);
            int at = -1;
            for (int q = 0; q < nk && at < 0; q++)
                if (kid[q] == id && kfly[q] == fly) at = q;
            if (at >= 0) {
                if (nwd_korok_play(name, kact[at], fly)) {
                    NWD.korok_played[NWD.korok_np++ % 16] = h, NWD.st.koroks++;
                    break;  // (one cutscene: nothing else this pass, the next ones wait for it to end)
                }
                int q = 0;  // (its Korok here but the start refused: again next pass, 30 times at most, then the seed alone)
                while (q < 16 && NWD.wait_h[q] != h) q++;
                if (q == 16) q = (int)(h % 16), NWD.wait_h[q] = h, NWD.wait_n[q] = 0;
                NWD.st.korok_wait++;
                if (++NWD.wait_n[q] < 30) continue;
            }
        }
        if ((g == NWDG_ITEM || g == NWDG_SEED || g == NWDG_ORB) && !nwd_pouch_fits(item)) {  // (ours: the flag waits, PR: lost)
            NWD.st.pouch_full++;
            if (g_net_world_log) LOG("[networld] %s: %s does not fit the pouch: the flag waits\n", name, item);
            continue;
        }
        int r = nwd_flag_set(h);
        if (r == 1) {
            nwd_bset(NWD.C.applied, i), NWD.st.applied++;
            if (g == NWDG_HEART) nwd_heart_give(), NWD.st.hearts++, gifts++;
            else if (g != NWDG_NONE && item) {
                nwd_pouch_give(item), gifts++;
                if (g == NWDG_SEED) NWD.st.seeds++;
                else if (g == NWDG_ORB) NWD.st.orbs++;
                else NWD.st.items++;
            }
            NWD.st.gifts += g != NWDG_NONE;
            if (g_net_world_log)
                LOG("[networld] %s: set (a friend has it)%s%s%s\n", name, g == NWDG_HEART ? ", a heart container" : g != NWDG_NONE && item ? ", into the pouch: " : "",
                    g != NWDG_NONE && g != NWDG_HEART && item ? item : "", played ? " (its cutscene ran but did not set it)" : "");
        } else if (r == 2) {
            nwd_bset(NWD.C.applied, i), NWD.st.had++;  // (set meanwhile: nothing with it)
        } else {
            nwd_bset(NWD.C.failed, i);
            if (r == 0) NWD.st.none++;
            else NWD.st.refused++;
            if (g_net_world_log) LOG("[networld] %s: %s\n", name, r == 0 ? "not a flag of this game: left" : "NOT set (the setter refused): left for this session");
        }
    }
    if (g_net_world_log && total > (uint32_t)n) LOG("[networld] %u more flag(s) to set from the friends: the next passes\n", total - (uint32_t)n);
}

// every net_tick (after the receive and net_loot_tick): link_pos NULL = not driven (nothing looked at, nothing called)
static void net_world_tick(const float *link_pos, uint64_t now) {
    if (!g_net_world) return;
    nwd_bind();
    if (!NWD.said) NWD.said = true, LOG("[networld] networld 1: %s\n", nwd_sites_txt());
    if (!NWD.sites) return;
    if (!NET.melty) {
        if (!NWD.said_udp) NWD.said_udp = true, LOG("[networld] networld: Melty rooms only (this net session is not one): nothing shared\n");
        return;
    }
    nwd_list_start();
    if (!NWD.ready) {
        nwd_list_take();
        int j = __atomic_load_n(&NWD.job, __ATOMIC_ACQUIRE);
        static bool said_fail;
        if (j == 3 && !said_fail) said_fail = true, LOG("[networld] no flag list (%s, from %s): networld does nothing\n", NWD.job_why, NWD.job_path);
        if (!NWD.ready) return;
    }
    bool room = NET.melty && sora_melty_connected();
    if (room && !NWD.in_room) {
        NWD.room++, NWD.said_fail = false;
        if (g_net_world_log) LOG("[networld] a room join (%u): the save is copied before its first union\n", NWD.room);
    }
    NWD.in_room = room;
    for (int k = 0; k < NET_MAX_PEERS; k++) {  // (someone new in a slot: his set is not known; ours goes out to him)
        uint64_t j = NET.slot[k].used && NET.slot[k].p.st == PEER_LIVE ? NET.slot[k].joined_ms + 1 : 0;
        if (j != NWD.joined[k]) {
            nwd_core_peer_reset(&NWD.C, k), NWD.said_list[k] = false;
            NWD.join_due = NWD.join_due || j, NWD.joined[k] = j;
        }
    }
    bool share = nwd_anyone();
    if (share && NWD.C.passed && NWD.join_due) NWD.join_due = false, nwd_send_state(true, true), nwd_send_pages();
    if (share && NWD.C.passed && NWD.answer_due) NWD.answer_due = false, nwd_send_state(true, false), nwd_send_pages();  // (no ask: no ping-pong)
    nwd_gate_tick(now);
    if (NWD.moons_fn && now >= NWD.next_moon) {  // (a Blood Moon here: the room clock's count)
        NWD.next_moon = now + 2000;
        int mo = NWD.moons_fn();
        if (mo >= 0) {
            if (NWD.moons_known && mo > NWD.moons) {  // (our flags as read so far are from before it: held after a whole look after it)
                if (!NWD.moon_pending) NWD.st.moons++, LOG("[networld] a Blood Moon here: nothing set from the friends until the next whole look at our flags\n");
                NWD.moon_pending = true, NWD.moon_at = now;
            }
            NWD.moons = mo, NWD.moons_known = true;
        }
    }
    // our own flags: a pass every NWD_PASS_MS, in chunks over driven frames
    if (!link_pos || NET.loading) return;
    uint32_t ps = 0;
    if (nwd_rd(NWD.s_pause, &ps, 4) && ps != 0) { NWD.st.paused++; return; }  // (0 playing, 2 the pause menu / map)
    if (!NWD.api->in_frame()) { NWD.st.not_frame++; return; }
    if (!NWD.in_pass) {
        if (now < NWD.next_pass) return;
        NWD.in_pass = true, NWD.pos = 0, NWD.pass_start = now;
    }
    LARGE_INTEGER t0, t1, fq;
    QueryPerformanceCounter(&t0);
    if (nwd_pass_step()) {
        NWD.in_pass = false, NWD.next_pass = now + NWD_PASS_MS, NWD.st.passes++;
        static uint16_t own[NET_WORLD_SET_MAX * 8];
        int total = 0;
        uint32_t cleared = 0;
        bool first = !NWD.C.passed;
        int no = nwd_core_pass_done(&NWD.C, own, (int)(sizeof own / sizeof *own), &total, &cleared);
        if (cleared >= 64) {  // (PR: a Blood Moon here, many revival flags cleared at once; the count above may come later)
            if (!NWD.moon_pending) NWD.st.moons++, LOG("[networld] %u of our world flags cleared at once (a Blood Moon here)\n", cleared);
            NWD.moon_pending = true, NWD.moon_at = now;  // (this pass may have read some before it: one more whole look)
        }
        if (NWD.moon_pending) {  // (the friends' revival flags we lack now are from before it: held, not set here again)
            uint32_t h = nwd_core_moon(&NWD.C, NWD.tmp);
            NWD.st.held += h, NWD.moon_held += h;
            if (NWD.pass_start > NWD.moon_at) {  // (a whole look after it: done; our fresh set to the friends)
                NWD.moon_pending = false, NWD.pages_due = true;
                LOG("[networld] after the Blood Moon: %u of the friends' world flags from before it are held (not set here again until "
                    "they leave the friends' sets)\n", NWD.moon_held);
                NWD.moon_held = 0;
            }
        }
        uint32_t rel = nwd_core_release(&NWD.C, NWD.tmp);
        NWD.st.released += rel;
        if (rel && g_net_world_log) LOG("[networld] %u held flag(s) left every friend's set: free again\n", rel);
        if (first && g_net_world_log) LOG("[networld] first look at our own flags: %u of %u set\n", nwd_popc(NWD.C.mine, NWD.C.nb), NWD.L.n);
        if (share) {
            if (NWD.pages_due || total > no) NWD.pages_due = false, nwd_send_pages();  // (many at once: the whole set instead)
            else if (no) {
                nwd_send_sets(own, no), NWD.st.own_tx += (uint32_t)no;
                if (g_net_world_log) LOG("[networld] %d flag(s) set by us since the last look, to the room (first: %s)\n", no, nwd_name(&NWD.L, own[0]));
            }
            if (NWD.moon_pending) NWD.st.waits_moon++;
            else nwd_apply(now);
        }
    }
    QueryPerformanceCounter(&t1);
    QueryPerformanceFrequency(&fq);
    uint32_t us = fq.QuadPart > 0 ? (uint32_t)((t1.QuadPart - t0.QuadPart) * 1000000 / fq.QuadPart) : 0;
    NWD.cost_last = us, NWD.cost_max = us > NWD.cost_max ? us : NWD.cost_max;
}

// net on: a fresh seq, no friends' sets (the list and our own flags stay)
static void net_world_reset(uint64_t now) {
    NWD.seq = (uint16_t)(now * 2654435761u >> 11);
    memset(NWD.joined, 0, sizeof NWD.joined), memset(NWD.said_list, 0, sizeof NWD.said_list);
    for (int k = 0; k < NWD_PEERS; k++) nwd_core_peer_reset(&NWD.C, k);
    NWD.said_udp = false, NWD.answer_due = false, NWD.join_due = true;
}

static bool net_world_live_set(const char *key, int v) {
    if (!strcmp(key, "networld")) {
        bool on = v != 0;
        if (on == g_net_world) return true;  // (a live rewrite re-applies every line: only a change counts)
        if (!on && NWD.ready && nwd_anyone()) nwd_send_state(false, false);  // (the friends forget our set)
        g_net_world = on;
        if (!on) {
            for (int k = 0; k < NWD_PEERS; k++) nwd_core_peer_reset(&NWD.C, k);
            NWD.answer_due = false;
        } else {
            NWD.said = false, NWD.join_due = true;
            if (NWD.G.st == NWDB_FAIL) NWD.G.st = NWDB_NONE, NWD.G.done_room = 0, NWD.G.tries = 0;  // (on again after a failed copy: it tries again)
        }
        nwd_bind();
        LOG("[networld] networld %d: %s (%s)\n", on, on ? "the room shares world flags: chests, Koroks, shrines, towers, runes, Divine Beasts, items taken, quests, "
                                                        "shop sold-outs (Melty rooms; the save is copied before the first union)"
                                                     : "off: every game keeps its own world (today)",
            nwd_sites_txt());
        return true;
    }
    if (!strcmp(key, "networldlog")) { g_net_world_log = v != 0; return true; }
    if (!strcmp(key, "networldfam")) {
        int f = v & NWDF_ALL;
        if (f == g_net_world_fam) return true;
        g_net_world_fam = f;
        if (NWD.ready) nwd_core_fams(&NWD.C, &NWD.L, f), NWD.pages_due = true;
        char t[200] = "";
        for (int b = 0; b < NWDF_COUNT; b++)
            if (f >> b & 1) snprintf(t + strlen(t), sizeof t - strlen(t), "%s%s", t[0] ? " " : "", NWDF_TXT[b]);
        LOG("[networld] networldfam %d: %s\n", f, t[0] ? t : "none");
        return true;
    }
    return false;
}

static void net_world_diag(void) {
    int friends = 0, have = 0;
    uint32_t lack = 0;
    for (int k = 0; k < NWD_PEERS; k++) friends += NWD.C.on[k], have += NWD.C.have[k];
    if (NWD.ready) nwd_core_lack(&NWD.C, NWD.tmp, NWD.lack, 0, &lack);
    const char *gate = nwd_gate_busy(&NWD.G) ? "copying" : nwd_gate_ok(&NWD.G, NWD.room) ? "copied" : nwd_gate_failed(&NWD.G, NWD.room) ? "FAILED" : "not yet";
    LOG("[networld] %s, %s: list %s (%u flags, fp %08x, fams %d), ours set %u, friends sharing %d (whole sets %d), to set here %u, held %u; "
        "room %u, save copy %s%s%s (slot %s; copies %u, same %u, torn %u, failed %u, other save read %u); passes %u, a step %u us (max %u); out: states %u pages %u sets %u (own flags %u), unsent %u; in: states %u "
        "pages %u (whole %u) sets %u (new %u), off %u, bad %u, early %u, other list %u; set %u had %u none %u refused %u; gifts %u (items %u, "
        "seeds %u, orbs %u, hearts %u, Korok cutscenes %u, waits %u), pouch full %u, slate waits %u; Blood Moons %u, held %u, freed %u; "
        "waits: gate %u, Blood Moon %u, cutscene %u, paused %u, not in frame %u; look-ups %u (buffer changes %u)\n",
        g_net_world ? "ON" : "off (set networld 1)", NWD.tried ? nwd_sites_txt() : "not bound yet",
        NWD.ready ? "built" : NWD.job == 1 ? "building" : NWD.job == 3 ? "FAILED" : "not built", NWD.L.n, NWD.L.fp, g_net_world_fam,
        NWD.ready ? nwd_popc(NWD.C.mine, NWD.C.nb) : 0, friends, have, lack, NWD.ready ? nwd_popc(NWD.C.held, NWD.C.nb) : 0, NWD.room, gate,
        NWD.G.path[0] ? " to " : NWD.G.why[0] ? ": " : "", NWD.G.path[0] ? NWD.G.path : NWD.G.why, NWD.G.cur[0] ? NWD.G.cur : "none known",
        NWD.st.backups, NWD.st.backup_same, NWD.st.backup_torn, NWD.st.backup_fail, NWD.st.rearms, NWD.st.passes, NWD.cost_last, NWD.cost_max,
        NWD.st.state_tx, NWD.st.page_tx, NWD.st.set_tx, NWD.st.own_tx, NWD.st.unsent, NWD.st.rx_state, NWD.st.rx_page, NWD.st.snaps, NWD.st.rx_set,
        NWD.st.set_new, NWD.st.rx_off, NWD.st.rx_bad, NWD.st.rx_early, NWD.st.rx_list, NWD.st.applied, NWD.st.had, NWD.st.none, NWD.st.refused,
        NWD.st.gifts, NWD.st.items, NWD.st.seeds, NWD.st.orbs, NWD.st.hearts, NWD.st.koroks, NWD.st.korok_wait, NWD.st.pouch_full,
        NWD.st.slate_wait, NWD.st.moons, NWD.st.held, NWD.st.released, NWD.st.waits_gate, NWD.st.waits_moon, NWD.st.waits_event, NWD.st.paused, NWD.st.not_frame,
        NWD.st.resolves, NWD.st.va_reset);
}
#endif  // NET_WORLD_GLUE
