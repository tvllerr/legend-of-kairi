// (s54 levels) KH2 levels for Sora in BotW, slice 1: EXP, levels, KH-relative damage. DLL-only (hot reload).
// Design: D:/SoraBotW/recon/S54_HELPER_LV_LEVELS.md (sec 2 EXP, 3.3 kill rule, 6.1a damage, 7 slice 1); status + live
// checks: D:/SoraBotW/recon/S54_HELPER_LV_SLICE1.md. Tables: the GENERATED block below (from data/levels/*.json by
// recon/wip_s54_levels/gen_sora_levels_data.py; one self-contained header, nothing else to copy next to sora.c).
//
// Hooked into runtime/sora.c by recon/wip_s54_levels/sora_hooks.diff (4 small hunks):
//   #include "sora_levels.h"       after the linkpos / framepace prototypes (needs LOG, linkpos_enemies,
//                                  framepace_game_ratio declared before it)
//   sl_hit(...)                    replaces the blade's and the spells' damage rule (blade_hits, spell_hit); it also
//                                  credits the hit for the kill rule
//   sl_tick()                      once per driven, unpaused frame (sora_mod_frame, after snap_tick)
//   sl_live_set(key, v)            live "set lv N | exp N | lvdmg 0/1 | lvlog 0/1 | khdmg <x100> | lvwin <s>"
//
// Level = the highest KH2 lvup row whose EXP the total reaches; Str / Mag / Def / AP are that row's absolute values
// (VERIFIED table). Only Str / Mag change something in this slice (damage); Def / AP / abilities are logged.
// Kill (sec 3.3): an enemy Sora hit goes life > 0 -> 0 within SL.win s (10) of his last queued hit (keyed by actor
// pointer; BotW causes after his hit - fire, a fall, a bomb - count). EXP per enemy by NAME: the DLL reads the actor's
// name itself (BaseProc +0x10, the same read as linkpos.c actor_name) -> no exe getter needed for slice 1.
// Env (start values, only when no hot-reload state exists): SAVAGEWIND_SORA_EXP=<n> | SAVAGEWIND_SORA_LV=<1-99>,
// SAVAGEWIND_SORA_LVDMG=0/1.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sora_win.h"  // (<windows.h> on Windows; Linux shims: Deck port)
#include "sora_abil.h"  // (s82 slice 3: abilities / AP, pure)

// Dream Weapon (another helper builds the picker at the start). 0 sword, 1 shield, 2 staff = the KH2 lvup ability
// column (PHH: +8 + 2 x choice). dream_giveup: -1 not picked; it has NO effect here: KH2's lvup rows carry only the three
// "kept" columns (VERIFIED data), so the given-up weapon never selects anything. The picker just assigns these; if it
// defines them itself, it #defines SORA_HAVE_DREAM_WEAPON before including this header.
#ifndef SORA_HAVE_DREAM_WEAPON
static int dream_weapon = 0, dream_giveup = -1;
#endif

