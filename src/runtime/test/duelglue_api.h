// (s291 dueltests) the API of one stubbed game for duelglue_test.c: runtime/test/duelglue_game.h is compiled twice (duelglue_game_a.c,
// duelglue_game_b.c), so two games with their OWN sora_duel.h state (all of it static) can trade real NET_DUEL packets in one test.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "sora_net_proto.h"

typedef struct { uint8_t b[64]; int n; uint32_t ip; uint16_t port; } DgPkt;  // one packet this game sent
typedef struct {
    int phase, role, result, reason, k, prompt, prompt_k;
    uint32_t id;
    int nblocked, nhits_taken, nguard_block, nad_block;
    int life;
    char banner[64];
    int others_on, fx_live, nfx_spawn, nfx_kill;
    bool live;
    bool my_time, his_time;
    uint16_t my_pm, his_pm;
} DgView;
enum { DGB_GUARD, DGB_REFLECT, DGB_INV, DGB_RCINV, DGB_ADODGE, DGB_LIMIT, DGB_N };  // the block reasons du_take_hit reads
typedef struct {
    const char *name;
    void (*reset)(uint8_t me);                    // a fresh game: duel 1, our player id, Sora at 0 0 0 facing +z, life 40 / 48
    void (*peer)(int k, uint8_t player, const char *name, const float feet[3], int st, bool hidden, uint16_t flags, int hp_pct, uint64_t now);
    void (*no_pose)(int k);                       // slot k keeps no SNAP (peer_sample: no valid pose)
    void (*body)(float x, float y, float z);
    void (*life)(int v);
    void (*pause)(bool on);                       // PAUSE_STATE != 0
    void (*block)(int what, bool on);             // DGB_*
    bool (*live_key)(const char *key, int v);     // du_live_key ("set <key> <v>")
    void (*ask)(int k, uint64_t now);             // du_ask (Triangle on "Duel" / live "duel ask")
    void (*accept)(uint64_t now);                 // du_accept (Triangle on "Accept Duel")
    void (*on_msg)(int k, const NetDuelMsg *m, uint64_t now);  // du_on_msg (as du_frame's pop)
    void (*recv)(int k, const uint8_t *b, int n, uint64_t now);  // net_duel_recv (the wire, queued for the next frame)
    void (*frame)(uint64_t now);                  // du_frame
    void (*input)(uint64_t *press, uint64_t *held, bool can);  // du_input (its clock is GetTickCount64)
    bool (*spell)(int kind, int power, const float c[3], float r, float lo, float hi);  // du_spell (GetTickCount64)
    void (*idle_long)(void);                      // DU.idle_since = 1: stood still long enough for the "Duel" prompt
    DgView (*view)(void);
    int (*ntx)(void);
    const DgPkt *(*tx)(int i);
    void (*tx_clear)(void);
    const char *(*log)(void);
    void (*log_clear)(void);
} DgGame;
extern const DgGame DGA, DGB;
