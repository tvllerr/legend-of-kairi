// Sora -> Link ragdoll / puppet joint map (see below). No KH2 model data in this file.
// Joint ids = bone indices of the KH2 model in sora_model.bin / sora_pose.bin order (a joint = the first bone at it; helper bones
// coincident with it sit below it in the hierarchy). Link bone ids = Link.sbfres skeleton order. Units: KH2 cm, model space +x LEFT,
// +y up, +z forward. The joint ids and the Sora -> Link map below are OUR mapping choices; the KH2 skeleton NUMBERS (bone parents, bind
// positions, segment bind directions / lengths) are not in the source: they are read at startup from data/ragmap/<char>.bin, which
// the installer builds from the player's own KH2 files (tools/sorabuild/ragmap, `sorabake ragmap`). Retarget design: SAVAGE_WIND_RECON.md session 22.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SORA_RAG_BONES 240      // Sora 228 + Keyblade 12
#define SORA_RAG_SORA_BONES 228
#define SORA_RAG_KB_PARENT 178   // Keyblade bone 228's parent (right palm)

// Sora joints (the first bone at each joint; helper bones coincident with it sit below it in the hierarchy)
#define SORA_J_ROOT            0   // model root at hip height; everything hangs off it = the pelvis to place
#define SORA_J_HIP_HUB         3
#define SORA_J_SPINE_1        65   // first spine bone (coincident with the root), waist band mesh
#define SORA_J_SPINE_2        66
#define SORA_J_SPINE_3        67
#define SORA_J_CHEST_HUB      68
#define SORA_J_NECK          136
#define SORA_J_HEAD          138
#define SORA_J_HEAD_TOP      141
#define SORA_J_CLAV_L         74
#define SORA_J_SHOULDER_L     76
#define SORA_J_ELBOW_L        79
#define SORA_J_WRIST_L        80
#define SORA_J_PALM_L         86
#define SORA_J_FINGER_MID_L   97
#define SORA_J_CLAV_R        166
#define SORA_J_SHOULDER_R    168
#define SORA_J_ELBOW_R       171
#define SORA_J_WRIST_R       172
#define SORA_J_PALM_R        178
#define SORA_J_FINGER_MID_R  189
#define SORA_J_HIP_L          10
#define SORA_J_KNEE_L         12
#define SORA_J_ANKLE_L        13
#define SORA_J_HEEL_L         16
#define SORA_J_TOE_L          17
#define SORA_J_HIP_R          39
#define SORA_J_KNEE_R         41
#define SORA_J_ANKLE_R        42
#define SORA_J_HEEL_R         45
#define SORA_J_TOE_R          46

// Link joints (Link.sbfres)
#define LINK_J_ROOT            0
#define LINK_J_SKL_ROOT        1
#define LINK_J_SPINE_1         2
#define LINK_J_SPINE_2         3
#define LINK_J_CLAVICLE_L      4
#define LINK_J_ARM_1_L         5
#define LINK_J_ARM_2_L         7
#define LINK_J_WRIST_L        10
#define LINK_J_FINGER_C_1_L   17
#define LINK_J_CLAVICLE_R     28
#define LINK_J_ARM_1_R        29
#define LINK_J_ARM_2_R        31
#define LINK_J_WRIST_R        34
#define LINK_J_FINGER_C_1_R   41
#define LINK_J_NECK           52
#define LINK_J_HEAD           53
#define LINK_J_HAIR_ROOT      90
#define LINK_J_WAIST          93
#define LINK_J_LEG_1_L       106
#define LINK_J_LEG_2_L       108
#define LINK_J_ANKLE_L       110
#define LINK_J_TOE_L         111
#define LINK_J_LEG_1_R       112
#define LINK_J_LEG_2_R       114
#define LINK_J_ANKLE_R       116
#define LINK_J_TOE_R         117

// parent of every bone (-1 = root) and the bind (rest) pose world position of every bone, cm: KH2 data, NOT in the source.
// Filled at startup from data/ragmap/sora.bin (built at install from the player's own KH2 files, tools/sorabuild/ragmap).
static int16_t sora_rag_parent[SORA_RAG_BONES];

static float sora_rag_bind_pos[SORA_RAG_BONES][3];