// ---- BEGIN GENERATED (gen_sora_levels_data.py) ----
// Do not edit by hand: python -I D:/SoraBotW/recon/wip_s54_levels/gen_sora_levels_data.py
// SL_LV[L-1] = KH2 lvup Sora row for level L (VERIFIED, kh2_sora_levels.json): EXP to reach L, absolute Str / Mag /
// Def / AP, the ability learned at L per Dream Weapon (0 sword, 1 shield, 2 staff; 0 = none).
typedef struct { uint32_t exp; uint8_t str, mag, def, ap; uint16_t ab[3]; } SlLevel;
static const SlLevel SL_LV[99] = {
    {0, 2, 6, 2, 0, {0, 0, 0}},  // LV1
    {40, 2, 6, 3, 0, {0, 0, 0}},  // LV2
    {100, 4, 6, 3, 2, {0, 0, 0}},  // LV3
    {184, 4, 6, 4, 2, {0, 0, 0}},  // LV4
    {296, 4, 6, 5, 4, {0, 0, 0}},  // LV5
    {440, 6, 6, 6, 4, {0, 0, 0}},  // LV6
    {620, 6, 6, 7, 4, {390, 411, 401}},  // LV7
    {840, 6, 8, 7, 6, {0, 0, 0}},  // LV8
    {1128, 6, 8, 8, 6, {401, 390, 411}},  // LV9
    {1492, 8, 8, 8, 6, {0, 0, 0}},  // LV10
    {1940, 8, 8, 9, 8, {0, 0, 0}},  // LV11
    {2480, 8, 10, 9, 8, {403, 403, 403}},  // LV12
    {3120, 8, 10, 10, 10, {0, 0, 0}},  // LV13
    {3902, 10, 10, 10, 10, {0, 0, 0}},  // LV14
    {4838, 10, 10, 11, 10, {392, 396, 408}},  // LV15
    {5940, 10, 12, 11, 12, {0, 0, 0}},  // LV16
    {7260, 10, 12, 12, 12, {411, 401, 390}},  // LV17
    {8814, 12, 14, 12, 12, {0, 0, 0}},  // LV18
    {10618, 12, 14, 13, 14, {0, 0, 0}},  // LV19
    {12688, 12, 16, 13, 14, {402, 402, 402}},  // LV20
    {15088, 12, 16, 14, 16, {0, 0, 0}},  // LV21
    {17838, 14, 16, 14, 16, {0, 0, 0}},  // LV22
    {20949, 14, 16, 15, 16, {408, 392, 396}},  // LV23
    {24433, 14, 18, 15, 18, {0, 0, 0}},  // LV24
    {28302, 14, 18, 16, 18, {397, 416, 405}},  // LV25
    {32622, 16, 18, 16, 18, {0, 0, 0}},  // LV26
    {37407, 16, 18, 17, 18, {0, 0, 0}},  // LV27
    {42671, 16, 20, 17, 18, {405, 397, 416}},  // LV28
    {48485, 16, 20, 18, 20, {0, 0, 0}},  // LV29
    {54865, 18, 20, 18, 20, {0, 0, 0}},  // LV30
    {61886, 18, 20, 19, 20, {400, 414, 409}},  // LV31
    {69566, 18, 22, 19, 20, {0, 0, 0}},  // LV32
    {77984, 18, 22, 20, 20, {396, 408, 392}},  // LV33
    {87160, 20, 22, 20, 20, {0, 0, 0}},  // LV34
    {97177, 20, 22, 21, 22, {0, 0, 0}},  // LV35
    {108057, 20, 24, 21, 22, {391, 406, 394}},  // LV36
    {119887, 20, 24, 22, 24, {0, 0, 0}},  // LV37
    {132691, 22, 24, 22, 24, {0, 0, 0}},  // LV38
    {146560, 22, 24, 23, 24, {409, 400, 414}},  // LV39
    {161520, 22, 26, 23, 26, {0, 0, 0}},  // LV40
    {177666, 22, 26, 24, 26, {540, 542, 410}},  // LV41
    {195026, 24, 26, 24, 26, {0, 0, 0}},  // LV42
    {213699, 24, 26, 25, 26, {0, 0, 0}},  // LV43
    {233715, 24, 28, 25, 26, {394, 391, 406}},  // LV44
    {255177, 24, 28, 26, 28, {0, 0, 0}},  // LV45
    {278117, 26, 28, 26, 28, {0, 0, 0}},  // LV46
    {302642, 26, 28, 27, 28, {416, 405, 397}},  // LV47
    {328786, 26, 30, 27, 28, {0, 0, 0}},  // LV48
    {356660, 26, 30, 28, 28, {393, 415, 395}},  // LV49
    {386378, 28, 30, 28, 28, {0, 0, 0}},  // LV50
    {417978, 28, 30, 29, 30, {0, 0, 0}},  // LV51
    {450378, 28, 32, 29, 30, {0, 0, 0}},  // LV52
    {483578, 28, 32, 30, 30, {410, 540, 542}},  // LV53
    {517578, 30, 32, 30, 30, {0, 0, 0}},  // LV54
    {552378, 30, 32, 31, 32, {0, 0, 0}},  // LV55
    {587978, 30, 34, 31, 32, {0, 0, 0}},  // LV56
    {624378, 30, 34, 32, 34, {0, 0, 0}},  // LV57
    {661578, 32, 34, 32, 34, {0, 0, 0}},  // LV58
    {699578, 32, 34, 33, 34, {414, 409, 400}},  // LV59
    {738378, 32, 36, 33, 34, {0, 0, 0}},  // LV60
    {777978, 32, 36, 34, 36, {0, 0, 0}},  // LV61
    {818378, 34, 36, 34, 36, {0, 0, 0}},  // LV62
    {859578, 34, 36, 35, 38, {0, 0, 0}},  // LV63
    {901578, 34, 38, 35, 38, {0, 0, 0}},  // LV64
    {944378, 34, 38, 36, 38, {395, 393, 415}},  // LV65
    {987978, 36, 38, 36, 38, {0, 0, 0}},  // LV66
    {1032378, 36, 38, 37, 40, {0, 0, 0}},  // LV67
    {1077578, 36, 40, 37, 40, {0, 0, 0}},  // LV68
    {1123578, 36, 40, 38, 42, {0, 0, 0}},  // LV69
    {1170378, 38, 40, 38, 42, {0, 0, 0}},  // LV70
    {1217978, 38, 40, 39, 44, {0, 0, 0}},  // LV71
    {1266378, 38, 42, 39, 44, {0, 0, 0}},  // LV72
    {1315578, 38, 42, 40, 44, {406, 394, 391}},  // LV73
    {1365578, 40, 42, 40, 44, {0, 0, 0}},  // LV74
    {1416378, 40, 42, 41, 46, {0, 0, 0}},  // LV75
    {1467978, 40, 44, 41, 46, {0, 0, 0}},  // LV76
    {1520378, 40, 44, 42, 48, {0, 0, 0}},  // LV77
    {1573578, 42, 44, 42, 48, {0, 0, 0}},  // LV78
    {1627578, 42, 44, 43, 50, {0, 0, 0}},  // LV79
    {1682378, 42, 46, 43, 50, {0, 0, 0}},  // LV80
    {1737978, 42, 46, 44, 52, {0, 0, 0}},  // LV81
    {1794378, 44, 46, 44, 52, {0, 0, 0}},  // LV82
    {1851578, 44, 46, 45, 54, {0, 0, 0}},  // LV83
    {1909578, 44, 48, 45, 54, {0, 0, 0}},  // LV84
    {1968378, 44, 48, 46, 54, {415, 395, 393}},  // LV85
    {2027978, 46, 48, 46, 54, {0, 0, 0}},  // LV86
    {2088378, 46, 48, 47, 56, {0, 0, 0}},  // LV87
    {2149578, 46, 50, 47, 56, {0, 0, 0}},  // LV88
    {2211578, 46, 50, 48, 58, {0, 0, 0}},  // LV89
    {2274378, 48, 50, 48, 58, {0, 0, 0}},  // LV90
    {2337978, 48, 50, 49, 60, {0, 0, 0}},  // LV91
    {2402378, 48, 52, 49, 60, {0, 0, 0}},  // LV92
    {2467578, 48, 52, 50, 62, {0, 0, 0}},  // LV93
    {2533578, 50, 52, 50, 62, {0, 0, 0}},  // LV94
    {2600378, 50, 52, 51, 64, {0, 0, 0}},  // LV95
    {2667978, 50, 54, 51, 64, {0, 0, 0}},  // LV96
    {2736378, 50, 54, 52, 66, {0, 0, 0}},  // LV97
    {2805578, 52, 54, 52, 66, {0, 0, 0}},  // LV98
    {2875578, 52, 54, 53, 66, {542, 410, 540}},  // LV99
};
// SL_LVPM[L-1] = KH2 lvpm enemy level L (VERIFIED): hp_mul (the enemy HP base), exp (EXP base)
typedef struct { uint16_t hp_mul, exp; } SlLvpm;
static const SlLvpm SL_LVPM[99] = {
    {20, 4}, {21, 5}, {23, 6}, {24, 7}, {26, 8}, {28, 9}, {29, 10}, {31, 12}, {33, 14}, {34, 16},
    {36, 18}, {38, 20}, {39, 23}, {41, 26}, {43, 29}, {44, 33}, {46, 37}, {48, 41}, {49, 45}, {51, 50},
    {53, 55}, {54, 61}, {56, 67}, {57, 73}, {59, 80}, {61, 87}, {62, 94}, {64, 102}, {66, 110}, {67, 119},
    {69, 128}, {71, 138}, {72, 148}, {74, 159}, {76, 170}, {77, 182}, {79, 194}, {81, 207}, {82, 220}, {84, 234},
    {86, 248}, {87, 263}, {89, 278}, {90, 294}, {92, 310}, {94, 327}, {95, 344}, {97, 362}, {99, 381}, {100, 400},
    {102, 400}, {104, 400}, {105, 400}, {107, 400}, {109, 400}, {110, 400}, {112, 400}, {114, 400}, {115, 400}, {117, 400},
    {119, 400}, {120, 400}, {122, 400}, {123, 400}, {125, 400}, {127, 400}, {128, 400}, {130, 400}, {132, 400}, {133, 400},
    {135, 400}, {137, 400}, {138, 400}, {140, 400}, {142, 400}, {143, 400}, {145, 400}, {147, 400}, {148, 400}, {150, 400},
    {152, 400}, {153, 400}, {155, 400}, {156, 400}, {158, 400}, {160, 400}, {161, 400}, {163, 400}, {165, 400}, {166, 400},
    {168, 400}, {170, 400}, {171, 400}, {173, 400}, {175, 400}, {176, 400}, {178, 400}, {180, 400}, {181, 400},
};
// (s109) SL_LVPM2[L-1] = KH2 lvpm enemy level L {Strength, Defense, Max Strength, Min Strength} (00battle.bin@0x2B788, VERIFIED)
typedef struct { uint8_t str, def, max_str, min_str; } SlLvpm2;
static const SlLvpm2 SL_LVPM2[99] = {
    {5, 2, 3, 1}, {6, 2, 3, 1}, {7, 3, 3, 1}, {7, 3, 4, 1}, {9, 4, 4, 1}, {9, 4, 4, 1}, {10, 5, 4, 1}, {11, 5, 5, 1}, {12, 6, 5, 1}, {12, 6, 5, 1},
    {14, 7, 6, 1}, {14, 7, 6, 2}, {15, 8, 6, 2}, {16, 8, 6, 2}, {17, 9, 7, 2}, {17, 9, 7, 2}, {19, 10, 7, 2}, {19, 10, 8, 2}, {20, 11, 8, 2}, {21, 11, 8, 2},
    {22, 12, 8, 2}, {22, 12, 9, 2}, {24, 13, 9, 3}, {24, 13, 9, 3}, {25, 14, 9, 3}, {26, 14, 10, 3}, {27, 15, 10, 3}, {27, 15, 10, 3}, {29, 16, 11, 3}, {29, 16, 11, 3},
    {30, 17, 11, 3}, {31, 17, 11, 3}, {32, 18, 12, 3}, {32, 18, 12, 4}, {34, 19, 12, 4}, {34, 19, 12, 4}, {35, 20, 13, 4}, {36, 20, 13, 4}, {37, 21, 13, 4}, {37, 21, 14, 4},
    {39, 22, 14, 4}, {39, 22, 14, 4}, {40, 23, 14, 4}, {40, 23, 15, 4}, {42, 24, 15, 4}, {42, 24, 15, 5}, {43, 25, 15, 5}, {44, 25, 16, 5}, {45, 26, 16, 5}, {45, 26, 16, 5},
    {47, 27, 17, 5}, {47, 27, 17, 5}, {48, 28, 17, 5}, {49, 28, 17, 5}, {50, 29, 18, 5}, {50, 29, 18, 5}, {52, 30, 18, 6}, {52, 30, 19, 6}, {53, 31, 19, 6}, {54, 31, 19, 6},
    {55, 32, 19, 6}, {55, 32, 20, 6}, {57, 33, 20, 6}, {57, 33, 20, 6}, {58, 34, 20, 6}, {59, 34, 21, 6}, {60, 35, 21, 6}, {60, 35, 21, 7}, {62, 36, 22, 7}, {62, 36, 22, 7},
    {63, 37, 22, 7}, {64, 37, 22, 7}, {65, 38, 23, 7}, {65, 38, 23, 7}, {67, 39, 23, 7}, {67, 39, 23, 7}, {68, 40, 24, 7}, {69, 40, 24, 7}, {70, 41, 24, 8}, {70, 41, 25, 8},
    {72, 42, 25, 8}, {72, 42, 25, 8}, {73, 43, 25, 8}, {73, 43, 26, 8}, {75, 44, 26, 8}, {75, 44, 26, 8}, {76, 45, 26, 8}, {77, 45, 27, 8}, {78, 46, 27, 8}, {78, 46, 27, 8},
    {80, 47, 28, 9}, {80, 47, 28, 9}, {81, 48, 28, 9}, {82, 48, 28, 9}, {83, 49, 29, 9}, {83, 49, 29, 9}, {85, 50, 29, 9}, {85, 50, 30, 9}, {86, 51, 30, 10},
};
static const struct { uint16_t id; const char *name; } SL_ABIL[] = {
    {390, "Combo Boost"},
    {391, "Air Combo Boost"},
    {392, "Reaction Boost"},
    {393, "Finishing Plus"},
    {394, "Negative Combo"},
    {395, "Berserk Charge"},
    {396, "Damage Drive"},
    {397, "Drive Boost"},
    {400, "Combination Boost"},
    {401, "Experience Boost"},
    {402, "Leaf Bracer"},
    {403, "Magic Lock-On"},
    {405, "Draw"},
    {406, "Jackpot"},
    {408, "Fire Boost"},
    {409, "Blizzard Boost"},
    {410, "Thunder Boost"},
    {411, "Item Boost"},
    {414, "Defender"},
    {415, "Second Chance"},
    {416, "Once More"},
    {540, "Drive Converter"},
    {542, "Damage Control"},
};
#define SL_ABIL_N ((int)(sizeof SL_ABIL / sizeof SL_ABIL[0]))
// SL_ENEMY: BotW actor -> {General.Life (VERIFIED packs), KH level + EXP percent + EXP (S54 design, GUESS),
// flags 1 boss (Get Bonus later, no EXP) / 2 Animal_* / 4 hostile animal (Enemy.Rank 1: wolves, bears)}.
// pct is also the KH HP percent for the damage rule (0 = boss / dragon: 200 there). Sorted by name.
typedef struct { const char *name; int32_t life; uint8_t kh_level, pct; uint16_t exp; uint8_t flags; } SlEnemy;
static const SlEnemy SL_ENEMY[] = {
    {"Animal_Bear_A", 250, 41, 100, 248, 6},
    {"Animal_Bear_B", 500, 46, 100, 327, 6},
    {"Animal_Boar_A", 8, 16, 100, 0, 2},
    {"Animal_Boar_B", 40, 28, 100, 0, 2},
    {"Animal_Boar_C", 8, 16, 100, 0, 2},
    {"Animal_Bull_A", 24, 24, 100, 0, 2},
    {"Animal_Cassowary_A", 8, 16, 100, 0, 2},
    {"Animal_Cow_A", 100, 34, 100, 0, 2},
    {"Animal_Crow_A", 1, 1, 100, 0, 2},
    {"Animal_Crow_Swarm", 1, 1, 100, 0, 2},
    {"Animal_Deer_A", 10, 18, 100, 0, 2},
    {"Animal_Deer_C", 10, 18, 100, 0, 2},
    {"Animal_Doe_A", 10, 18, 100, 0, 2},
    {"Animal_Dog_A", 100, 34, 100, 0, 2},
    {"Animal_Dog_B", 100, 34, 100, 0, 2},
    {"Animal_Dog_C", 100, 34, 100, 0, 2},
    {"Animal_Donkey_B", 100, 34, 100, 0, 2},
    {"Animal_Donkey_C", 100, 34, 100, 0, 2},
    {"Animal_Elk_A", 30, 25, 100, 0, 2},
    {"Animal_Fish_A", 1, 1, 100, 0, 2},
    {"Animal_Fish_B", 1, 1, 100, 0, 2},
    {"Animal_Fish_C", 1, 1, 100, 0, 2},
    {"Animal_Fish_D", 1, 1, 100, 0, 2},
    {"Animal_Fish_E", 1, 1, 100, 0, 2},
    {"Animal_Fish_F", 1, 1, 100, 0, 2},
    {"Animal_Fish_G", 1, 1, 100, 0, 2},
    {"Animal_Fish_H", 1, 1, 100, 0, 2},
    {"Animal_Fish_I", 1, 1, 100, 0, 2},
    {"Animal_Fish_J", 1, 1, 100, 0, 2},
    {"Animal_Fish_K", 1, 1, 100, 0, 2},
    {"Animal_Fish_L", 1, 1, 100, 0, 2},
    {"Animal_Fish_M", 1, 1, 100, 0, 2},
    {"Animal_Fish_X", 1, 1, 100, 0, 2},
    {"Animal_Fish_Z", 1, 1, 100, 0, 2},
    {"Animal_Fox_A", 8, 16, 100, 0, 2},
    {"Animal_Fox_B", 18, 22, 100, 0, 2},
    {"Animal_Goat_A", 100, 34, 100, 0, 2},
    {"Animal_Goat_B", 100, 34, 100, 0, 2},
    {"Animal_Gull_A", 1, 1, 100, 0, 2},
    {"Animal_Gull_Swarm", 1, 1, 100, 0, 2},
    {"Animal_Hawk_A", 1, 1, 100, 0, 2},
    {"Animal_Heron_A", 1, 1, 100, 0, 2},
    {"Animal_Heron_B", 1, 1, 100, 0, 2},
    {"Animal_Horse_A", 100, 34, 100, 0, 2},
    {"Animal_Kokko_A", 100, 34, 100, 0, 2},
    {"Animal_Kokko_B", 100, 34, 100, 0, 2},
    {"Animal_LittleBird_A", 1, 1, 100, 0, 2},
    {"Animal_LittleBird_B", 1, 1, 100, 0, 2},
    {"Animal_LittleBird_C", 1, 1, 100, 0, 2},
    {"Animal_LittleBird_D", 1, 1, 100, 0, 2},
    {"Animal_LittleBird_E", 1, 1, 100, 0, 2},
    {"Animal_LittleBird_F", 1, 1, 100, 0, 2},
    {"Animal_Pigeon_A", 1, 1, 100, 0, 2},
    {"Animal_Pigeon_B", 1, 1, 100, 0, 2},
    {"Animal_Pigeon_B_Swarm", 1, 1, 100, 0, 2},
    {"Animal_Pigeon_Swarm", 1, 1, 100, 0, 2},
    {"Animal_Ptarmigan_A", 1, 1, 100, 0, 2},
    {"Animal_Ptarmigan_B", 1, 1, 100, 0, 2},
    {"Animal_Rhino_A", 80, 33, 100, 0, 2},
    {"Animal_RupeeRabbit_A", 100, 34, 100, 0, 2},
    {"Animal_Sheep_A", 100, 34, 100, 0, 2},
    {"Animal_Squirrel_A", 1, 1, 100, 0, 2},
    {"Animal_SunazarashiSP_A", 100, 34, 100, 0, 2},
    {"Animal_SunazarashiSP_B", 100, 34, 100, 0, 2},
    {"Animal_SunazarashiSP_C", 100, 34, 100, 0, 2},
    {"Animal_Sunazarashi_A", 100, 34, 100, 0, 2},
    {"Animal_Sunazarashi_B", 100, 34, 100, 0, 2},
    {"Animal_Sunazarashi_D_DLC2nd", 100, 34, 100, 0, 2},
    {"Animal_WildDuck_A", 1, 1, 100, 0, 2},
    {"Animal_WildGoat_A", 8, 16, 100, 0, 2},
    {"Animal_Wolf_A", 16, 21, 50, 27, 6},
    {"Animal_Wolf_B", 30, 25, 50, 40, 6},
    {"Animal_Wolf_C", 45, 28, 50, 51, 6},
    {"Enemy_AirOctarock", 8, 16, 50, 16, 0},
    {"Enemy_Assassin_Azito_Middle", 600, 36, 150, 273, 0},
    {"Enemy_Assassin_Azito_Middle_DLC2", 600, 36, 150, 273, 0},
    {"Enemy_Assassin_Junior", 72, 16, 150, 49, 0},
    {"Enemy_Assassin_Junior_Npc_001", 72, 16, 150, 49, 0},
    {"Enemy_Assassin_Junior_Npc_002", 72, 16, 150, 49, 0},
    {"Enemy_Assassin_Middle", 400, 36, 150, 273, 0},
    {"Enemy_Assassin_Middle_Quest", 400, 36, 150, 273, 0},
    {"Enemy_Assassin_Senior", 300, 54, 150, 600, 0},
    {"Enemy_Assassin_Shooter_Azito_Junior", 64, 16, 150, 49, 0},
    {"Enemy_Assassin_Shooter_Junior", 64, 16, 150, 49, 0},
    {"Enemy_Beamos_Static", 1000, 51, 200, 800, 0},
    {"Enemy_Bee_Swarm", 2, 6, 50, 4, 0},
    {"Enemy_Bokoblin_Bone_Junior", 1, 16, 50, 16, 0},
    {"Enemy_Bokoblin_Bone_Junior_AllDay", 1, 16, 50, 16, 0},
    {"Enemy_Bokoblin_Dark", 720, 38, 100, 207, 0},
    {"Enemy_Bokoblin_Gold", 1080, 51, 100, 400, 0},
    {"Enemy_Bokoblin_Guard_Junior", 13, 8, 100, 12, 0},
    {"Enemy_Bokoblin_Guard_Junior_Ambush", 13, 8, 100, 12, 0},
    {"Enemy_Bokoblin_Guard_Junior_TreeHouseTop", 13, 8, 100, 12, 0},
    {"Enemy_Bokoblin_Guard_Middle", 72, 16, 100, 33, 0},
    {"Enemy_Bokoblin_Guard_Middle_Ambush", 72, 16, 100, 33, 0},
    {"Enemy_Bokoblin_Guard_Middle_TreeHouseTop", 72, 16, 100, 33, 0},
    {"Enemy_Bokoblin_Junior", 13, 8, 50, 12, 0},  // (s110: HP% 100 -> 50 = KH2's weakest Heartless tier (Shadow, enmp 0); EXP field unchanged)
    {"Enemy_Bokoblin_Middle", 72, 16, 100, 33, 0},
    {"Enemy_Bokoblin_Senior", 240, 26, 100, 87, 0},
    {"Enemy_Chuchu_Electric_Junior", 12, 19, 50, 22, 0},
    {"Enemy_Chuchu_Electric_Middle", 20, 5, 100, 8, 0},
    {"Enemy_Chuchu_Electric_Senior", 48, 7, 100, 10, 0},
    {"Enemy_Chuchu_Fire_Junior", 12, 19, 50, 22, 0},
    {"Enemy_Chuchu_Fire_Middle", 20, 5, 100, 8, 0},
    {"Enemy_Chuchu_Fire_Senior", 48, 7, 100, 10, 0},
    {"Enemy_Chuchu_Ice_Junior", 12, 19, 50, 22, 0},
    {"Enemy_Chuchu_Ice_Middle", 20, 5, 100, 8, 0},
    {"Enemy_Chuchu_Ice_Senior", 48, 7, 100, 10, 0},
    {"Enemy_Chuchu_Junior", 3, 9, 50, 7, 0},
    {"Enemy_Chuchu_Middle", 20, 5, 100, 8, 0},
    {"Enemy_Chuchu_Senior", 48, 7, 100, 10, 0},
    {"Enemy_Dragon_Electric", 100, 34, 0, 0, 0},
    {"Enemy_Dragon_Fire", 100, 34, 0, 0, 0},
    {"Enemy_Dragon_Grudge", 100, 34, 0, 0, 0},
    {"Enemy_Dragon_Ice", 100, 34, 0, 0, 0},
    {"Enemy_Ganon", 8000, 60, 0, 0, 1},
    {"Enemy_GanonBeast", 8, 16, 0, 0, 1},
    {"Enemy_GanonGrudge", 1, 1, 0, 0, 1},
    {"Enemy_GanonGrudge_01", 30, 25, 0, 0, 1},
    {"Enemy_GanonGrudge_02", 1, 1, 0, 0, 1},
    {"Enemy_Giant_Bone", 1000, 26, 200, 174, 0},
    {"Enemy_Giant_Bone_AllDay", 1000, 26, 50, 43, 0},
    {"Enemy_Giant_Junior", 600, 26, 200, 174, 0},
    {"Enemy_Giant_Middle", 800, 26, 200, 174, 0},
    {"Enemy_Giant_Senior", 1000, 26, 200, 174, 0},
    {"Enemy_Golem_Fire", 800, 26, 200, 174, 0},
    {"Enemy_Golem_Fire_R", 1600, 26, 200, 174, 0},
    {"Enemy_Golem_Ice", 800, 26, 200, 174, 0},
    {"Enemy_Golem_Junior", 300, 26, 200, 174, 0},
    {"Enemy_Golem_Little", 20, 7, 100, 10, 0},
    {"Enemy_Golem_Little_Fire", 20, 7, 100, 10, 0},
    {"Enemy_Golem_Little_Ice", 20, 7, 100, 10, 0},
    {"Enemy_Golem_Middle", 600, 26, 200, 174, 0},
    {"Enemy_Golem_Senior", 900, 26, 200, 174, 0},
    {"Enemy_Guardian_A", 1500, 20, 200, 100, 0},
    {"Enemy_Guardian_A_Fixed_Moss", 500, 7, 200, 20, 0},
    {"Enemy_Guardian_A_Fixed_Sand", 500, 7, 200, 20, 0},
    {"Enemy_Guardian_A_Fixed_Snow", 500, 7, 200, 20, 0},
    {"Enemy_Guardian_A_Moss", 1500, 20, 200, 100, 0},
    {"Enemy_Guardian_A_Sand", 1500, 20, 200, 100, 0},
    {"Enemy_Guardian_A_Snow", 1500, 20, 200, 100, 0},
    {"Enemy_Guardian_B", 1500, 14, 200, 52, 0},
    {"Enemy_Guardian_B_Clean", 1500, 14, 200, 52, 0},
    {"Enemy_Guardian_C", 1500, 20, 200, 100, 0},
    {"Enemy_Guardian_C_Clean", 1500, 20, 200, 100, 0},
    {"Enemy_Guardian_Mini_Baby", 13, 21, 50, 27, 0},
    {"Enemy_Guardian_Mini_Baby_Dark", 13, 21, 50, 27, 0},
    {"Enemy_Guardian_Mini_Junior", 100, 21, 150, 82, 0},
    {"Enemy_Guardian_Mini_Junior_Dark", 100, 21, 150, 82, 0},
    {"Enemy_Guardian_Mini_Junior_DetachLineBeam", 100, 21, 150, 82, 0},
    {"Enemy_Guardian_Mini_Junior_Wipe", 375, 21, 150, 82, 0},
    {"Enemy_Guardian_Mini_Junior_Wipe_DetachLineBeam", 375, 21, 150, 82, 0},
    {"Enemy_Guardian_Mini_Middle", 1500, 21, 150, 82, 0},
    {"Enemy_Guardian_Mini_Middle_Dark", 300, 21, 150, 82, 0},
    {"Enemy_Guardian_Mini_Middle_DetachLineBeam", 1500, 21, 150, 82, 0},
    {"Enemy_Guardian_Mini_Senior", 3000, 21, 150, 82, 0},
    {"Enemy_Guardian_Mini_Senior_Dark", 600, 21, 150, 82, 0},
    {"Enemy_Keese", 1, 4, 50, 3, 0},
    {"Enemy_Keese_AllDay", 1, 4, 50, 3, 0},
    {"Enemy_Keese_Electric", 1, 4, 50, 3, 0},
    {"Enemy_Keese_Electric_AllDay", 1, 4, 50, 3, 0},
    {"Enemy_Keese_Fire", 1, 4, 50, 3, 0},
    {"Enemy_Keese_Fire_AllDay", 1, 4, 50, 3, 0},
    {"Enemy_Keese_Ice", 1, 4, 50, 3, 0},
    {"Enemy_Keese_Ice_AllDay", 1, 4, 50, 3, 0},
    {"Enemy_Keese_Swarm", 1, 14, 50, 13, 0},
    {"Enemy_Keese_Swarm_AllDay", 1, 14, 50, 13, 0},
    {"Enemy_Lizalfos_Bone_Junior", 1, 28, 50, 51, 0},
    {"Enemy_Lizalfos_Dark", 864, 41, 150, 372, 0},
    {"Enemy_Lizalfos_Electric", 288, 34, 150, 238, 0},
    {"Enemy_Lizalfos_Electric_ForestTower", 288, 34, 150, 238, 0},
    {"Enemy_Lizalfos_Fire", 160, 34, 150, 238, 0},
    {"Enemy_Lizalfos_Fire_Guard_Ambush", 160, 34, 150, 238, 0},
    {"Enemy_Lizalfos_Gold", 1296, 54, 150, 600, 0},
    {"Enemy_Lizalfos_Guard_Junior", 50, 14, 150, 39, 0},
    {"Enemy_Lizalfos_Guard_Junior_LongVisibility", 50, 14, 150, 39, 0},
    {"Enemy_Lizalfos_Guard_Middle", 120, 28, 150, 153, 0},
    {"Enemy_Lizalfos_Guard_Middle_LongVisibility", 120, 28, 150, 153, 0},
    {"Enemy_Lizalfos_Ice", 288, 34, 150, 238, 0},
    {"Enemy_Lizalfos_Junior", 50, 14, 150, 39, 0},
    {"Enemy_Lizalfos_Junior_Guard_Ambush", 50, 14, 150, 39, 0},
    {"Enemy_Lizalfos_Middle", 120, 28, 150, 153, 0},
    {"Enemy_Lizalfos_Middle_Guard_Ambush", 120, 28, 150, 153, 0},
    {"Enemy_Lizalfos_Senior", 288, 34, 150, 238, 0},
    {"Enemy_Lynel_Dark", 5000, 54, 200, 800, 0},
    {"Enemy_Lynel_Gold", 7500, 66, 200, 800, 0},
    {"Enemy_Lynel_Junior", 2000, 38, 200, 414, 0},
    {"Enemy_Lynel_Junior_Mountain", 2000, 38, 200, 414, 0},
    {"Enemy_Lynel_Middle", 3000, 44, 200, 588, 0},
    {"Enemy_Lynel_Senior", 4000, 48, 200, 724, 0},
    {"Enemy_Moriblin_Bone_Junior", 2, 31, 50, 64, 0},
    {"Enemy_Moriblin_Dark", 1080, 44, 150, 441, 0},
    {"Enemy_Moriblin_Gold", 1620, 56, 150, 600, 0},
    {"Enemy_Moriblin_Junior", 56, 16, 150, 49, 0},
    {"Enemy_Moriblin_Middle", 144, 31, 150, 192, 0},
    {"Enemy_Moriblin_Senior", 360, 36, 150, 273, 0},
    {"Enemy_Moriblin_Senior_Volcano", 360, 36, 150, 273, 0},
    {"Enemy_Octarock", 8, 7, 100, 10, 0},
    {"Enemy_Octarock_Desert", 8, 7, 100, 10, 0},
    {"Enemy_Octarock_Forest", 8, 7, 100, 10, 0},
    {"Enemy_Octarock_Snow", 8, 7, 100, 10, 0},
    {"Enemy_Octarock_Stone", 8, 7, 100, 10, 0},
    {"Enemy_Sandworm", 1500, 54, 200, 800, 0},
    {"Enemy_SandwormR", 1600, 54, 200, 800, 0},
    {"Enemy_SiteBoss_Bow", 800, 49, 0, 0, 1},
    {"Enemy_SiteBoss_Bow_Castle", 800, 49, 0, 0, 1},
    {"Enemy_SiteBoss_Bow_R", 500, 46, 0, 0, 1},
    {"Enemy_SiteBoss_Lsword", 800, 49, 0, 0, 1},
    {"Enemy_SiteBoss_Lsword_Castle", 800, 49, 0, 0, 1},
    {"Enemy_SiteBoss_Lsword_R", 500, 46, 0, 0, 1},
    {"Enemy_SiteBoss_Spear", 800, 49, 0, 0, 1},
    {"Enemy_SiteBoss_Spear_Castle", 800, 49, 0, 0, 1},
    {"Enemy_SiteBoss_Spear_R", 500, 46, 0, 0, 1},
    {"Enemy_SiteBoss_Sword", 800, 49, 0, 0, 1},
    {"Enemy_SiteBoss_Sword_Castle", 800, 49, 0, 0, 1},
    {"Enemy_SiteBoss_Sword_R", 500, 46, 0, 0, 1},
    {"Enemy_Wizzrobe_Electric", 150, 14, 150, 39, 0},
    {"Enemy_Wizzrobe_Electric_Senior", 300, 24, 150, 109, 0},
    {"Enemy_Wizzrobe_Fire", 150, 14, 150, 39, 0},
    {"Enemy_Wizzrobe_Fire_Senior", 300, 24, 150, 109, 0},
    {"Enemy_Wizzrobe_Ice", 150, 14, 150, 39, 0},
    {"Enemy_Wizzrobe_Ice_Senior", 300, 24, 150, 109, 0},
};
#define SL_ENEMY_N ((int)(sizeof SL_ENEMY / sizeof SL_ENEMY[0]))
// ---- END GENERATED ----

// (s85 slice 5) the equipped main Keyblade's Attack / Magic (sora_kb.h KB_TAB, 03system.bin; Kingdom Key 3 / 1 VERIFIED
// PHH keyblades.kh.json, 03system.bin@0x13ED4). KB is declared below; the uses come after it.
#define SL_KEY_STR kb_str(KB.kb[KB_MAIN])
#define SL_KEY_MAG kb_mag(KB.kb[KB_MAIN])
#define SL_NC 32       // credit list: the enemies Sora hit lately
#define SL_ENV "SORA_KH2_LV_STATE"  // (process env: survives the DLL hot reload, not a game restart)
static const char *const SL_DREAM_NAME[3] = {"sword", "shield", "staff"};

typedef struct {
    uint64_t actor;
    float hit_at;              // SL.clock (game seconds) of Sora's last queued hit on it
    int32_t life, max_life;    // last life seen (linkpos's copy of actor +0x848); table life or the highest seen
    int32_t queued;            // damage queued since its life last changed (the vanish rule's "lethal")
    uint8_t kh_level, pct, flags;
    uint16_t exp;
    uint16_t hp_pct, cap, mind;  // (s109) KH2 enmp HP percent / Damage Cap / Minimum Damage of its KH2 analogue
    bool used, paid, named;
    bool boss;                   // (s110) a SL_BOSS_HP row (keeps its own level under worldlv 1)
    char name[48];
} SlCredit;

static struct {
    bool init;
    uint32_t exp, kills;
    int lv;
    int lvdmg, lvlog, khdmg, win;  // live: KH-relative damage on, per-hit damage log, scale x100, kill window s
    int dream_seen;
    int devlv;                     // (s67) live "set devlv N": act as LV N (Str / Mag, Drive max 9); 0 = off; never saved
    float clock;                   // game seconds of driven, unpaused frames (framepace_game_ratio / 30 each)
    SlCredit c[SL_NC];
} SL = {.lvdmg = 1, .lvlog = 1, .khdmg = 100, .win = 10, .lv = 1, .dream_seen = -99};
// (s90) live "set debugall 1": the testing toggle (sora_debugall.h, included from sora.c after sora_anti.h)
static int g_dbgall;
static int g_dbm_god, g_dbm_clip;  // (s143 dbggod) Debug Menu: God Mode (exe drops every hit + life refilled) / No Clip (sora_dbgmenu.h)
static void dbg_init(void);
static void dbm_init(void);                  // (s140 dbgmenu, sora_dbgmenu.h)
static int dbm_suspend(void);
static int g_dbm_newgame;  // (s142 review) set by sl_slot_saved's New Game branch, read + cleared by dbm_resume: the debug EXP is not put back after it
static void dbm_resume(int was, bool is_load);
static void dbg_tick(void);
static int dbg_suspend(const char *why);
static void dbg_resume(int was, const char *why);
static bool dbg_set_key(const char *key, int v);
// (s88 EXP) live "set expcurve N": 0 = the S54 table as is | 1 flat x2 | 2 level-gap
// (enemy KH level vs Sora LV, x1.6) | 3 LV bands (x0.7 <LV10, x1 <20, x2 <30, x3 30+). GUESS numbers: recon/S88_HELPER_EXP.md.
static int sl_expcurve = 3;  // (s88: user picked option 3 LV bands 2026-11-02)
static uint32_t sl_curve_exp(uint32_t e, int kh, int lv) {
    if (!sl_expcurve) return e;  // (s88 review: 0 = bit for bit the old value, no rounding path)
    int pct = 100;
    if (sl_expcurve == 1) pct = 200;
    else if (sl_expcurve == 2) { pct = (100 + 8 * (kh - lv)) * 16 / 10; pct = pct < 40 ? 40 : pct > 400 ? 400 : pct; }
    else if (sl_expcurve == 3) pct = lv < 10 ? 70 : lv < 20 ? 100 : lv < 30 ? 200 : 300;
    uint64_t r = ((uint64_t)e * (uint64_t)pct + 50) / 100;  // (s88 review: 64-bit, no wrap)
    if (r > 0xffffffffu) r = 0xffffffffu;
    return e && !r ? 1 : (uint32_t)r;
}
// (s87 user decision: "keep him at a base level strength like link", Keyblades give the damage) Str stays at LV 1's
// whatever the level; Mag / Def / AP / abilities / MP still grow. Live "set lvstr 1" = Str from the level row again.
static int sl_lvstr = 1;  // (s91 user: "do the KH2 way for now" - Str grows with level again; set lvstr 0 = the s87 fixed Str)
static int sl_str_of(const SlLevel *s) { return sl_lvstr ? s->str : SL_LV[0].str; }
// (s87b user: "keep magic fixed too", spell upgrades Fira / Blizzara / ... will give the damage) Mag stays at LV 1's.
// Live "set lvmag 1" = Mag from the level row again.
static int sl_lvmag;
static int sl_mag_of(const SlLevel *s) { return sl_lvmag ? s->mag : SL_LV[0].mag; }

// ------------------------------------------------------------------ tables
static int sl_dream(void) { return dream_weapon >= 0 && dream_weapon <= 2 ? dream_weapon : 0; }
static uint32_t sl_exp_max(void) { return SL_LV[98].exp; }
static int sl_level_of(uint32_t exp) {
    int l = 1;
    while (l < 99 && exp >= SL_LV[l].exp) l++;
    return l;
}
static int sl_lv(void) { return SL.devlv > 0 ? SL.devlv : SL.lv; }  // (s67: the level the rules use; devlv = dev override)
static void dg_kill(int pct, int flags);  // (s67 drive: sora_drive.h, included after this header)
static int dg_points(void);
static void dg_load_points(int p, const char *why);
static uint32_t vl_exp_get(void);           // (s70 sora_valor_lv.h)
static void vl_load(int exp, const char *why);
static void wl_load(int exp, const char *why);  // (s75 Wisdom EXP: sora_wisdom_lv.h, included later)
static uint32_t wl_exp_get(void);
static void wl_kill(int flags);
static void ll_load(int exp, const char *why);  // (s89 Limit EXP: sora_limit_lv.h, included later)
static uint32_t ll_exp_get(void);
static void ml_load(int exp, const char *why);  // (s89 b4-master: sora_master_lv.h, included later)
static uint32_t ml_exp_get(void);
static void ml_kill(int pct, int flags);
static void fl_load(int exp, const char *why);  // (s89 b4-final Final EXP: sora_final_lv.h, included later)
static uint32_t fl_exp_get(void);
static void fl_kill(int flags);
static uint32_t anti_pts_get(void);              // (s89 Anti points: sora_anti.h, included later)
static void anti_load(int pts, const char *why);
static bool anti_no_exp(void);
static uint32_t su_exp_get(void);              // (s148 Summon EXP: sora_sumlv.h, included later)
static void su_load(int exp, const char *why);
// (s89 trial) a New Game / a slot without a sidecar: every Drive Form back to LV1 + no Anti points (before: the last
// world's Valor / Wisdom EXP stayed - now also the Drive gauge's max, which grows with the form levels). Live "set
// ngforms 0" = the old rule (form EXPs kept).
static int g_sl_forms_reset = 1;
static void sl_forms_reset(const char *why) {
    if (!g_sl_forms_reset) { LOG("[sora] %s: form levels KEPT (ngforms 0)\n", why); return; }
    vl_load(-1, why), wl_load(-1, why), ll_load(-1, why), ml_load(-1, why), fl_load(-1, why), anti_load(-1, why), su_load(-1, why);  // (s148: + the Summon level)
}
static uint32_t sl_next(int lv) { return lv < 99 ? SL_LV[lv].exp : SL_LV[98].exp; }  // (EXP that reaches lv + 1)
static const char *sl_abil_name(int id) {
    for (int i = 0; i < SL_ABIL_N; i++) if (SL_ABIL[i].id == id) return SL_ABIL[i].name;
    return id ? "?" : "-";
}
static const SlEnemy *sl_enemy_find(const char *name) {  // (binary search: SL_ENEMY is in strcmp order)
    int lo = 0, hi = SL_ENEMY_N - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2, c = strcmp(name, SL_ENEMY[mid].name);
        if (!c) return &SL_ENEMY[mid];
        if (c < 0) hi = mid - 1;
        else lo = mid + 1;
    }
    return NULL;
}
// (s109 strength rework) Boss-class BotW enemies -> the enmp row of a KH2 boss of the same weight (00battle.bin enmp,
// VERIFIED data: normal Heartless/Nobodies are hp 50-250 %, story bosses 1000-2000 %, Data Org 1500-1878 %). hp = HP
// percent of lvpm[level].hp_mul (OpenKH: HP = (hp x hp_mul + 99) / 100), cap / mind = that row's Damage Cap / Minimum
// Damage. The analogue picks are a design choice (GUESS), the numbers are KH2's. Longest prefix wins.
typedef struct { const char *prefix; uint16_t hp, cap, mind; const char *kh2; } SlBossHp;
static const SlBossHp SL_BOSS_HP[] = {
    {"Enemy_Lynel_Junior", 1000, 300, 50, "Cerberus (enmp 19)"},
    {"Enemy_Lynel_Middle", 1300, 231, 39, "Shan-Yu (enmp 71)"},
    {"Enemy_Lynel_Senior", 1500, 134, 34, "Xaldin (enmp 87)"},
    {"Enemy_Lynel_Dark", 1600, 125, 32, "Storm Rider (enmp 25)"},
    {"Enemy_Lynel_Gold", 2000, 75, 25, "Sephiroth (enmp 140)"},
    {"Enemy_Giant_Junior", 600, 500, 84, "Pete, Olympus (enmp 26)"},
    {"Enemy_Giant_Middle", 800, 313, 63, "Volcanic Lord (enmp 122)"},
    {"Enemy_Giant_Senior", 1000, 300, 50, "Dark Thorn (enmp 22)"},
    {"Enemy_Giant_Bone", 650, 385, 77, "Oogie Boogie (enmp 75)"},
    {"Enemy_Golem_Junior", 500, 300, 100, "Sark (enmp 100)"},
    {"Enemy_Golem_Middle", 700, 429, 72, "Cornerstone cage (enmp 79)"},
    {"Enemy_Golem_Senior", 1000, 200, 50, "Grim Reaper 1 (enmp 154)"},
    {"Enemy_Golem_Fire", 800, 313, 63, "Volcanic Lord (enmp 122)"},
    {"Enemy_Golem_Fire_R", 1200, 521, 42, "Volcanic Lord, Hades cup (enmp 197)"},
    {"Enemy_Golem_Ice", 800, 313, 63, "Blizzard Lord (enmp 123)"},
    {"Enemy_Golem_Little", 0, 0, 0, NULL},  // (Pebblit: stays a normal enemy)
    {"Enemy_Sandworm", 1600, 125, 32, "Groundshaker (enmp 95)"},
    {"Enemy_Guardian_A", 1000, 200, 50, "Grim Reaper 2 (enmp 124)"},
    {"Enemy_Guardian_A_Fixed", 450, 667, 112, "Shadow Stalker (enmp 23)"},
    {"Enemy_Guardian_B", 700, 429, 72, "Cornerstone cage (enmp 79)"},
    {"Enemy_Guardian_C", 1000, 200, 50, "Grim Reaper 2 (enmp 124)"},
    {"Enemy_SiteBoss", 1500, 134, 34, "Xaldin (enmp 87)"},
    {"Enemy_Ganon", 2000, 75, 25, "Sephiroth (enmp 140)"},
    {"Enemy_GanonBeast", 0, 0, 0, NULL},
    {"Enemy_GanonGrudge", 0, 0, 0, NULL},
};
static int sl_boss_hp_on = 1;  // live "set bosshp 0" = the s54 rule (table pct, 200 for bosses)
static const SlBossHp *sl_boss_hp(const char *name) {
    const SlBossHp *b = NULL;
    size_t bl = 0;
    for (size_t i = 0; i < sizeof SL_BOSS_HP / sizeof SL_BOSS_HP[0]; i++) {
        size_t l = strlen(SL_BOSS_HP[i].prefix);
        if (l > bl && !strncmp(name, SL_BOSS_HP[i].prefix, l)) b = &SL_BOSS_HP[i], bl = l;
    }
    return b && b->hp ? b : NULL;
}
static int sl_life_level(int32_t life) {  // (the tool's rank < 2 rule: 1 + 7.2 ln(life), for names not in the table)
    int l = (int)lroundf(1.0f + 7.2f * logf((float)(life > 1 ? life : 1)));
    return l < 1 ? 1 : l > 60 ? 60 : l;
}
// the abilities learned up to LV lv with the current Dream Weapon, ", "-separated
static void sl_abil_list(int lv, char *out, size_t n) {
    size_t k = 0;
    out[0] = 0;
    for (int l = 1; l <= lv && l <= 99; l++) {
        int id = SL_LV[l - 1].ab[sl_dream()];
        if (id && k + 40 < n) k += (size_t)snprintf(out + k, n - k, "%s%s (LV%d)", k ? ", " : "", sl_abil_name(id), l);
    }
    if (!k) snprintf(out, n, "none");
}

// ------------------------------------------------------------------ the actor's name (DLL-side, read only)
#ifndef SL_ACTOR_NAME
// (s54 levels) the same read as linkpos.c actor_name: BaseProc +0x10 = sead::SafeString mName's mStringTop, a guest
// pointer (rt.h GUEST_LO / HI; guest memory is mapped at its own address). Only called for an actor that is in
// linkpos's list this frame (Sora just hit it), so it is the read linkpos does on it every frame.
static const char *sl_actor_name_guest(uint64_t a) {
    if (a < 0x7000000000ull || a + 0x18 > 0x7320000000ull) return NULL;
    uint64_t s = *(const uint64_t *)(uintptr_t)(a + 0x10);
    return s >= 0x7000000000ull && s + 48 <= 0x7320000000ull ? (const char *)(uintptr_t)s : NULL;
}
#define SL_ACTOR_NAME(a) sl_actor_name_guest(a)
#endif

// ------------------------------------------------------------------ exe exports (s55 slice 2), by GetProcAddress
// recon/S55_HELPER_LEVELS23.md. Each is NULL on an exe without it, and the DLL still loads and runs: no died queue =
// the slice-1 kill rules only; no slot events = mtime polling of game_data.sav + "the newest save at boot".
typedef struct { uint64_t actor, frame; int32_t life, kind, reason; char name[48]; } SlDied;  // = linkpos.c LinkposDied
static struct {
    bool bound;
    int (*died)(SlDied *, int, int);                          // linkpos_enemy_died(out, max, sizeof): drains the queue
    int (*name)(uint64_t, char *, int);                       // linkpos_enemy_name(actor, buf, n)
    int (*save_ev)(uint64_t *, int *, int *, char *, int);    // fs_save_event(&seq, &slot, &kind, dir, n)
    int (*gdt_s32)(const char *, int32_t *);                  // linkpos_gdt_s32 / _bool (slice 5 milestones; a check now)
    int (*gdt_bool)(const char *, int *);
    const char *const *save_dir;                              // fs.c g_save_dir (a data export, old exe too)
} SLX;
static void sl_exe_bind(void) {
    if (SLX.bound) return;
    SLX.bound = true;
    HMODULE exe = GetModuleHandleA(NULL);
    SLX.died = (int (*)(SlDied *, int, int))(void *)GetProcAddress(exe, "linkpos_enemy_died");
    SLX.name = (int (*)(uint64_t, char *, int))(void *)GetProcAddress(exe, "linkpos_enemy_name");
    SLX.save_ev = (int (*)(uint64_t *, int *, int *, char *, int))(void *)GetProcAddress(exe, "fs_save_event");
    SLX.gdt_s32 = (int (*)(const char *, int32_t *))(void *)GetProcAddress(exe, "linkpos_gdt_s32");
    SLX.gdt_bool = (int (*)(const char *, int *))(void *)GetProcAddress(exe, "linkpos_gdt_bool");
    SLX.save_dir = (const char *const *)(void *)GetProcAddress(exe, "g_save_dir");
    LOG("[sora] levels: exe exports: died queue %s, enemy names %s, save-slot events %s, gdt getters %s, g_save_dir %s\n",
        SLX.died ? "yes" : "NO (slice-1 kill rules)", SLX.name ? "yes" : "no (DLL read)",
        SLX.save_ev ? "yes" : "NO (mtime polling)", SLX.gdt_s32 && SLX.gdt_bool ? "yes" : "no", SLX.save_dir ? "yes" : "no");
}