enum { SORA_RAG_PELVIS = 0, SORA_RAG_CHEST = 1, SORA_RAG_LIMB = 2, SORA_RAG_HEAD = 3 };
typedef struct {
    uint8_t role;                 // SORA_RAG_*: PELVIS / CHEST = full frame, LIMB = swing-only, HEAD = orientation delta
    int16_t sora, link;           // the Sora bone to rotate (and place, for the pelvis) and Link's joint at the same spot
    int16_t sora_to, link_to;     // segment end: direction sora->sora_to is aimed along link->link_to (frames: the 'up' axis)
    int16_t sora_l, sora_r;       // frames only: 'left' axis = pos(sora_l) - pos(sora_r) vs pos(link_l) - pos(link_r); -1 for limbs
    int16_t link_l, link_r;
    float bind_dir[3], bind_len;  // sora->sora_to in the bind pose: unit direction (KH axes) and length, cm (0 until sora_ragmap_load fills them from the data file)
    const char *name;
} SoraRagBone;

// Order = top-down (parents before children), so a single pass can apply frames then limbs.
static SoraRagBone sora_ragmap[] = {
    {SORA_RAG_PELVIS, SORA_J_ROOT, LINK_J_SKL_ROOT, SORA_J_SPINE_3, LINK_J_SPINE_1, SORA_J_HIP_L, SORA_J_HIP_R, LINK_J_LEG_1_L, LINK_J_LEG_1_R, {0, 0, 0}, 0, "pelvis"},
    {SORA_RAG_CHEST, SORA_J_SPINE_3, LINK_J_SPINE_1, SORA_J_NECK, LINK_J_NECK, SORA_J_SHOULDER_L, SORA_J_SHOULDER_R, LINK_J_ARM_1_L, LINK_J_ARM_1_R, {0, 0, 0}, 0, "chest (jacket bone; arms + head hang off it)"},
    {SORA_RAG_LIMB, SORA_J_NECK, LINK_J_NECK, SORA_J_HEAD, LINK_J_HEAD, -1, -1, -1, -1, {0, 0, 0}, 0, "neck"},
    {SORA_RAG_HEAD, SORA_J_HEAD, LINK_J_HEAD, SORA_J_HEAD_TOP, LINK_J_HAIR_ROOT, -1, -1, -1, -1, {0, 0, 0}, 0, "head (Link Head orientation delta; the position pair is a weak fallback)"},
    {SORA_RAG_LIMB, SORA_J_CLAV_L, LINK_J_CLAVICLE_L, SORA_J_SHOULDER_L, LINK_J_ARM_1_L, -1, -1, -1, -1, {0, 0, 0}, 0, "clavicle L"},
    {SORA_RAG_LIMB, SORA_J_SHOULDER_L, LINK_J_ARM_1_L, SORA_J_ELBOW_L, LINK_J_ARM_2_L, -1, -1, -1, -1, {0, 0, 0}, 0, "upper arm L"},
    {SORA_RAG_LIMB, SORA_J_ELBOW_L, LINK_J_ARM_2_L, SORA_J_WRIST_L, LINK_J_WRIST_L, -1, -1, -1, -1, {0, 0, 0}, 0, "forearm L"},
    {SORA_RAG_LIMB, SORA_J_WRIST_L, LINK_J_WRIST_L, SORA_J_FINGER_MID_L, LINK_J_FINGER_C_1_L, -1, -1, -1, -1, {0, 0, 0}, 0, "hand L (optional: Finger_C = middle finger assumed)"},
    {SORA_RAG_LIMB, SORA_J_CLAV_R, LINK_J_CLAVICLE_R, SORA_J_SHOULDER_R, LINK_J_ARM_1_R, -1, -1, -1, -1, {0, 0, 0}, 0, "clavicle R"},
    {SORA_RAG_LIMB, SORA_J_SHOULDER_R, LINK_J_ARM_1_R, SORA_J_ELBOW_R, LINK_J_ARM_2_R, -1, -1, -1, -1, {0, 0, 0}, 0, "upper arm R"},
    {SORA_RAG_LIMB, SORA_J_ELBOW_R, LINK_J_ARM_2_R, SORA_J_WRIST_R, LINK_J_WRIST_R, -1, -1, -1, -1, {0, 0, 0}, 0, "forearm R"},
    {SORA_RAG_LIMB, SORA_J_WRIST_R, LINK_J_WRIST_R, SORA_J_FINGER_MID_R, LINK_J_FINGER_C_1_R, -1, -1, -1, -1, {0, 0, 0}, 0, "hand R (optional; the Keyblade rides on 178 under it)"},
    {SORA_RAG_LIMB, SORA_J_HIP_L, LINK_J_LEG_1_L, SORA_J_KNEE_L, LINK_J_LEG_2_L, -1, -1, -1, -1, {0, 0, 0}, 0, "thigh L"},
    {SORA_RAG_LIMB, SORA_J_KNEE_L, LINK_J_LEG_2_L, SORA_J_ANKLE_L, LINK_J_ANKLE_L, -1, -1, -1, -1, {0, 0, 0}, 0, "shin L"},
    {SORA_RAG_LIMB, SORA_J_ANKLE_L, LINK_J_ANKLE_L, SORA_J_TOE_L, LINK_J_TOE_L, -1, -1, -1, -1, {0, 0, 0}, 0, "foot L (ankle -> shoe tip)"},
    {SORA_RAG_LIMB, SORA_J_HIP_R, LINK_J_LEG_1_R, SORA_J_KNEE_R, LINK_J_LEG_2_R, -1, -1, -1, -1, {0, 0, 0}, 0, "thigh R"},
    {SORA_RAG_LIMB, SORA_J_KNEE_R, LINK_J_LEG_2_R, SORA_J_ANKLE_R, LINK_J_ANKLE_R, -1, -1, -1, -1, {0, 0, 0}, 0, "shin R"},
    {SORA_RAG_LIMB, SORA_J_ANKLE_R, LINK_J_ANKLE_R, SORA_J_TOE_R, LINK_J_TOE_R, -1, -1, -1, -1, {0, 0, 0}, 0, "foot R"},
};
// data/ragmap/<char>.bin (tools/sorabuild/ragmap/ragbin.py): u32 'SRAG', u32 1, u32 nbones, u32 nmap, int16 parent[nbones] (padded to 4),
// float bind_pos[nbones][3], float map[nmap][4] (bind_dir xyz + bind_len). Returns 1 and fills the three tables, or 0 (tables untouched,
// *why = a short reason) when the file is missing / has another size: the caller then turns that character's ragdoll / puppet off.
// every parent in [-1, i) (top-down order: no cycle, no index past the table) and every bind position finite
static int sora_ragmap_sane(const unsigned char *par, const unsigned char *posb, int nbones) {
    for (int i = 0; i < nbones; i++) {
        int16_t p;
        memcpy(&p, par + 2 * i, 2);
        if (p < -1 || p >= i) return 0;
        float v[3];
        memcpy(v, posb + 12 * i, 12);
        for (int k = 0; k < 3; k++) if (!(v[k] == v[k]) || v[k] > 1e6f || v[k] < -1e6f) return 0;
    }
    return 1;
}
static int sora_ragmap_load(const char *path, int nbones, int16_t *parent, float (*pos)[3], SoraRagBone *map, int nmap, const char **why) {
    *why = "cannot open";
    FILE *f = path ? fopen(path, "rb") : NULL;
    if (!f) return 0;
    uint32_t h[4];
    size_t pb = ((size_t)nbones * 2 + 3) & ~(size_t)3, want = 16 + pb + (size_t)nbones * 12 + (size_t)nmap * 16;
    unsigned char *buf = (unsigned char *)malloc(want + 1);
    size_t got = buf ? fread(buf, 1, want + 1, f) : 0;
    fclose(f);
    int ok = 0;
    if (!buf) *why = "out of memory";
    else if (got != want) *why = "wrong file size";
    else {
        memcpy(h, buf, 16);
        if (h[0] != 0x47415253u || h[1] != 1u) *why = "not a SRAG v1 file";
        else if (h[2] != (uint32_t)nbones || h[3] != (uint32_t)nmap) *why = "bone / map count differs from this build";
        else if (!sora_ragmap_sane(buf + 16, buf + 16 + pb, nbones)) *why = "bad parent / pos";  // (s182 review S1: a cycle or an
        // out-of-range parent would hang / overrun rmask_init + hp_desc, which walk parents unchecked)
        else {
            memcpy(parent, buf + 16, (size_t)nbones * 2);
            memcpy(pos, buf + 16 + pb, (size_t)nbones * 12);
            const unsigned char *m = buf + 16 + pb + (size_t)nbones * 12;
            for (int i = 0; i < nmap; i++) {
                float v[4];
                memcpy(v, m + 16 * i, 16);
                map[i].bind_dir[0] = v[0], map[i].bind_dir[1] = v[1], map[i].bind_dir[2] = v[2], map[i].bind_len = v[3];
            }
            ok = 1;
        }
    }
    free(buf);
    return ok;
}

#define SORA_RAGMAP_N 18