// ------------------------------------------------------------------ persistence
// (s55 slice 3) Per BotW save slot: <save dir>/save_0/<N>/sora_kh2.json next to BotW's game_data.sav (doc sec 5), so a
// level survives game restarts and follows Continue / Load like BotW's own progress:
//   BotW READS slot N's game_data.sav (Continue, Load, Continue after a Game Over; exe event kind 0)
//       -> the sidecar's EXP / kills / Dream Weapon. No sidecar (an old save): the story floor (SLS.floor, GUESS:
//          paraglider -> LV 5, + 6 per Divine Beast; LV 1 before it) - or the unsaved LV if this state belongs to no
//          slot yet (slice 1 / env start values) and is higher.
//   BotW WROTE slot N (autosave / save; exe event kind 2 "written out", or its mtime settled for 1.5 s)
//       -> the sidecar gets the state at the moment the write began. New Game rule: a written save with no story
//          progress (no paraglider, tower, shrine, beast) after a state with progress = a new playthrough -> LV 1.
//   Old exe (no events): the newest save at boot = Continue's pick (auto-Continue does the same), writes by polling.
// Story progress is read from the save file itself (u32 crc32(name) + u32 value pairs from 0x0c, tools/give_warps.py).
// Hot reload: the process environment (SL_ENV, v2 adds the slot, the progress, the floor knob, the event sequence).
// GetEnvironmentVariableA, not getenv: the CRT's copy never sees SetEnvironmentVariableA.
// (s111 roxload) The body this slot was saved with (sidecar "roxas" 0/1; no field: the save file's paraglider), applied at once on a
// slot load in auto mode (sora_roxas.h rox_tick) so a load shows the right body from the first frame. -1 = none. Live "set roxload 0" = off.
static int g_roxload = 1, g_rox_hint = -1;
static ULONGLONG g_rox_hint_at;
static int rox_on_now(void);  // (sora_roxas.h) 1 Roxas is the body, 0 Sora
static int g_char;            // (sora_roxas.h)
static int g_dbm_sora;        // (s146 outfit, sora_roxas.h / sora_dbgmenu.h) Debug Menu "Become Sora" ON: Sora is the body whatever the story says; never saved
static bool g_rox_hold;  // (sora_mod_frame drops every draw actor after the body while set: Sora is not shown while Roxas loads; DLL-only)
#define SL_SLOTS 8
#define SL_SIDECAR "sora_kh2.json"
#define SL_DIE_GRACE 5.0f  // s past the kill window a died event still pays (the death animation before slot 54)
typedef struct { bool ok; int glider, beasts, towers, shrines, prog; } SlStory;
static struct {
    int slot;              // the BotW save slot this state belongs to (-1: none yet)
    int base;              // that save's story progress (-1 unknown): the New Game rule
    int floor;             // live lvfloor: a save without a sidecar starts at the story floor (1) or at LV 1 (0)
    uint64_t seq;          // the last fs_save_event seen
    char root[1024];       // <save dir>/save_0
    uint64_t next_poll;    // GetTickCount64 of the next mtime poll
    bool polled, gdt_said;
    int died_bad;
    struct { int64_t mtime, size; uint64_t pend_at; uint32_t exp, kills; bool pend; } f[SL_SLOTS];
} SLS = {.slot = -1, .base = -1, .floor = 1};

// (s82 slice 3) Sora's equipped abilities: on / seen masks (sora_abil.h bits), saved in the slot sidecar ("abil_on" /
// "abil_seen", missing = 0 -> auto-equip in learn order) + SL_AB_ENV over the hot reload. ab_tick re-syncs when the
// learned list or AP max changes (level up, Dream Weapon, form LV2, slot load).
#define SL_AB_ENV "SORA_KH2_ABIL"
static int vl_lv(void);
static int wl_lv(void);
static int ll_lv(void);
static int ml_lv(void);
static int fl_lv(void);
static struct { abm_t on, seen, dev_on, dev_seen, a_on, a_seen; int key; bool dirty, all; int hp, hp_max; } AB = {.key = -1};  // (s141 dbglv) all = the Debug Menu "All Abilities" layer is applied; a_* = the masks under it (real, or the test-level set)
static bool ab_all(void) { return g_dbgall || AB.all; }  // every ability learned + equipped (debugall, or the Debug Menu row)
static abm_t ab_real_on(void) { return SL.devlv ? AB.dev_on : AB.all ? AB.a_on : AB.on; }  // the masks a save / the env keep: never a debug layer
static abm_t ab_real_seen(void) { return SL.devlv ? AB.dev_seen : AB.all ? AB.a_seen : AB.seen; }
static int dbm_set(int row, int val, const char *why);  // (s141, sora_dbgmenu.h)
static void dbm_ab_tick(void);  // (s141, sora_dbgmenu.h)  // dev_*: stash while devlv  // hp: sora.c HUD (Experience Boost)
// (s102 b16-chestabil) abilities BotW's treasure chests gave (sora_chestabil.h): CA.mask (sora_abil.h bits; popcount = rewards given).
// Saved in the slot sidecar ("chest_mask" / "chest_mask_hi") + SL_CA_ENV over the hot reload.
#define SL_CA_ENV "SORA_KH2_CHEST"
static struct { abm_t mask; } CA;
// (s84 slice 4) Sora's command-menu shortcuts (sora_cust.h; sora.c menu_tick + HUD read SC.sc), saved in the slot
// sidecar ("shortcuts", cust_pack; missing = the default) + SL_SC_ENV over the hot reload.
#include "sora_cust.h"
#define SL_SC_ENV "SORA_KH2_SC"
static struct { int sc[CUST_SLOTS]; } SC = {{0, 1, 2, 3}};
static void sc_log(const char *why) {
    LOG("[sora] shortcuts (%s): Triangle %s, Square %s, Cross %s, Circle %s\n", why, cust_str(SC.sc[0]), cust_str(SC.sc[1]),
        cust_str(SC.sc[2]), cust_str(SC.sc[3]));
}
static void sc_load(int found, uint32_t v, const char *why) {
    int bad = cust_load(found, v, SC.sc);
    if (bad) LOG("[sora] shortcuts: %d bad byte(s) in %s 0x%08x -> those slots empty\n", bad, why, v);
}
// (s85 slice 5) Sora's Keyblades (sora_kb.h: main + Valor sub-weapon), saved in the slot sidecar ("keyblade", kb_pack;
// missing = Kingdom Key + Star Seeker) + SL_KB_ENV over the hot reload. sora.c kb_send hands the blades to the exe.
#include "sora_kb.h"
#define SL_KB_ENV "SORA_KH2_KB"
static struct { int kb[KB_SLOTS]; uint32_t own; } KB = {{KB_KINGDOM_KEY, KB_STAR_SEEKER, 0, 0}, KB_OWN_ALWAYS};  // (s89 kb) own: sora_unlock.h
static bool ul_kb_ok(int id);  // (s89 kb, sora_unlock.h) owned, or the owned filter / kball off
static int g_ul_noside_reset = 1;  // (s89 review) live "set ulnoside 0" = a slot without a sidecar keeps the owned set it had
static void kb_log(const char *why) {
    LOG("[sora] keyblade main = %s (%s)\n", kb_name(KB.kb[KB_MAIN]), why);
    LOG("[sora] keyblade valor = %s (%s)\n", kb_name(KB.kb[KB_VALOR]), why);
    LOG("[sora] keyblade master = %s, final = %s (%s; s91 mfkb %s)\n", KB.kb[KB_MASTER] ? kb_name(KB.kb[KB_MASTER]) : "auto (Valor's)",
        KB.kb[KB_FINAL] ? kb_name(KB.kb[KB_FINAL]) : "auto (Valor's)", why, g_mfkb ? "on" : "off: both use Valor's slot");
}
// (s91 mfkb) Master + Final slots (kb_load_v just zeroed them): the sidecar "keyblade2" (found) or, in an old save, the
// owned-Keyblade defaults; KB.own must be set first
static void kb_load_forms(int found, uint32_t v, const char *why) {
    int bad = found ? kb_unpack2(v, KB.kb) : 0, n = 0;  // (s118: no defaults here: the unlock state may not be read yet ->
                                                       //  kb_forms_sync at the menu open fills only the unlocked forms)
    if (bad) LOG("[sora] keyblade: %d bad Master / Final slot(s) in %s 0x%08x -> auto\n", bad, why, v);
    if (n) LOG("[sora] keyblade: %d Master / Final slot(s) given a default owned Keyblade (%s)\n", n, why);
    kb_log(why);
}
static void kb_load_v(int found, uint32_t v, const char *why) {
    int bad = kb_load(found, v, KB.kb);
    if (bad) LOG("[sora] keyblade: %d bad slot(s) in %s 0x%08x -> back to the default\n", bad, why, v);
    kb_log(why);
}
// (s110, user: "I won't be able to use AP Ups ... unlock AP more while leveling") KH2's AP = the lvup row + AP Boost items;
// BotW has no AP Boosts, so each level past 1 adds sl_aplv extra AP (live "set aplv N", default 1, 0 = KH2's lvup AP only)
static int sl_aplv = 1;
static int ab_ap_max(void) {
    if (ab_all()) return 999;
    int l = sl_lv();
    l = l < 1 ? 1 : l > 99 ? 99 : l;
    return 2 + SL_LV[l - 1].ap + sl_aplv * (l - 1);  // (s83: devlv too)
}
static int ab_order(uint16_t order[AB_MAX_LEARN]) {
    if (ab_all()) {  // (s91 review, user: debugall = EVERY row of the Abilities page learned, not only the reached LVs)
        int n = 0;
        for (int i = 0; i < AB_N && n < AB_MAX_LEARN; i++)
            if (ab_idx(AB_TAB[i].id) >= 0) order[n++] = AB_TAB[i].id;
        return n;
    }
    uint16_t lvab[99];
    for (int l = 0; l < 99; l++) lvab[l] = SL_LV[l].ab[sl_dream()];
    int n = ab_learned(lvab, sl_lv(), vl_lv(), wl_lv(), ll_lv(), ml_lv(), fl_lv(), order);
    abm_t have = ab_mask(order, n);  // (s102 b16-chestabil: the chest ones after the level ones)
    for (int i = 0; i < AB_N && n < AB_MAX_LEARN; i++)
        if (ab_idx(AB_TAB[i].id) >= 0 && (CA.mask >> i & 1) && !(have >> i & 1)) order[n++] = AB_TAB[i].id;
    return n;
}
static const char *AB_NEW_NAME(int id) {  // (s91) names of the appended rows (03system item table, sys.bar US)
    static const struct { uint16_t id; const char *n; } T[] = {
        {82, "Guard"}, {137, "Upper Slash"}, {271, "Horizontal Slash"}, {267, "Finishing Leap"}, {273, "Retaliating Slash"},
        {262, "Slapshot"}, {263, "Dodge Slash"}, {559, "Flash Step"}, {264, "Slide Dash"}, {562, "Vicinity Break"},
        {265, "Guard Break"}, {266, "Explosion"}, {269, "Aerial Sweep"}, {560, "Aerial Dive"}, {270, "Aerial Spiral"},
        {272, "Aerial Finish"}, {561, "Magnet Burst"}, {268, "Counterguard"}, {198, "Trinity Limit"}, {568, "Auto Limit"},
        {387, "Auto Master"}, {388, "Auto Final"}, {138, "Scan"}, {158, "Aerial Recovery"}, {539, "Combo Master"}, {564, "Dodge Roll"} };
    for (int i = 0; i < (int)(sizeof T / sizeof T[0]); i++) if (T[i].id == id) return T[i].n;
    return "?";
}
static const char *ab_name(int id) {
    return id == AB_AUTO_VALOR ? "Auto Valor" : id == AB_AUTO_WISDOM ? "Auto Wisdom" : ab_idx(id) < 0 ? "?" : ab_idx(id) >= AB_N_OLD ? AB_NEW_NAME(id) : sl_abil_name(id);
}
static void ab_log(const char *why) {
    uint16_t o[AB_MAX_LEARN];
    int n = ab_order(o);
    char b[1200];  // (s91 review: 50 rows)
    size_t k = 0;
    b[0] = 0;
    for (int i = 0; i < n && k + 40 < sizeof b; i++)
        k += (size_t)snprintf(b + k, sizeof b - k, "%s%s%s", k ? ", " : "", ab_on(AB.on, o[i]) ? "" : "-", ab_name(o[i]));
    LOG("[sora] abilities (%s): AP %d/%d, %d learned: %s\n", why, ab_used(AB.on), ab_ap_max(), n, n ? b : "none");
}
static void sl_persist_save(void);
static void ab_tick(void) {
    dbg_tick();  // (b20-lynel: debugall mode 2 keeps its real-EXP snapshot current)
    dbm_ab_tick();  // (s141 dbglv: the All Abilities layer after a hot reload / a save)
    int key = sl_lv() | sl_dream() << 7 | (vl_lv() >= 2) << 9 | (wl_lv() >= 2) << 10 | (ll_lv() >= 2) << 11 | (ml_lv() >= 2) << 12 | (fl_lv() >= 2) << 13 | g_abilfix << 14 | (ab_all() != 0) << 15;
    if (key == AB.key && !AB.dirty) return;
    AB.key = key, AB.dirty = false;
    uint16_t o[AB_MAX_LEARN];
    int n = ab_order(o);
    // (s91 review) abilfix 0 hides rows 25..: keep their on bits (else the sync drops them and, already seen, they stay off
    // after "set abilfix 1")
    abm_t hide = g_abilfix ? 0 : AB.on & ~(((abm_t)1 << AB_N_OLD) - 1);
    bool ch = ab_sync(&AB.on, &AB.seen, o, n, ab_ap_max());
    AB.on |= hide;
    if (ch) ab_log("sync"), sl_persist_save();
}
// live / menu: want 1 on, 0 off -> 1 on, 0 off, -1 refused (not learned / AP short; logged)
static int ab_set(int id, int want) {
    uint16_t o[AB_MAX_LEARN];
    abm_t learned = ab_mask(o, ab_order(o));
    int r = ab_on(AB.on, id) == (want != 0) && (learned & ab_bit(id)) ? want != 0 : ab_toggle(&AB.on, learned, id, ab_ap_max());
    LOG("[sora] ability %d %s -> %s (AP %d/%d)\n", id, ab_name(id), r < 0 ? "REFUSED" : r ? "on" : "off", ab_used(AB.on), ab_ap_max());
    if (r >= 0) sl_persist_save();
    return r;
}

// live / camp page: slot 0..3 <- spell 0..3 or CUST_EMPTY -> 1 changed, 0 same, -1 refused (logged)
static int sc_set(int slot, int spell, const char *why) {
    int r = cust_set(SC.sc, slot, spell);
    if (r < 0) LOG("[sora] shortcut %d = %d REFUSED (%s)\n", slot, spell, why);
    else LOG("[sora] shortcut %d = %s (%s%s)\n", slot, cust_str(spell), why, r ? "" : ", already");
    if (r > 0) sl_persist_save();
    return r;
}

// (s85) live / camp page: slot KB_MAIN / KB_VALOR <- a Keyblade item id -> 1 changed, 0 same, -1 refused (logged)
static int kb_set_v(int slot, int id, const char *why) {
    int r = kb_index(id) >= 0 && !ul_kb_ok(id) ? -1 : kb_set(KB.kb, slot, id);  // (s89 kb: not owned = refused)
    if (r < 0) LOG("[sora] keyblade %d = 0x%x REFUSED (%s)\n", slot, id, why);
    else LOG("[sora] camp: keyblade %s -> %s (%s%s)\n", slot == KB_MAIN ? "main" : slot == KB_VALOR ? "valor" : slot == KB_MASTER ? "master" : "final", kb_name(id), why, r ? "" : ", already");
    if (r > 0) sl_persist_save();
    return r;
}
// (s118 kbswap) the Equipment page picked a Keyblade another slot holds: slot takes id, the holder takes slot's old one
static const char *kb_slot_name(int s) { return s == KB_MAIN ? "main" : s == KB_VALOR ? "valor" : s == KB_MASTER ? "master" : "final"; }
static int kb_swap_v(int slot, int id, const char *why) {
    int o = kb_holder(KB.kb, slot, id), old = slot >= 0 && slot < KB_SLOTS ? KB.kb[slot] : 0;
    int r = !ul_kb_ok(id) ? -1 : kb_swap(KB.kb, slot, id);
    if (r < 0) { LOG("[sora] keyblade swap %d = 0x%x REFUSED (%s)\n", slot, id, why); return r; }
    LOG("[sora] camp: keyblade swap: %s -> %s, %s -> %s (%s)\n", kb_slot_name(slot), kb_name(id), kb_slot_name(o), kb_name(old), why);
    sl_persist_save();
    return r;
}
// (s118) a locked Drive Form's slot (row hidden) gives its Keyblade back; an unlocked one with none gets its default
static void kb_forms_sync(int forms, const char *why) {
    int c = kb_lock_forms(KB.kb, forms), n = kb_form_defaults_m(KB.kb, kb_own_fix(KB.own, KB.kb), forms);
    if (c) LOG("[sora] keyblade: %d locked Drive Form slot(s) freed (Master %s, Final %s; %s)\n", c, forms & 1 ? "open" : "locked", forms & 2 ? "open" : "locked", why);
    if (n) LOG("[sora] keyblade: %d unlocked Drive Form slot(s) given a default owned Keyblade (%s)\n", n, why);
    if (c || n) kb_log(why), sl_persist_save();
}

static void sl_persist_save(void) {
    char b[256];
    snprintf(b, sizeof b, "v2 exp=%u kills=%u lvdmg=%d lvlog=%d khdmg=%d win=%d slot=%d base=%d floor=%d seq=%llu", SL.exp,
             SL.kills, SL.lvdmg, SL.lvlog, SL.khdmg, SL.win, SLS.slot, SLS.base, SLS.floor, (unsigned long long)SLS.seq);
    SetEnvironmentVariableA(SL_ENV, b);
    snprintf(b, sizeof b, "v2 on=%llu seen=%llu", (unsigned long long)ab_real_on(), (unsigned long long)ab_real_seen());  // (s83: never the devlv masks)
    SetEnvironmentVariableA(SL_AB_ENV, b);
    {   // (s102 b16-chestabil)
        char c[64];
        snprintf(c, sizeof c, "v2 mask=%llu", (unsigned long long)CA.mask);
        SetEnvironmentVariableA(SL_CA_ENV, c);
    }
    snprintf(b, sizeof b, "v1 sc=%u", cust_pack(SC.sc));
    SetEnvironmentVariableA(SL_SC_ENV, b);
    snprintf(b, sizeof b, "v1 kb=%u own=%u kb2=%u", kb_pack(KB.kb), KB.own, kb_pack2(KB.kb));  // (s85; s89 kb: + the owned set)
    SetEnvironmentVariableA(SL_KB_ENV, b);
    snprintf(b, sizeof b, "v1 ec=%d", sl_expcurve);  // (s88)
    SetEnvironmentVariableA("SORA_LV_EXPCURVE", b);
}
static bool sl_persist_load(void) {
    char b[256];
    DWORD n = GetEnvironmentVariableA(SL_ENV, b, sizeof b);
    if (!n || n >= sizeof b) return false;
    char a[64];
    unsigned long long ao = 0, as = 0;
    DWORD an = GetEnvironmentVariableA(SL_AB_ENV, a, sizeof a);
    if (an && an < sizeof a && (sscanf(a, "v2 on=%llu seen=%llu", &ao, &as) == 2 || sscanf(a, "v1 on=%llu seen=%llu", &ao, &as) == 2)) AB.on = ao, AB.seen = as, AB.dirty = true;  // (s91: v2 = 64-bit masks)
    {   // (s102 b16-chestabil)
        char c[64];
        unsigned long long cm = 0;
        DWORD cl = GetEnvironmentVariableA(SL_CA_ENV, c, sizeof c);
        if (cl && cl < sizeof c && sscanf(c, "v2 mask=%llu", &cm) == 1) CA.mask = cm, AB.dirty = true;
    }
    unsigned sv = 0;
    an = GetEnvironmentVariableA(SL_SC_ENV, a, sizeof a);
    if (an && an < sizeof a && sscanf(a, "v1 sc=%u", &sv) == 1) sc_load(1, sv, "the hot reload env"), sc_log("kept over the hot reload");
    an = GetEnvironmentVariableA(SL_KB_ENV, a, sizeof a);  // (s85)
    unsigned so = 0, k2 = 0;
    if (an && an < sizeof a && sscanf(a, "v1 kb=%u own=%u kb2=%u", &sv, &so, &k2) >= 1) {
        KB.own = so | KB_OWN_ALWAYS, kb_load_v(1, sv, "kept over the hot reload");
        kb_load_forms(sscanf(a, "v1 kb=%*u own=%*u kb2=%u", &k2) == 1, k2, "kept over the hot reload");  // (s91 mfkb)
    }
    int ec = 0;
    an = GetEnvironmentVariableA("SORA_LV_EXPCURVE", a, sizeof a);  // (s88)
    if (an && an < sizeof a && sscanf(a, "v1 ec=%d", &ec) == 1) sl_expcurve = ec >= 0 && ec <= 3 ? ec : 0;
    unsigned e = 0, k = 0;
    unsigned long long q = 0;
    int d = 1, lg = 1, kd = 100, w = 10, sl = -1, ba = -1, fl = 1;
    if (sscanf(b, "v2 exp=%u kills=%u lvdmg=%d lvlog=%d khdmg=%d win=%d slot=%d base=%d floor=%d seq=%llu", &e, &k, &d, &lg,
               &kd, &w, &sl, &ba, &fl, &q) != 10 &&
        sscanf(b, "v1 exp=%u kills=%u lvdmg=%d lvlog=%d khdmg=%d win=%d", &e, &k, &d, &lg, &kd, &w) != 6)
        return false;
    SL.exp = e > sl_exp_max() ? sl_exp_max() : e, SL.kills = k, SL.lvdmg = d != 0, SL.lvlog = lg != 0;
    SL.khdmg = kd < 1 ? 1 : kd, SL.win = w < 1 ? 1 : w;
    SLS.slot = sl >= 0 && sl < SL_SLOTS ? sl : -1, SLS.base = ba, SLS.floor = fl != 0, SLS.seq = q;
    return true;
}

static const char *sl_root(void) {  // <save dir>/save_0 (the fs events set it from their path)
    if (!SLS.root[0]) {
        const char *d = getenv("SAVAGEWIND_SAVE_DIR");
        if ((!d || !*d) && SLX.save_dir && *SLX.save_dir) d = *SLX.save_dir;
        snprintf(SLS.root, sizeof SLS.root, "%s/save_0", d && *d ? d : "save");
    }
    return SLS.root;
}
static uint32_t sl_crc32(const char *s) {  // (zlib's crc32: the save file's flag name hash)
    uint32_t c = 0xffffffffu;
    for (; *s; s++) {
        c ^= (uint8_t)*s;
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
    }
    return ~c;
}
typedef struct { uint32_t h; uint8_t cat; } SlFlag;  // cat 0 paraglider, 1 beast, 2 tower, 3 shrine
static int sl_flag_cmp(const void *a, const void *b) {
    uint32_t x = ((const SlFlag *)a)->h, y = ((const SlFlag *)b)->h;
    return x < y ? -1 : x > y;
}
// Story progress of a save (VERIFIED names: S54_HELPER_LV_LEVELS.md 4.1): IsGet_PlayerStole2, Clear_Remains{Water,
// Wind, Fire, Electric}, MapTower_01..15, Clear_Dungeon000..159 (names that do not exist simply never match).
static SlStory sl_story(int slot) {
    static SlFlag fl[1 + 4 + 15 + 160];
    static int nfl;
    SlStory st = {0};
    if (!nfl) {
        char nm[40];
        static const char *const beast[4] = {"Water", "Wind", "Fire", "Electric"};
        fl[nfl].h = sl_crc32("IsGet_PlayerStole2"), fl[nfl++].cat = 0;
        for (int i = 0; i < 4; i++) snprintf(nm, sizeof nm, "Clear_Remains%s", beast[i]), fl[nfl].h = sl_crc32(nm), fl[nfl++].cat = 1;
        for (int i = 1; i <= 15; i++) snprintf(nm, sizeof nm, "MapTower_%02d", i), fl[nfl].h = sl_crc32(nm), fl[nfl++].cat = 2;
        for (int i = 0; i < 160; i++) snprintf(nm, sizeof nm, "Clear_Dungeon%03d", i), fl[nfl].h = sl_crc32(nm), fl[nfl++].cat = 3;
        qsort(fl, (size_t)nfl, sizeof *fl, sl_flag_cmp);
    }
    char p[1100];
    snprintf(p, sizeof p, "%s/%d/game_data.sav", sl_root(), slot);
    FILE *f = fopen(p, "rb");
    if (!f) return st;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = size >= 0x1000 && size <= (8 << 20) ? (uint8_t *)malloc((size_t)size) : NULL;
    bool rd = d && fread(d, 1, (size_t)size, f) == (size_t)size;
    fclose(f);
    if (!rd) { free(d); return st; }
    uint8_t seen[sizeof fl / sizeof fl[0]] = {0};
    for (long o = 0x0c; o + 8 <= size; o += 8) {
        SlFlag key;
        uint32_t v;
        memcpy(&key.h, d + o, 4), memcpy(&v, d + o + 4, 4);
        const SlFlag *hit = (const SlFlag *)bsearch(&key, fl, (size_t)nfl, sizeof *fl, sl_flag_cmp);
        if (!hit || v != 1 || seen[hit - fl]++) continue;
        if (hit->cat == 0) st.glider = 1;
        else if (hit->cat == 1) st.beasts++;
        else if (hit->cat == 2) st.towers++;
        else st.shrines++;
    }
    free(d);
    st.ok = true, st.prog = st.glider + st.beasts + st.towers + st.shrines;
    return st;
}
static int sl_floor_lv(const SlStory *s) {  // (GUESS, user's call: doc sec 5 "LV 5 + 6 per beast")
    if (!s->ok || !s->glider) return 1;
    int l = 5 + 6 * s->beasts;
    return l > 99 ? 99 : l;
}

// (s128 wpnmenu) sora_wpn.h owns these (included later); declared here for the slot sidecar's "wpnuse"
static int g_wpn_use;
static char g_wpn_last[48];

// tiny JSON reads (our own file: flat keys)
static bool sl_json_u32(const char *b, const char *key, uint32_t *v) {
    char k[48];
    snprintf(k, sizeof k, "\"%s\"", key);
    const char *p = strstr(b, k);
    if (!p || !(p = strchr(p + strlen(k), ':'))) return false;
    char *e;
    unsigned long x = strtoul(p + 1, &e, 10);
    if (e == p + 1) return false;
    *v = (uint32_t)x;
    return true;
}
static bool sl_json_dream(const char *b, const char *key, int *w) {  // "sword" / "shield" / "staff" / "-" (-1)
    char k[48];
    snprintf(k, sizeof k, "\"%s\"", key);
    const char *p = strstr(b, k);
    if (!p || !(p = strchr(p + strlen(k), ':')) || !(p = strchr(p, '"'))) return false;
    *w = -1;
    for (int i = 0; i < 3; i++)
        if (!strncmp(p + 1, SL_DREAM_NAME[i], strlen(SL_DREAM_NAME[i])) && p[1 + strlen(SL_DREAM_NAME[i])] == '"') *w = i;
    return true;
}
static const char *sl_dream_str(int w) { return w >= 0 && w <= 2 ? SL_DREAM_NAME[w] : "-"; }

static bool sl_sidecar_write(int slot, uint32_t exp, uint32_t kills, const SlStory *st) {
    char p[1100], t[1110], b[2048];  // (s89 review: was 900 - the s89 keys (kb_owned, limit, master, final, anti) came close)
    snprintf(p, sizeof p, "%s/%d/" SL_SIDECAR, sl_root(), slot);
    snprintf(t, sizeof t, "%s.tmp", p);
    SYSTEMTIME now;
    GetLocalTime(&now);
    char roxs[80];  // (s111 roxload) auto mode only: a forced body (set char 1 / 2) says nothing about the story
    int rn = snprintf(roxs, sizeof roxs, "  \"wpnuse\": %d,\n", g_wpn_use != 0);  // (s128 wpnmenu) 1 = Link's BotW weapon, 0 = the Keyblade
    if (g_char == 0 && !g_dbm_sora) snprintf(roxs + rn, sizeof roxs - rn, "  \"roxas\": %d,\n", rox_on_now());
    int n = snprintf(b, sizeof b,
                     "{\n  \"version\": 1,\n  \"what\": \"Sora in BotW: Sora's KH2 level for this BotW save slot (sora_mod.dll, "
                     "recon/S55_HELPER_LEVELS23.md)\",\n  \"slot\": %d,\n  \"exp\": %u,\n  \"lv\": %d,\n  \"kills\": %u,\n"
                     "  \"drive\": %d,\n  \"abil_on\": %u,\n  \"abil_seen\": %u,\n  \"abil_on_hi\": %u,\n  \"abil_seen_hi\": %u,\n  \"chest_mask\": %u,\n  \"chest_mask_hi\": %u,\n  \"shortcuts\": %u,\n  \"keyblade\": %u,\n  \"keyblade2\": %u,\n  \"kb_owned\": %u,\n  \"valor\": %u,\n  \"wisdom\": %u,\n  \"limit\": %u,\n  \"master\": %u,\n  \"final\": %u,\n  \"anti\": %u,\n  \"summon\": %u,\n%s  \"dream\":\"%s\",\n  \"dream_giveup\": \"%s\",\n  \"story\": {\"ok\": %d, \"paraglider\": %d, "
                     "\"beasts\": %d, \"towers\": %d, \"shrines\": %d},\n  \"saved\": \"%04d-%02d-%02d %02d:%02d:%02d\"\n}\n",
                     slot, exp, sl_level_of(exp), kills, dg_points(), (uint32_t)ab_real_on(), (uint32_t)ab_real_seen(), (uint32_t)(ab_real_on() >> 32), (uint32_t)(ab_real_seen() >> 32), (uint32_t)CA.mask, (uint32_t)(CA.mask >> 32), cust_pack(SC.sc), kb_pack(KB.kb), kb_pack2(KB.kb), kb_own_fix(KB.own, KB.kb), vl_exp_get(), wl_exp_get(), ll_exp_get(), ml_exp_get(), fl_exp_get(), anti_pts_get(), su_exp_get(), roxs, sl_dream_str(dream_weapon), sl_dream_str(dream_giveup), st->ok,
                     st->glider, st->beasts, st->towers, st->shrines, now.wYear, now.wMonth, now.wDay, now.wHour,
                     now.wMinute, now.wSecond);
    if (n < 0 || n >= (int)sizeof b) { LOG("[sora] levels: sidecar text too long (%d), not written\n", n); return false; }  // (s89 review)
    FILE *f = fopen(t, "wb");
    bool ok = f && fwrite(b, 1, (size_t)n, f) == (size_t)n;
    if (f) ok = !fclose(f) && ok;
    ok = ok && MoveFileExA(t, p, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!ok) LOG("[sora] levels: could not write %s (%lu)\n", p, GetLastError()), DeleteFileA(t);
    return ok;
}

// BotW loaded slot N (or the boot guess / live "set slot N"): this state becomes that save's.
static void orb_slot_load(int slot);   // (s131 orbs, sora_orbs.h)
static void orb_slot_saved(int slot);
static void sl_slot_load(int slot, const char *why) {
    int dbg = dbg_suspend("BotW load / set slot");  // (s90 debugall: this load's values are the real ones)
    int dbm = dbm_suspend();  // (s140 dbgmenu: the Keyblade slots really owned)
    char p[1100], b[2048];
    snprintf(p, sizeof p, "%s/%d/" SL_SIDECAR, sl_root(), slot);
    SlStory st = sl_story(slot);
    FILE *f = fopen(p, "rb");
    size_t n = f ? fread(b, 1, sizeof b - 1, f) : 0;
    if (f) fclose(f);
    b[n] = 0;
    uint32_t exp = 0, kills = 0;
    int dw = -1, dg = -1;
    bool side = n && sl_json_u32(b, "exp", &exp);
    g_wpn_use = 1, g_wpn_last[0] = 0;  // (s128 wpnmenu) default = the BotW weapon; a new slot re-learns the equipped one
    const char *how;
    if (side) {
        sl_json_u32(b, "kills", &kills);
        SL.exp = exp > sl_exp_max() ? sl_exp_max() : exp, SL.kills = kills;
        uint32_t dp = 0;
        bool dpk = sl_json_u32(b, "drive", &dp);  // (s89 trial: loaded AFTER the form EXPs below - the gauge max grows with them)
        uint32_t ve = 0;
        vl_load(sl_json_u32(b, "valor", &ve) ? (int)ve : -1, "slot sidecar");  // (s70 Valor EXP; none = LV1)
        uint32_t we = 0;
        wl_load(sl_json_u32(b, "wisdom", &we) ? (int)we : -1, "slot sidecar");  // (s75 Wisdom EXP; none = LV1)
        uint32_t le = 0;
        ll_load(sl_json_u32(b, "limit", &le) ? (int)le : -1, "slot sidecar");  // (s89 Limit EXP; none = LV1)
        uint32_t me = 0;
        ml_load(sl_json_u32(b, "master", &me) ? (int)me : -1, "slot sidecar");  // (s89 Master EXP; none = LV1)
        uint32_t fe = 0;
        fl_load(sl_json_u32(b, "final", &fe) ? (int)fe : -1, "slot sidecar");  // (s89 b4-final Final EXP; none = LV1)
        uint32_t ap = 0;
        anti_load(sl_json_u32(b, "anti", &ap) ? (int)ap : -1, "slot sidecar");  // (s89 Anti points; none = 0)
        uint32_t se = 0;
        su_load(sl_json_u32(b, "summon", &se) ? (int)se : -1, "slot sidecar");  // (s148 Summon EXP; none = LV1)
        dg_load_points(dpk ? (int)dp : -1, "slot sidecar");  // (s67 drive; none = empty; s89 trial: after the form LVs = its max)
        AB.on = AB.seen = 0;  // (s82 abilities; none = auto-equip)
        {   // (s91: 64-bit masks = the old low 32 bits + abil_*_hi, missing hi = 0)
            uint32_t lo = 0, hi = 0;
            sl_json_u32(b, "abil_on", &lo), sl_json_u32(b, "abil_on_hi", &hi), AB.on = (abm_t)hi << 32 | lo;
            lo = hi = 0;
            sl_json_u32(b, "abil_seen", &lo), sl_json_u32(b, "abil_seen_hi", &hi), AB.seen = (abm_t)hi << 32 | lo;
        }
        if (SL.devlv) AB.dev_on = AB.on, AB.dev_seen = AB.seen;  // (s83b review: under devlv the stash = this slot's real masks)
        {   // (s102 b16-chestabil)
            uint32_t lo = 0, hi = 0;
            sl_json_u32(b, "chest_mask", &lo), sl_json_u32(b, "chest_mask_hi", &hi), CA.mask = (abm_t)hi << 32 | lo;
        }
        uint32_t sv = 0;
        sc_load(sl_json_u32(b, "shortcuts", &sv), sv, "the slot sidecar");  // (s84 shortcuts; none = the default)
        sv = 1;
        g_wpn_use = sl_json_u32(b, "wpnuse", &sv) ? sv != 0 : 1;  // (s128 wpnmenu; missing key = 1)
        sv = 0;
        kb_load_v(sl_json_u32(b, "keyblade", &sv), sv, "the slot sidecar");  // (s85 Keyblades; none = the default)
        sv = 0;  // (s89 kb: the owned set; none = the always ones + the equipped; sora_unlock.h adds the milestones)
        KB.own = kb_own_fix(sl_json_u32(b, "kb_owned", &sv) ? sv : 0, KB.kb);
        sv = 0;
        kb_load_forms(sl_json_u32(b, "keyblade2", &sv), sv, "the slot sidecar");  // (s91 mfkb; none = the owned defaults)
        if (sl_json_dream(b, "dream", &dw)) {
            sl_json_dream(b, "dream_giveup", &dg);
            dream_weapon = dw, dream_giveup = dg != dw ? dg : -1;
#ifdef SORA_DREAM_H
            dream_mem_save();  // (the picker's own memory: it reloads from there)
#endif
        }
        how = "its sidecar";
    } else {
        int fl = SLS.floor ? sl_floor_lv(&st) : 1;
        uint32_t fe = SL_LV[fl - 1].exp;
        if (SLS.slot < 0 && SL.exp > fe) how = "no sidecar: kept the unsaved LV (this state had no slot yet)";
        else cust_default(SC.sc), SL.exp = fe, SL.kills = 0, AB.on = AB.seen = AB.dev_on = AB.dev_seen = 0, CA.mask = 0, sl_forms_reset("no sidecar"), dg_load_points(-1, "no sidecar"), how = SLS.floor && st.glider ? "no sidecar: story floor (GUESS rule)" : "no sidecar: LV 1",
             KB.own = g_ul_noside_reset ? kb_own_fix(0, KB.kb) : KB.own;  // (s89 review: not the previous slot's owned set; sora_unlock.h re-adds this save's milestones)
    }
    {   // (s111 roxload) the body this save was in
        uint32_t rv = 0;
        bool fld = side && sl_json_u32(b, "roxas", &rv) && rv <= 1;
        int h = fld ? (int)rv : st.ok ? (st.glider ? 0 : 1) : -1;
        g_rox_hint = g_roxload && g_char == 0 && !g_dbm_sora ? h : -1, g_rox_hint_at = GetTickCount64();
        if (g_rox_hint >= 0) {
            LOG("[roxas] slot %d: body %s (%s) - applied at once\n", slot, h ? "Roxas" : "Sora", fld ? "sidecar roxas" : "the save file's paraglider");
            if (h == 1 && !rox_on_now()) g_rox_hold = 1;  // (Sora is not drawn while Roxas' assets load)
        }
    }
    SL.lv = sl_level_of(SL.exp);
    AB.dirty = true;
    SLS.slot = slot, SLS.base = st.ok ? st.prog : -1;
    memset(SL.c, 0, sizeof SL.c);  // (a new world: no credit carries over)
    const SlLevel *s = &SL_LV[SL.lv - 1];
    LOG("[sora] slot %d: LV %d EXP %u (next %u) Str %d Mag %d Def %d AP %d, %u kills, Dream Weapon %s (%s; %s). Save: "
        "paraglider %d, beasts %d, towers %d, shrines %d%s\n", slot, SL.lv, SL.exp, sl_next(SL.lv), s->str, s->mag, s->def,
        2 + s->ap, SL.kills, sl_dream_str(dream_weapon), how, why, st.glider, st.beasts, st.towers, st.shrines,
        st.ok ? "" : " (game_data.sav not readable)");
    sc_log("slot load");
    skt_slot_load(slot);  // (s110 skate: this save's skateboard)
    orb_slot_load(slot);  // (s131 orbs: this save's munny; no orbs carry over)
    sl_persist_save();
    dbg_resume(dbg, "load");
    dbm_resume(dbm, true);
}

// BotW wrote slot N: the sidecar gets (exp, kills) = the state when the write began.
static void sl_slot_saved(int slot, uint32_t exp, uint32_t kills, const char *why) {
    int dbg = dbg_suspend("BotW save");  // (s90 debugall: the sidecar gets the real values, never the debug ones)
    int dbm = dbm_suspend();  // (s140 dbgmenu)
    if (dbg) exp = SL.exp, kills = SL.kills;
    if (dbm & 8) exp = SL.exp, kills = SL.kills;  // (s142 dbgexp: All Abilities ON: the write began with the debug EXP)
    SlStory st = sl_story(slot);
    if (st.ok && st.prog == 0 && SLS.base > 0) {
        LOG("[sora] slot %d: a save with no story progress after one with %d (paraglider + towers + shrines + beasts): "
            "New Game -> LV 1, Dream Weapon forgotten (was LV %d EXP %u)\n", slot, SLS.base, SL.lv, SL.exp);
        SL.exp = 0, SL.kills = 0, SL.lv = 1, exp = kills = 0, g_dbm_newgame = 1;
        sl_forms_reset("new game");  // (s89 trial: before the gauge, whose max grows with them)
        dg_load_points(-1, "new game");  // (s67 drive)
        AB.on = AB.seen = AB.dev_on = AB.dev_seen = 0, AB.dirty = true, CA.mask = 0;  // (s82 abilities; s83b: the devlv stash too; s102 chest abilities)
        cust_default(SC.sc);  // (s84 shortcuts)
        kb_default(KB.kb), KB.own = KB_OWN_ALWAYS, kb_log("new game");  // (s85; s89 kb owned set)
        dream_weapon = dream_giveup = -1;
#ifdef SORA_DREAM_H
        dream_mem_save();
#endif
        memset(SL.c, 0, sizeof SL.c);
    }
    bool ok = sl_sidecar_write(slot, exp, kills, &st);
    SLS.slot = slot;
    if (st.ok) SLS.base = st.prog;
    LOG("[sora] slot %d saved (%s): LV %d EXP %u, %u kills, Dream Weapon %s -> %s/%d/" SL_SIDECAR "%s\n", slot, why,
        sl_level_of(exp), exp, kills, sl_dream_str(dream_weapon), sl_root(), slot, ok ? "" : " FAILED");
    skt_slot_saved(slot);  // (s110 skate: the board's place goes with the save)
    orb_slot_saved(slot);  // (s131 orbs: the munny goes with the save)
    sl_persist_save();
    dbg_resume(dbg, "save");
    dbm_resume(dbm, false);
}

static bool sl_stat(int slot, int64_t *mtime, int64_t *size) {
    char p[1100];
    WIN32_FILE_ATTRIBUTE_DATA a;
    snprintf(p, sizeof p, "%s/%d/game_data.sav", sl_root(), slot);
    if (!GetFileAttributesExA(p, GetFileExInfoStandard, &a)) return false;
    *mtime = (int64_t)((uint64_t)a.ftLastWriteTime.dwHighDateTime << 32 | a.ftLastWriteTime.dwLowDateTime);
    *size = (int64_t)((uint64_t)a.nFileSizeHigh << 32 | a.nFileSizeLow);
    return true;
}
static int sl_newest_slot(void) {
    int best = -1;
    int64_t bt = 0, t, z;
    for (int i = 0; i < SL_SLOTS; i++)
        if (sl_stat(i, &t, &z) && (best < 0 || t > bt)) best = i, bt = t;
    return best;
}
static void sl_pend(int slot, uint64_t now) {
    if (!SLS.f[slot].pend) SLS.f[slot].pend = true, SLS.f[slot].exp = SL.exp, SLS.f[slot].kills = SL.kills;
    SLS.f[slot].pend_at = now;
}
static void sl_slot_done(int slot, const char *why) {
    if (!SLS.f[slot].pend) SLS.f[slot].exp = SL.exp, SLS.f[slot].kills = SL.kills;
    SLS.f[slot].pend = false;
    sl_stat(slot, &SLS.f[slot].mtime, &SLS.f[slot].size);  // (known now: the poll does not take it again)
    sl_slot_saved(slot, SLS.f[slot].exp, SLS.f[slot].kills, why);
}

// Once per tick: the exe's slot events (in order), then the mtime poll once a second (both exes; on the new one it only
// finishes a write whose "written out" event never came).
static void sl_slot_tick(void) {
    uint64_t now = GetTickCount64();
    if (SLX.save_ev) {
        uint64_t seq = SLS.seq;
        int slot, kind;
        char dir[1024];
        while (SLX.save_ev(&seq, &slot, &kind, dir, sizeof dir)) {
            SLS.seq = seq;
            if (slot < 0 || slot >= SL_SLOTS) continue;
            char *cut = strrchr(dir, '/');
            if (cut && (cut[1] >= '0' && cut[1] <= '9')) {
                *cut = 0;
                if (strcmp(dir, SLS.root)) LOG("[sora] levels: save root %s (from the fs event)\n", dir);
                snprintf(SLS.root, sizeof SLS.root, "%s", dir);
            }
            if (kind == 0 && SLS.f[slot].pend)  // (a read-back while its own write is pending: the sidecar is not new yet)
                LOG("[sora] slot %d: BotW read it while its write is pending: not loaded (GUESS: a check read)\n", slot);
            else if (kind == 0) {
                sl_slot_load(slot, "BotW read its game_data.sav: Continue / Load");
                // s180: a new world: old remote hits / kills waiting in the exe must not hit the fresh enemies (exe 22:21 s180+; older exe: no-op)
                static int (*edmg_reset)(int *);
                static bool looked;
                if (!looked) looked = true, edmg_reset = (int (*)(int *))(void *)GetProcAddress(GetModuleHandleA(NULL), "linkpos_edmg_reset");
                if (edmg_reset) edmg_reset(NULL);
                else LOG("[sora] slot %d load: this exe has no linkpos_edmg_reset (old remote kills may still apply)\n", slot);
                net_kills_on_load(slot);  // (s196 killsync, after the exe reset: our kill list goes too, then the peers are asked again)
            }
            else if (kind == 1) sl_pend(slot, now);
            else sl_slot_done(slot, "written out (fs event)");
            sl_persist_save();
        }
    }
    if (now < SLS.next_poll) return;
    SLS.next_poll = now + 1000;
    for (int i = 0; i < SL_SLOTS; i++) {
        int64_t t, z;
        if (!sl_stat(i, &t, &z)) continue;
        if (!SLS.polled) { SLS.f[i].mtime = t, SLS.f[i].size = z; continue; }
        if (t != SLS.f[i].mtime || z != SLS.f[i].size) SLS.f[i].mtime = t, SLS.f[i].size = z, sl_pend(i, now);
        else if (SLS.f[i].pend && now - SLS.f[i].pend_at >= 1500)
            sl_slot_done(i, SLX.save_ev ? "mtime settled, no written-out event" : "mtime poll (exe without slot events)");
    }
    if (!SLS.polled) LOG("[sora] levels: watching %s/<0-%d>/game_data.sav (slot %d now)\n", sl_root(), SL_SLOTS - 1, SLS.slot);
    SLS.polled = true;
}

static void sl_init(void) {
    if (SL.init) return;
    SL.init = true;
    dbg_init();  // (s90: a hot reload while debugall is ON keeps its real-value snapshot)
    dbm_init();  // (s140 dbgmenu: the Debug Menu toggles likewise)
    sl_exe_bind();
    bool kept = sl_persist_load();
    const char *src = kept ? "kept over the hot reload" : "new game process";
    if (!kept) {
        const char *e = getenv("SAVAGEWIND_SORA_EXP"), *l = getenv("SAVAGEWIND_SORA_LV"), *d = getenv("SAVAGEWIND_SORA_LVDMG");
        if (l && *l) { int v = atoi(l); SL.exp = SL_LV[(v < 1 ? 1 : v > 99 ? 99 : v) - 1].exp; }
        if (e && *e) { long v = atol(e); SL.exp = v < 0 ? 0 : (uint32_t)v > sl_exp_max() ? sl_exp_max() : (uint32_t)v; }
        if (d && *d) SL.lvdmg = atoi(d) != 0;
        bool envst = (l && *l) || (e && *e);
        int newest = envst || SLX.save_ev ? -1 : sl_newest_slot();
        if (newest >= 0) {
            sl_slot_load(newest, "the newest save at boot = Continue's pick (exe without slot events, GUESS)");
            src = "new game process; the newest save's sidecar";
        } else
            src = envst ? "new game process; env start values (a slot event / Load replaces them)"
                  : SLX.save_ev ? "new game process; the slot comes with BotW's read of its save" : "new game process; no save found";
    }
    SL.lv = sl_level_of(SL.exp);
    const SlLevel *s = &SL_LV[SL.lv - 1];
    LOG("[sora] levels: LV %d EXP %u (next %u) Str %d Mag %d Def %d AP %d, %d kills, Dream Weapon %s, damage %s x%d%%, "
        "kill window %d s, slot %d (%s)\n", SL.lv, SL.exp, sl_next(SL.lv), s->str, s->mag, s->def, 2 + s->ap, SL.kills,
        SL_DREAM_NAME[sl_dream()], SL.lvdmg ? "KH-relative" : "old flat rule", SL.khdmg, SL.win, SLS.slot, src);
    sl_persist_save();
}

// ------------------------------------------------------------------ level-up feedback (s55, DLL-only stand-in)
// sfx.bin is read once at game start (exe), so the chime plays through winmm's PlaySound from its own wav: KH2 se000
// #25 (GUESS: the level-up per a PHH note, VERIFY by ear; copied at its KH2 volume 0.9). The burst = KH2 effects
// fx.bin already has (GUESS stand-in: hit_finish flash + cure_sparkle at his chest) until slice 4 bakes the real one.
// Live: set lvup 1 (show it now), set lvse 0/1, set lvfx 0/1.
#define SL_LVUP_WAV "D:/SoraBotW/data/sfx/extra/se/se000/025.wav"
static int sl_lvup_queue, sl_lvup_se = 1, sl_lvup_fx = 1;
static void sl_lvup_sound(void) {
    static BOOL (WINAPI *ps)(LPCSTR, HMODULE, DWORD);
    static int tried;
    if (!tried) {
        tried = 1;
        HMODULE w = LoadLibraryA("winmm.dll");
        ps = w ? (BOOL (WINAPI *)(LPCSTR, HMODULE, DWORD))(void *)GetProcAddress(w, "PlaySoundA") : NULL;
        if (!ps) LOG("[sora] level-up chime: no PlaySoundA (winmm)\n");
    }
    const char *p = getenv("SAVAGEWIND_SORA_LVUP_SE");
    if (ps) ps(sora_path(p && *p ? p : SL_LVUP_WAV), NULL, 0x20000 | 0x2 | 0x1);  // (SND_FILENAME | SND_NODEFAULT | SND_ASYNC)
}
// sora.c calls this every frame; one burst + one chime however many levels came at once (KH2 re:0x3ED190 starts
// system element 15 once per batch, VERIFIED S55_HELPER_BAKE.md). With the s55 packs (fx "level_up" = element 15,
// sfx "level_up" = se000 #25) the real ones play, bound to his centre like g_cure_fx; older packs: the stand-ins.
static int sl_lvup_h = -1;
static void sl_lvup_show(const float feet[3], float face, float to_u) {
    float ctr[3] = {feet[0], feet[1] + 90 * to_u, feet[2]};  // (GUESS 90 cm: spell_spawn's Cure centre)
    if (sl_lvup_h >= 0) { if (sora_fx_alive(sl_lvup_h)) sora_fx_move(sl_lvup_h, ctr, face); else sl_lvup_h = -1; }
    if (!sl_lvup_queue) return;
    int n = sl_lvup_queue;
    sl_lvup_queue = 0;
    bool real_fx = sora_fx_id("level_up") >= 0, real_se = sora_sfx_id("level_up") >= 0;
    int a = -1, b = -1;
    if (sl_lvup_fx && real_fx) a = sl_lvup_h = sora_fx_event("level_up", ctr, face, to_u);
    else if (sl_lvup_fx) a = sora_fx_event("hit_finish", ctr, face, 1.6f * to_u), b = sora_fx_event("cure_sparkle", ctr, face, to_u);
    if (sl_lvup_se && real_se) sora_sfx_event("level_up", 1.0f);
    else if (sl_lvup_se) sl_lvup_sound();
    LOG("[sora] level-up shown (%d level%s): chime %s (%s), fx %s %d %d\n", n, n > 1 ? "s" : "", sl_lvup_se ? "on" : "off",
        real_se ? "sfx.bin" : "PlaySound wav", real_fx ? "KH2 element 15" : "stand-in", a, b);
}

// ------------------------------------------------------------------ (s55 HUD) KH2's level-up panel (exe side)
// The exe's sora_hud_levelup (recon/wip_s55_hud/sora_hud.c, next exe link; S55_HELPER_HUD.md) draws KH2's own red
// "LEVEL UP! LV. n  Sora" panel with KH2's lines. Looked up by name, so this DLL still loads on an older exe (then only
// the chime + burst above show; logged once). KH2's lines (re:0x3ED7D0, PHH levelup_kh2.kh2_lines): the stats that
// grew over the previous level in the order Strength / Magic / Defense / Maximum AP, then the level's ability for the
// Dream Weapon kept; the first two go on the panel (no KH2 row has more). One panel per level, like KH2 (the exe
// queues them: up to 4 at once, stacked). Live: set lvpanel 0/1; "set lvup 1" also shows the current level's panel.
#include "sora_hud.h"  // (SoraHudLevelUp, SORA_LVUP_*: header only, safe on an older exe)
static int sl_lvup_panel = 1;
static void sl_lvup_hud(int lv) {
    static void (*fn)(const SoraHudLevelUp *);
    static int tried;
    if (!tried) {
        tried = 1;
        fn = (void (*)(const SoraHudLevelUp *))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_levelup");
        LOG("[sora] level-up panel: sora_hud_levelup %s\n", fn ? "found" : "not in this exe (the panel needs the next exe link)");
    }
    if (!fn || !sl_lvup_panel || lv < 2 || lv > 99) return;
    const SlLevel *s = &SL_LV[lv - 1], *p = &SL_LV[lv - 2];
    uint32_t cand[5];
    int n = 0, ab = s->ab[sl_dream()];
    if (sl_lvstr && s->str > p->str) cand[n++] = SORA_LVUP_STR;  // (s87: no Str line while Str is fixed)
    if (sl_lvmag && s->mag > p->mag) cand[n++] = SORA_LVUP_MAG;  // (s87b: no Mag line while Mag is fixed)
    if (s->def > p->def) cand[n++] = SORA_LVUP_DEF;
    if (s->ap > p->ap) cand[n++] = SORA_LVUP_AP;
    if (ab) cand[n++] = SORA_LVUP_ABILITY(ab);
    SoraHudLevelUp u = {.level = lv};
    for (int i = 0; i < n && u.n_lines < 2; i++) u.line[u.n_lines++] = cand[i];
    fn(&u);
}
// (s76) a Drive Form's level-up panel: "LV. n  Valor" / "Wisdom" + the level's line (00battle.bin fmlv @0x3446C: the
// level's ability, or on growth levels 3/5/7 the form's growth ability High Jump / Quick Run LV1-3). The panels of
// levels lv-n+1..lv (the exe queues them); the form's name label is in lvup.bin since s76 (older bake = "Sora").
static void sl_form_lvup_hud(int form, int lv, int n) {
    // (s89) Limit (FM form 3): LV2 568 Auto Limit, LV4 405 Draw, LV6 407 Lucky Lucky, growth Dodge Roll LV1-3 564-566
    // (s89) Master (form 4): fmlv 0x42..0x47 LV2 387 Auto Master, LV4 / LV6 163 Air Combo Plus, growth Aerial Dodge 102-104
    // (s89 trial) Final (form 5): fmlv 0x52..0x57 LV2 388 Auto Final, LV4 / LV6 398 Form Boost, growth Glide LV1-3 106-108
    // (03system item names VERIFIED s89 review); its name = sys.bar 488 "Final Form" first word (lvup.bin fn5, s89 rebake)
    static const uint16_t AB[6][8] = {{0}, {0, 0, 385, 94, 162, 95, 162, 96}, {0, 0, 386, 98, 412, 99, 413, 100},
                                      {0, 0, 568, 564, 405, 565, 407, 566}, {0, 0, 387, 102, 163, 103, 163, 104},
                                      {0, 0, 388, 106, 398, 107, 398, 108}};
    static void (*fn)(const SoraHudLevelUp *);
    if (!fn) fn = (void (*)(const SoraHudLevelUp *))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_levelup");
    if (!fn || !sl_lvup_panel || form < 1 || form > 5) return;
    for (int l = lv - n + 1; l <= lv; l++) {
        if (l < 2 || l > 7) continue;
        SoraHudLevelUp u = {.level = l, .n_lines = 1, .line = {SORA_LVUP_ABILITY(AB[form][l])}, .name = SORA_LVUP_NAME_FORM(form)};
        fn(&u);
    }
}

// ------------------------------------------------------------------ EXP and levels
static void sl_level_up_log(int lv) {
    const SlLevel *s = &SL_LV[lv - 1];
    int id = s->ab[sl_dream()];
    LOG("[sora] LEVEL UP -> LV %d (Str %d Mag %d Def %d AP %d; ability %s%s%s)\n", lv, s->str, s->mag, s->def, 2 + s->ap,
        sl_abil_name(id), id ? ", Dream Weapon " : "", id ? SL_DREAM_NAME[sl_dream()] : "");
    sl_lvup_queue++;
    sl_lvup_hud(lv);  // (s55 HUD: KH2's panel; the exe paces it, the chime + burst stay one per batch in sl_lvup_show)
    // TODO(slice 6): the ability's effect + AP equip.
}
static void sl_set_exp(uint32_t exp, const char *why) {
    int old = SL.lv;
    SL.exp = exp > sl_exp_max() ? sl_exp_max() : exp;
    SL.lv = sl_level_of(SL.exp);
    if (SL.lv - old <= 3) for (int l = old + 1; l <= SL.lv; l++) sl_level_up_log(l);  // (bigger jumps: the summary only)
    char ab[768];
    sl_abil_list(SL.lv, ab, sizeof ab);
    const SlLevel *s = &SL_LV[SL.lv - 1];
    LOG("[sora] LEVEL %s: LV %d EXP %u (next %u) Str %d Mag %d Def %d AP %d; abilities (%s): %s\n", why, SL.lv, SL.exp,
        sl_next(SL.lv), s->str, s->mag, s->def, 2 + s->ap, SL_DREAM_NAME[sl_dream()], ab);
    sl_persist_save();
}
static void sl_gain(const SlCredit *c, const char *how) {
    int old = SL.lv;
    uint32_t base = sl_curve_exp(c->exp, c->kh_level, SL.lv);  // (s88 expcurve; 0 = unchanged)
    if (base != c->exp) LOG("[sora] expcurve %d: EXP %u -> %u (enemy KH LV%d, Sora LV %d)\n", sl_expcurve, c->exp, base, c->kh_level, SL.lv);
    uint32_t e = ab_exp(AB.on, base, AB.hp, AB.hp_max);  // (s82 Experience Boost: x2 while HP <= 50%, GUESS)
    if (e != base) LOG("[sora] Experience Boost: EXP %u -> %u (HP %d/%d)\n", base, e, AB.hp, AB.hp_max);
    SL.exp = SL.exp + e > sl_exp_max() ? sl_exp_max() : SL.exp + e;
    SL.lv = sl_level_of(SL.exp);
    LOG("[sora] EXP +%u (enemy %llx %s life %d -> 0, max %d, KH LV%d, %s) total %u LV %d (next %u)\n", e,
        (unsigned long long)c->actor, c->name, c->life, c->max_life, c->kh_level, how, SL.exp, SL.lv, sl_next(SL.lv));
    for (int l = old + 1; l <= SL.lv; l++) sl_level_up_log(l);
    sl_persist_save();
}
static bool orb_kill(const SlCredit *c);  // (s131 orbs, sora_orbs.h: true = the kill drops orbs that carry its Drive points)
static void sl_kill(SlCredit *c, const char *how) {
    c->paid = true, SL.kills++;
    if (!orb_kill(c)) dg_kill(c->pct, c->flags);  // (s67 drive: KH2's Drive orbs)
    wl_kill(c->flags);  // (s75: Wisdom EXP +1 per kill in Wisdom)
    ml_kill(c->pct, c->flags);  // (s89: Master EXP +1 per Drive orb of the kill, in Master)
    fl_kill(c->flags);  // (s89 b4-final: Final EXP +1 per kill in Final)
    if (c->exp && anti_no_exp()) {  // (s89: Anti Form equips KH2's No Experience, plrp 0x86 item 404; live antiexp 1)
        LOG("[anti] KILL %llx %s: no EXP in Anti Form (%u lost)\n", (unsigned long long)c->actor, c->name, c->exp);
        sl_persist_save();
        return;
    }
    if (c->exp) { sl_gain(c, how); return; }
    LOG("[sora] KILL %llx %s life %d -> 0 (%s): no EXP (%s)\n", (unsigned long long)c->actor, c->name, c->life, how,
        c->flags & 1 ? "boss: a Get Bonus in slice 5" : c->flags & 2 ? "an animal that does not fight" : "0 in the table");
    sl_persist_save();
}

// ------------------------------------------------------------------ credit list + damage
static int32_t sl_life_now(uint64_t actor) {
    uint64_t a[64];
    float p[64][3];
    int32_t l[64];
    int n = linkpos_enemies(a, p, l, 64);
    for (int i = 0; i < n; i++) if (a[i] == actor) return l[i];
    return -1;
}
static SlCredit *sl_credit(uint64_t actor, int32_t life) {
    SlCredit *c = NULL, *old = NULL;
    for (int i = 0; i < SL_NC && !c; i++) if (SL.c[i].used && SL.c[i].actor == actor) c = &SL.c[i];
    if (c && !(c->paid && life > 0)) return c;  // (paid but alive again: a new actor at that address -> fresh entry)
    for (int i = 0; i < SL_NC && !c; i++) if (!SL.c[i].used) c = &SL.c[i];
    for (int i = 0; i < SL_NC && !c; i++) if (!old || SL.c[i].hit_at < old->hit_at) old = &SL.c[i];
    if (!c) c = old;
    memset(c, 0, sizeof *c);
    c->used = true, c->actor = actor, c->life = c->max_life = life, c->hit_at = SL.clock;
    char nb[48];
    const char *nm = SLX.name && SLX.name(actor, nb, (int)sizeof nb) > 0 ? nb : SL_ACTOR_NAME(actor);  // (s55: exe copy)
    int k = 0;
    for (; nm && k < (int)sizeof c->name - 1 && nm[k] > ' ' && nm[k] < 127; k++) c->name[k] = nm[k];
    c->name[k] = 0;
    const SlEnemy *t = k ? sl_enemy_find(c->name) : NULL;
    if (t) {
        c->named = true, c->kh_level = t->kh_level, c->pct = t->pct, c->exp = t->exp, c->flags = t->flags;
        if (t->life > c->max_life) c->max_life = t->life;
    } else {  // (unknown name / none: the life rule, percent 100; an unknown Animal_* gives nothing - user: only fighters)
        bool animal = !strncmp(c->name, "Animal_", 7);
        c->kh_level = (uint8_t)sl_life_level(life), c->pct = 100, c->flags = animal ? 2 : 0;
        c->exp = animal ? 0 : (uint16_t)(SL_LVPM[c->kh_level - 1].exp > 0 ? SL_LVPM[c->kh_level - 1].exp : 1);
        if (!k) snprintf(c->name, sizeof c->name, "(no name)");
        LOG("[sora] levels: %llx %s not in the table: KH LV%d by its life %d, EXP %u (GUESS)\n", (unsigned long long)actor,
            c->name, c->kh_level, life, c->exp);
    }
    // (s109) KH2 HP percent for the damage rule: a boss-class analogue's enmp row, else the table pct (0 = boss: 200)
    const SlBossHp *b = sl_boss_hp_on && k ? sl_boss_hp(c->name) : NULL;
    c->hp_pct = b ? b->hp : c->pct ? c->pct : 200;
    c->boss = b != NULL;
    // normal enemies: KH2's enmp rows tie Cap / Min to the HP percent (VERIFIED pattern: 50 -> 10000/1000, 70 ->
    // 10000/715, 100 -> 10000/500, 160 -> 5000/313, 250 -> 2500/200: Min = 50000 / hp%, Cap by band). (s109d: was
    // left 0 = every normal-enemy hit clamped to V 1)
    c->cap = b ? b->cap : c->hp_pct <= 100 ? 10000 : c->hp_pct <= 160 ? 5000 : 2500;
    c->mind = b ? b->mind : (uint16_t)((50000 + c->hp_pct / 2) / c->hp_pct);
    if (b) LOG("[sora] bosshp: %s = KH2 %s: HP %u%% of lvpm LV%d (KH HP %u), cap %u, min %u\n", c->name, b->kh2, b->hp,
               c->kh_level, (unsigned)((b->hp * SL_LVPM[c->kh_level - 1].hp_mul + 99) / 100), b->cap, b->mind);
    return c;
}

// (s109) KH2 FM's real damage function (KH2 exe FUN_1403c12f0, recon/S109_HELPER_KH2DMG.md, VERIFIED decompile):
//   V = S - Def (S = attacker Str, or Magic for spells; Def = enemy lvpm[level].Defense)
//   V = min(V, Cap), V = max(V, max(1, Min))   (Cap / Min = enmp value x enemy max HP / 10000, rounded up; Min wins)
//   dmg = ceil(weak% x Power x V / 100)        (weakness 100 here; no random, no crits)
//   x difficulty "dealt" (player team -> enemy): 0 1.5, 1 1.0, 2 1.0, 3 1.25 (ids VERIFIED, names GUESS: Beginner /
//   Standard / Proud / Critical), each step ceil(m x dmg / 100). Enemy HP = ceil(enmp hp% x lvpm hp_mul / 100).
// BotW damage = round(dmg / KH HP x the enemy's BotW max life), min 1. Same function for Sora, Donald and Goofy (KH2 has
// no party multiplier). Live: khformula 0 = the s54 ratio rule, khdiff 0-3, khdmg <x100> scales the KH dmg (test knob).
static int sl_khformula = 1, sl_khdiff = 1;
static const int SL_DIFF_DEALT[4] = {150, 100, 100, 125}, SL_DIFF_TAKEN[4] = {50, 100, 200, 200};
// (s110, user s109d: "red Bokoblin ~10 hits, want 3-5 -> KH2's world-level rule") KH2's enemies have enmp level 0 = the
// WORLD battle level (KH2 exe FUN_1403ee1d0: the btlv rows whose story flag is set, summed per world; recon/S109_HELPER_KH2DMG.md),
// not a level of their own. KH2's numbers (00battle btlv, VERIFIED dump of data/levels/kh2_sora_levels.json): Roxas' prologue
// Twilight Town = 1; Sora's first visits HB 7, LoD 9, BC 12, OC 15, DC 17, TR 18, PR 19, AG 21, HT 23, PL 25, SP 27; second
// visits 33..40; TWTNW 49. BotW has no worlds, so its story progress picks the level (GUESS, the BotW -> KH2 mapping):
//   no paraglider (IsGet_PlayerStole2 0, Roxas' Great Plateau) = 1 (the KH2 prologue)
//   paraglider = 7 (Sora's first world) + 7 per Divine Beast freed (Clear_Remains*) + 1 per 10 shrines cleared
//   (Clear_DungeonNNN), at most 49 (TWTNW).
// worldlv 1 (default): normal enemies' damage / HP / Defense / their hits on Sora use the world level; bosses (SL_BOSS_HP rows,
// user-verified s109c) keep their own. 2 = bosses too. 0 = the old per-enemy level (BotW rank). EXP stays on the enemy's
// table level (unchanged pacing). Live "set worldlvfix N" forces world level N (test knob, 0 = off).
static int sl_worldlv = 1, sl_worldlv_fix = 0;
static struct { int lv, glider, beasts, shrines, sweep_i, sweep_n; float next; bool have; } SW = {.lv = 1};
static int sl_world_lv(void) {
    if (sl_worldlv_fix > 0) return SW.lv = sl_worldlv_fix > 99 ? 99 : sl_worldlv_fix;
    if (!SLX.gdt_bool) return SW.lv;
    if (SW.sweep_i == 0 && SW.have && SL.clock < SW.next) return SW.lv;
    char nm[32];
    int v = 0;
    for (int k = 0; k < 24 && SW.sweep_i < 137; k++, SW.sweep_i++) {  // (24 flags a call: one sweep ~ 6 frames, as chestabil's)
        snprintf(nm, sizeof nm, "Clear_Dungeon%03d", SW.sweep_i);
        v = 0;
        if (SLX.gdt_bool(nm, &v) && v) SW.sweep_n++;
    }
    if (SW.sweep_i < 137) return SW.lv;
    int glider = 0, beasts = 0;
    if (!SLX.gdt_bool("IsGet_PlayerStole2", &glider)) { SW.sweep_i = SW.sweep_n = 0; return SW.lv; }  // (title / loading: keep)
    static const char *const BEAST[4] = {"Clear_RemainsWind", "Clear_RemainsWater", "Clear_RemainsFire", "Clear_RemainsElectric"};
    for (int i = 0; i < 4; i++) { v = 0; if (SLX.gdt_bool(BEAST[i], &v) && v) beasts++; }
    int lv = !glider ? 1 : 7 + 7 * beasts + SW.sweep_n / 10;
    if (lv > 49) lv = 49;
    if (!SW.have || lv != SW.lv || glider != SW.glider || beasts != SW.beasts || SW.sweep_n != SW.shrines)
        LOG("[sora] worldlv: world level %d (paraglider %d, Divine Beasts %d, shrines %d)%s\n", lv, glider, beasts, SW.sweep_n,
            sl_worldlv ? "" : " [worldlv 0: per-enemy levels used]");
    if (g_ab_glider != (glider != 0)) g_ab_glider = glider != 0, AB.dirty = true;  // (s110 chestfirst: Dodge Roll comes with the paraglider)
    SW.lv = lv, SW.glider = glider, SW.beasts = beasts, SW.shrines = SW.sweep_n, SW.have = true;
    SW.sweep_i = SW.sweep_n = 0, SW.next = SL.clock + 5.0f;
    return SW.lv;
}
// the level c's damage / HP / Defense use (EXP keeps c->kh_level)
static int sl_dmg_lv(const SlCredit *c) {
    int lv = sl_worldlv && (sl_worldlv == 2 || !c->boss) ? SW.lv : c->kh_level;
    return lv < 1 ? 1 : lv > 99 ? 99 : lv;
}
static int sl_kh_hp_of(const SlCredit *c) {
    int hp = (c->hp_pct * SL_LVPM[sl_dmg_lv(c) - 1].hp_mul + 99) / 100;
    return hp < 1 ? 1 : hp;
}
// c's KH dmg for stat S and Power; out: BotW damage. why = log text.
static int sl_kh_apply(const SlCredit *c, int S, int power, char *why, int n) {
    int hp = sl_kh_hp_of(c), def = SL_LVPM2[sl_dmg_lv(c) - 1].def;
    int cap = (int)(((int64_t)c->cap * hp + 9999) / 10000), mn = (int)(((int64_t)c->mind * hp + 9999) / 10000);
    int v = S - def, vraw = v;
    if (v > cap) v = cap;
    if (v < (mn > 1 ? mn : 1)) v = mn > 1 ? mn : 1;
    int dmg = power > 0 ? (power * v + 99) / 100 : 0;
    if (power > 0 && dmg < 1) dmg = 1;
    int dif = sl_khdiff < 0 ? 0 : sl_khdiff > 3 ? 3 : sl_khdiff;
    if (dmg > 0) dmg = (SL_DIFF_DEALT[dif] * dmg + 99) / 100;
    if (SL.khdmg != 100) dmg = (dmg * SL.khdmg + 50) / 100;
    float raw = (float)dmg / (float)hp * (float)c->max_life;
    int d = raw > 1e6f ? 1000000 : (int)lroundf(raw);
    if (d < 1) d = 1;
    snprintf(why, (size_t)n, "S %d - Def %d = %d -> clamp [%d, %d] = %d x power %d = KH %d%s of KH HP %d (LV%d %u%%)", S, def,
             vraw, mn > 1 ? mn : 1, cap, v, power, dmg, dif != 1 ? " (diff)" : "", hp, sl_dmg_lv(c), c->hp_pct);
    return d;
}

// The damage of one of Sora's hits (Power = the atkp / spell power, 100 = A300), credited for the kill rule.
// khformula 1 (s109): sl_kh_apply above. khformula 0 = the s54 rule: max(1, round(kh_dmg / kh_hp x enemy max life)),
// kh_dmg = (Str + key) x Power / 100 (spells Mag + key) x khdmg %, kh_hp = lvpm hp_mul x hp% / 100.
// lvdmg 0 = the old flat rule max(1, round(base x Power / 100)). life < 0: looked up in linkpos's list.
static int sl_dmg_force;  // (s128 wpn) > 0 = this hit's damage is given (Link's BotW weapon), credit / EXP as usual
static int sl_str_bonus;  // (s106 Berserk Charge: +2 Str during MP Charge, set by sora.c's blade_hits; Str only, not Mag)
static int sl_hit(uint64_t actor, int32_t life, int power, float base, int magic) {
    sl_init();
    int flat = (int)lroundf(base * (float)power / 100.0f), d;
    if (flat < 1) flat = 1;
    d = flat;
    if (life < 0) life = sl_life_now(actor);
    SlCredit *c = life > 0 ? sl_credit(actor, life) : NULL;
    if (sl_dmg_force > 0) flat = d = sl_dmg_force;
    if (!c) return d;
    if (life > c->max_life) c->max_life = life;
    const SlLevel *s = &SL_LV[sl_lv() - 1];
    int st = magic ? sl_mag_of(s) + SL_KEY_MAG : sl_str_of(s) + SL_KEY_STR + sl_str_bonus;
    char why[200];
    if (sl_khformula) {
        int k = sl_kh_apply(c, st, power, why, (int)sizeof why);
        if (SL.lvdmg) d = k;
        if (SL.lvlog)
            LOG("[sora] lvdmg %llx %s: LV %d %s: %s x max life %d -> %d%s\n", (unsigned long long)actor, c->name, SL.lv,
                magic ? "Mag" : "Str", why, c->max_life, d, SL.lvdmg ? "" : " [lvdmg 0: flat used]");
    } else {
        float kh = (float)st * (float)power / 100.0f * (float)SL.khdmg / 100.0f;
        float kh_hp = (float)SL_LVPM[c->kh_level - 1].hp_mul * (float)c->hp_pct / 100.0f;
        float raw = kh / kh_hp * (float)c->max_life;
        if (SL.lvdmg) d = raw > 1e6f ? 1000000 : (int)lroundf(raw), d = d < 1 ? 1 : d;
        if (SL.lvlog)
            LOG("[sora] lvdmg %llx %s: LV %d %s %d x power %d = %.2f vs KH HP %.1f (KH LV%d, %d%%) x max life %d -> %d%s "
                "(flat %d) [khformula 0]\n", (unsigned long long)actor, c->name, SL.lv, magic ? "Mag+key" : "Str+key", st,
                power, kh, kh_hp, c->kh_level, c->hp_pct, c->max_life, d, SL.lvdmg ? "" : " [lvdmg 0: flat used]", flat);
    }
    if (sl_dmg_force > 0) d = sl_dmg_force;
    c->hit_at = SL.clock, c->queued += d;
    return d;
}
// (for the L5 enemy gauge, not hooked in slice 1) the max life the credit list knows, 0 = unknown
static int32_t sl_enemy_max_life(uint64_t actor) {
    for (int i = 0; i < SL_NC; i++) if (SL.c[i].used && SL.c[i].actor == actor) return SL.c[i].max_life;
    return 0;
}

// TODO(s54 levels, slice 5): Glide LV from the Divine Beasts (Clear_RemainsWater / Wind / Fire / Electric: doc sec 6
// + 8), NOT from Sora's level (user decision). 0 = no change (sora.c keeps g_glide_lv); slice 5 returns 1..5 and
// sora.c's tick copies it into g_glide_lv.
static int sl_glide_lv(void) { return 0; }

// ------------------------------------------------------------------ the exe's died queue (s55 slice 2)
// linkpos's actor slot 54 hook (an enemy's die call, with its life and name): the exact kill, off-screen ones too. An
// enemy Sora hit within SL.win + SL_DIE_GRACE s pays (unless the life-0 rule paid already). Base slot 54 calls with
// life > 0 are not deaths (other delete paths).
static void sl_died_one(const SlDied *d) {
    SlCredit *c = NULL;
    for (int i = 0; i < SL_NC && !c; i++) if (SL.c[i].used && SL.c[i].actor == d->actor) c = &SL.c[i];
    if (!c || c->paid) return;  // (not Sora's, or the life-0 rule paid it)
    if (d->kind == 2 && d->life > 0) {
        LOG("[sora] died %llx %s: base slot 54 at life %d = not a death, no EXP\n", (unsigned long long)d->actor, d->name, d->life);
        return;
    }
    float ago = SL.clock - c->hit_at;
    if (ago > (float)SL.win + SL_DIE_GRACE) {
        LOG("[sora] died %llx %s: Sora's last hit %.1f s ago (window %d + %.0f s): no EXP\n", (unsigned long long)d->actor,
            d->name, ago, SL.win, SL_DIE_GRACE);
        c->used = false;
        return;
    }
    sl_kill(c, d->kind == 1 ? "died: enemy die hook (exe)" : "died: actor die hook (exe)");
}
static void sl_died_tick(void) {
    if (!SLX.died) return;
    SlDied d[16];
    int n = 0;
    for (int guard = 0; guard < 8 && (n = SLX.died(d, 16, (int)sizeof *d)) > 0; guard++)
        for (int i = 0; i < n; i++) sl_died_one(&d[i]);
    if (n < 0) {
        LOG("[sora] levels: linkpos_enemy_died refused the struct size %d (exe / DLL mismatch): died queue off\n", (int)sizeof *d);
        SLX.died = NULL;
    }
}

// ------------------------------------------------------------------ per frame
// (s109) KH2 Defense on incoming hits (same KH2 function, enemy as attacker, recon/S109_HELPER_KH2DMG.md sec 4):
// V = clamp(enemy lvpm Str - Sora Def, [max(1, MinStr), MaxStr]); KH2 dmg = ceil(Power x V / 100) x difficulty taken
// (0 x0.5, 1 x1, 2 x2, 3 x2). BotW's own hit has no atkp Power, so its damage is scaled by V(Sora Def) / V(Def 0) x
// taken%: Def only matters once Str - Def < MaxStr (KH2's design: a strong enemy's hits stay capped either way). The
// attacker = the nearest enemy within 20 m (GUESS: BotW's hit carries no attacker for us). Exe export
// linkpos_hurt_scale_set(pct) applies it in the player addDamage hook (old exe: no effect). Live khdef 0 = off.
static int sl_khdef = 1;
static void sl_def_tick(void) {
    static void (*set)(int);
    static bool bound;
    static int said = -1;
    if (!bound) {
        bound = true;
        set = (void (*)(int))(void *)GetProcAddress(GetModuleHandleA(NULL), "linkpos_hurt_scale_set");
        LOG("[sora] khdef: %s\n", set ? "exe hurt scale found" : "linkpos_hurt_scale_set not in this exe (Defense off)");
    }
    if (!set) return;
    int pct = 100, lv = 0, dif = sl_khdiff < 0 ? 0 : sl_khdiff > 3 ? 3 : sl_khdiff;
    float m[12];
    if (sl_khdef && linkpos_link_mtx(m)) {
        uint64_t a[64];
        float p[64][3];
        int32_t l[64];
        int n = linkpos_enemies(a, p, l, 64), best = -1;
        float bd = 20.0f * 20.0f;
        for (int i = 0; i < n; i++) {
            if (l[i] <= 0) continue;
            float dx = p[i][0] - m[3], dy = p[i][1] - m[7], dz = p[i][2] - m[11], d2 = dx * dx + dy * dy + dz * dz;
            if (d2 < bd) bd = d2, best = i;
        }
        if (best >= 0) {
            for (int i = 0; i < SL_NC && !lv; i++) if (SL.c[i].used && SL.c[i].actor == a[best]) lv = sl_dmg_lv(&SL.c[i]);  // (s110 worldlv)
            if (!lv) {
                char nb[48];
                bool named = SLX.name && SLX.name(a[best], nb, (int)sizeof nb) > 0;
                const SlEnemy *t = named ? sl_enemy_find(nb) : NULL;
                bool boss = named && sl_boss_hp_on && sl_boss_hp(nb);
                lv = sl_worldlv && (sl_worldlv == 2 || !boss) ? SW.lv : t ? t->kh_level : sl_life_level(l[best]);  // (s110: KH2 enemy Str at the world level)
            }
            if (lv < 1) lv = 1;
            if (lv > 99) lv = 99;
            const SlLvpm2 *e = &SL_LVPM2[lv - 1];
            int lo = e->min_str > 1 ? e->min_str : 1, hi = e->max_str > lo ? e->max_str : lo;
            int v = e->str - SL_LV[sl_lv() - 1].def, v0 = e->str;
            v = v < lo ? lo : v > hi ? hi : v, v0 = v0 < lo ? lo : v0 > hi ? hi : v0;
            pct = (100 * v + v0 - 1) / v0;
            pct = (pct * SL_DIFF_TAKEN[dif] + 99) / 100;
        }
    }
    if (!sl_khdef) pct = 100;
    set(pct);
    if (pct != said) {
        said = pct;
        LOG("[sora] khdef: incoming x%d%% (nearest enemy KH LV%d, Sora LV %d Def %d, diff %d)\n", pct, lv, sl_lv(),
            SL_LV[sl_lv() - 1].def, dif);
    }
}
static void sl_tick(void) {
    sl_init();
    sl_world_lv();  // (s110 worldlv: a few gdt flags a frame, one full read every 5 s)
    sl_def_tick();
    float r = framepace_game_ratio();
    if (!(r > 0.05f && r < 8.0f)) r = 1;
    SL.clock += r / 30.0f;
    sl_slot_tick();  // (s55 slice 3: BotW's save reads / writes -> sidecars)
    sl_died_tick();  // (s55 slice 2: the exe's died queue -> kill credit)
    ab_tick();       // (s82 slice 3: learned abilities -> auto-equip / AP fit)
    if (!SLS.gdt_said && SLX.gdt_s32 && SL.clock > 5.0f) {  // (one check of the slice-2 gdt getters; slice 5 uses them)
        int32_t hv = -1;
        int gl = -1, a = SLX.gdt_s32("MaxHartValue", &hv), b = SLX.gdt_bool ? SLX.gdt_bool("IsGet_PlayerStole2", &gl) : 0;
        SLS.gdt_said = true;
        LOG("[sora] levels: gdt MaxHartValue = %d (%s), IsGet_PlayerStole2 = %d (%s)\n", hv, a ? "found" : "not found", gl,
            b ? "found" : "not found");
    }
    if (dream_weapon != SL.dream_seen) {
        bool first = SL.dream_seen == -99;
        SL.dream_seen = dream_weapon;
        char ab[768];
        sl_abil_list(SL.lv, ab, sizeof ab);
        LOG("[sora] Dream Weapon %s%s (give up %s): abilities up to LV %d: %s\n", first ? "" : "-> ", SL_DREAM_NAME[sl_dream()],
            dream_giveup >= 0 && dream_giveup <= 2 ? SL_DREAM_NAME[dream_giveup] : "-", SL.lv, ab);
    }
    int used = 0;
    for (int i = 0; i < SL_NC; i++) used += SL.c[i].used;
    if (!used) return;
    uint64_t a[64];
    float p[64][3];
    int32_t l[64];
    int n = linkpos_enemies(a, p, l, 64);
    for (int i = 0; i < SL_NC; i++) {
        SlCredit *c = &SL.c[i];
        if (!c->used) continue;
        int k = 0;
        while (k < n && a[k] != c->actor) k++;
        if (k < n) {
            if (l[k] > c->max_life) c->max_life = l[k];
            if (!c->paid && c->life > 0 && l[k] <= 0 && SL.clock - c->hit_at <= (float)SL.win) sl_kill(c, "seen at life 0");
            if (l[k] != c->life) c->queued = 0;
            if (l[k] > 0) c->life = l[k];
            else if (!c->paid) c->life = 0;  // (died outside the window: no EXP, never again; paid keeps its last life)
            if (SL.clock - c->hit_at > 60.0f) c->used = false;  // (long ago: free the slot)
        } else if (SLX.died) {
            // (s55) dropped out of linkpos's list: the exe's die hook reports a death there (sl_died_tick), so no
            // vanish guess; the entry waits for a late died event until the window + grace is over
            if (c->paid || SL.clock - c->hit_at > (float)SL.win + SL_DIE_GRACE) c->used = false;
        } else {
            // dropped out of linkpos's list (60 unseen frames, or deleted). [GUESS] a death we never saw at life 0 (an
            // actor removed the frame it died): the queued damage reached its life and the hit is fresh.
            if (!c->paid && c->life > 0 && c->queued >= c->life && SL.clock - c->hit_at <= (float)SL.win + 2.5f)
                sl_kill(c, "gone after a lethal hit, GUESS");
            c->used = false;
        }
    }
}

// ------------------------------------------------------------------ live "set" keys (sora.c's live_poll)
// (s55 slice 3) slot -1 = log the slot state | slot N = load slot N's sidecar now (as BotW's read would) | slotsave N =
// write slot N's sidecar now | lvfloor 0/1 = a save without a sidecar starts at LV 1 / the story floor
static bool sl_slot_live(const char *key, int v) {
    if (!strcmp(key, "slot")) {
        if (v >= 0 && v < SL_SLOTS) sl_slot_load(v, "live set slot");
        else
            LOG("[sora] levels: slot %d, story progress %d, root %s, exe events %s (seq %llu), floor %d, LV %d EXP %u\n",
                SLS.slot, SLS.base, sl_root(), SLX.save_ev ? "yes" : "no", (unsigned long long)SLS.seq, SLS.floor, SL.lv, SL.exp);
    } else if (!strcmp(key, "slotsave") && v >= 0 && v < SL_SLOTS) sl_slot_saved(v, SL.exp, SL.kills, "live set slotsave");
    else if (!strcmp(key, "lvfloor")) SLS.floor = v != 0;
    else return false;
    return true;
}
static bool sl_live_set(const char *key, int v) {
    sl_init();
    if (sl_slot_live(key, v)) { sl_persist_save(); return true; }  // (s55 slice 3: slot / slotsave / lvfloor)
    if (!strcmp(key, "lv")) sl_set_exp(SL_LV[(v < 1 ? 1 : v > 99 ? 99 : v) - 1].exp, "set lv");
    else if (!strcmp(key, "exp")) sl_set_exp(v < 0 ? 0 : (uint32_t)v, "set exp");
    else if (!strcmp(key, "lvdmg")) SL.lvdmg = v != 0;
    else if (!strcmp(key, "worldlv")) sl_worldlv = v < 0 ? 0 : v > 2 ? 2 : v, SW.have = false, LOG("[sora] live worldlv %d (0 per-enemy, 1 normal enemies at the world level, 2 bosses too)\n", sl_worldlv);
    else if (!strcmp(key, "worldlvfix")) sl_worldlv_fix = v < 0 ? 0 : v, SW.have = false, LOG("[sora] live worldlvfix %d (0 = story progress)\n", sl_worldlv_fix);
    else if (!strcmp(key, "aplv")) sl_aplv = v < 0 ? 0 : v > 5 ? 5 : v, AB.dirty = true, LOG("[sora] live aplv %d (extra AP per level; AP max now %d)\n", sl_aplv, ab_ap_max());
    else if (!strcmp(key, "bosshp")) sl_boss_hp_on = v != 0;  // (s109; new credits only = enemies hit after the change)
    else if (!strcmp(key, "khformula")) sl_khformula = v != 0;  // (s109)
    else if (!strcmp(key, "khdef")) sl_khdef = v != 0;  // (s109 Defense on incoming hits, needs the s109 exe)
    else if (!strcmp(key, "khdiff")) sl_khdiff = v < 0 ? 0 : v > 3 ? 3 : v;  // (s109: 0 Beginner 1 Standard 2 Proud 3 Critical, names GUESS)
    else if (!strcmp(key, "ngforms")) g_sl_forms_reset = v != 0;  // (s89 trial)
    else if (!strcmp(key, "lvstr")) sl_lvstr = v != 0;
    else if (!strcmp(key, "lvmag")) sl_lvmag = v != 0;
    else if (!strcmp(key, "lvlog")) SL.lvlog = v != 0;
    else if (!strcmp(key, "khdmg")) SL.khdmg = v < 1 ? 1 : v;
    else if (!strcmp(key, "lvwin")) SL.win = v < 1 ? 1 : v;
    else if (!strcmp(key, "expcurve")) {  // (s88) 0 = old table, 1/2/3 (3 = default, user pick) = proposals (recon/S88_HELPER_EXP.md)
        sl_expcurve = v >= 0 && v <= 3 ? v : 0;
        LOG("[sora] expcurve %d (%s)\n", sl_expcurve,
            sl_expcurve == 1 ? "flat x2" : sl_expcurve == 2 ? "level-gap" : sl_expcurve == 3 ? "LV bands" : "off: S54 table");
    }
    else if (!strcmp(key, "lvup")) { if (v) sl_lvup_queue++, sl_lvup_hud(SL.lv > 1 ? SL.lv : 2); return true; }
    else if (!strcmp(key, "lvpanel")) { sl_lvup_panel = v != 0; return true; }
    else if (!strcmp(key, "lvse")) { sl_lvup_se = v != 0; return true; }
    else if (!strcmp(key, "lvfx")) { sl_lvup_fx = v != 0; return true; }
    // (s82) set abon / aboff <id>, ablist, abreset (forget on / seen: auto-equip again). Live lines all run in one
    // frame, so each syncs first (a "set lv" just before it).
    else if (!strcmp(key, "abon")) { ab_tick(); ab_set(v, 1); return true; }
    else if (!strcmp(key, "aboff")) { ab_tick(); ab_set(v, 0); return true; }
    else if (!strcmp(key, "ablist")) { ab_tick(); ab_log("live"); return true; }
    else if (!strcmp(key, "abilfix")) { g_abilfix = v != 0; AB.dirty = true; LOG("[sora] abilfix %d (%s)\n", v, v ? "KH2 categories + Action abilities by story LV" : "OLD list: 25 rows"); return true; }
    else if (!strcmp(key, "abreset")) { AB.on = AB.seen = 0, AB.dirty = true; ab_tick(); ab_log("reset"); }
    else if (!strcmp(key, "abforce")) {  // (s82 dev) effect test without the level: bit on, no checks; a sync drops it
        AB.on |= ab_bit(v);
        LOG("[sora] ability %d %s FORCED on (dev; dropped at the next sync if not learned)\n", v, ab_name(v));
        return true;
    }
    else if (!strcmp(key, "devlv")) {  // (s67) dev override: 0 off, 1..99
        if (AB.all) { dbm_set(0, v, "live set devlv"); return true; }  // (s141 review) All Abilities ON: the Debug Menu path keeps the layers apart
        int was = SL.devlv;
        SL.devlv = v <= 0 ? 0 : v > 99 ? 99 : v;
        // (s83) abilities follow devlv; the real on / seen masks are stashed and come back at devlv 0
        if (!was && SL.devlv) AB.dev_on = AB.on, AB.dev_seen = AB.seen;
        if (was && !SL.devlv) AB.on = AB.dev_on, AB.seen = AB.dev_seen;
        AB.dirty = true;
        LOG("[sora] devlv %d: rules use LV %d (saved LV %d EXP %u untouched)\n", SL.devlv, sl_lv(), SL.lv, SL.exp);
    }
    else return false;
    sl_persist_save();
    return true;
}
