// Roxas -> Link ragdoll / puppet joint map. No KH2 model data in this file.
// Include AFTER sora_ragmap.h (LINK_J_*, SORA_RAG_* roles, SoraRagBone and sora_ragmap_load come from there).
// Joint ids = bone indices of the KH2 model in sora_model.bin / sora_pose.bin order (a joint = the first bone at it; helper bones
// coincident with it sit below it in the hierarchy). Link bone ids = Link.sbfres skeleton order. Units: KH2 cm, model space +x LEFT,
// +y up, +z forward. The joint ids and the Sora -> Link map below are OUR mapping choices; the KH2 skeleton NUMBERS (bone parents, bind
// positions, segment bind directions / lengths) are not in the source: they are read at startup from data/ragmap/<char>.bin, which
// the installer builds from the player's own KH2 files (tools/sorabuild/ragmap, `sorabake ragmap`). Retarget design: SAVAGE_WIND_RECON.md session 22.
#pragma once
#include <stdint.h>

#define ROXAS_RAG_BONES 241      // body 229 + Keyblade 12
#define ROXAS_RAG_BODY_BONES 229
#define ROXAS_RAG_KB_PARENT 172   // Keyblade bone 229's parent (right palm)

// Sora joints (the first bone at each joint; helper bones coincident with it sit below it in the hierarchy)
#define ROXAS_J_ROOT            0   // model root at hip height; everything hangs off it = the pelvis to place
#define ROXAS_J_HIP_HUB         3
#define ROXAS_J_SPINE_1        41   // first spine bone (coincident with the root), waist band mesh
#define ROXAS_J_SPINE_2        42
#define ROXAS_J_SPINE_3        45
#define ROXAS_J_CHEST_HUB      46
#define ROXAS_J_NECK          109
#define ROXAS_J_HEAD          111
#define ROXAS_J_HEAD_TOP      114
#define ROXAS_J_CLAV_L         51
#define ROXAS_J_SHOULDER_L     53
#define ROXAS_J_ELBOW_L        56
#define ROXAS_J_WRIST_L        57
#define ROXAS_J_PALM_L         63
#define ROXAS_J_FINGER_MID_L   74
#define ROXAS_J_CLAV_R        160
#define ROXAS_J_SHOULDER_R    162
#define ROXAS_J_ELBOW_R       165
#define ROXAS_J_WRIST_R       166
#define ROXAS_J_PALM_R        172
#define ROXAS_J_FINGER_MID_R  183
#define ROXAS_J_HIP_L           6
#define ROXAS_J_KNEE_L          8
#define ROXAS_J_ANKLE_L         9
#define ROXAS_J_HEEL_L         12
#define ROXAS_J_TOE_L          13
#define ROXAS_J_HIP_R          21
#define ROXAS_J_KNEE_R         23
#define ROXAS_J_ANKLE_R        24
#define ROXAS_J_HEEL_R         27
#define ROXAS_J_TOE_R          28
#define ROXAS_J_HEAD_MESH     113

// parent of every bone (-1 = root) and the bind (rest) pose world position of every bone, cm: KH2 data, NOT in the source.
// Filled at startup from data/ragmap/roxas.bin (built at install from the player's own KH2 files, tools/sorabuild/ragmap).
static int16_t rx_rag_parent[ROXAS_RAG_BONES];

static float rx_rag_bind_pos[ROXAS_RAG_BONES][3];

// Order = top-down (parents before children), so a single pass can apply frames then limbs.
static SoraRagBone sora_ragmap_roxas[] = {
    {SORA_RAG_PELVIS, ROXAS_J_ROOT, LINK_J_SKL_ROOT, ROXAS_J_SPINE_3, LINK_J_SPINE_1, ROXAS_J_HIP_L, ROXAS_J_HIP_R, LINK_J_LEG_1_L, LINK_J_LEG_1_R, {0, 0, 0}, 0, "pelvis"},
    {SORA_RAG_CHEST, ROXAS_J_SPINE_3, LINK_J_SPINE_1, ROXAS_J_NECK, LINK_J_NECK, ROXAS_J_SHOULDER_L, ROXAS_J_SHOULDER_R, LINK_J_ARM_1_L, LINK_J_ARM_1_R, {0, 0, 0}, 0, "chest (jacket bone; arms + head hang off it)"},
    {SORA_RAG_LIMB, ROXAS_J_NECK, LINK_J_NECK, ROXAS_J_HEAD, LINK_J_HEAD, -1, -1, -1, -1, {0, 0, 0}, 0, "neck"},
    {SORA_RAG_HEAD, ROXAS_J_HEAD, LINK_J_HEAD, ROXAS_J_HEAD_TOP, LINK_J_HAIR_ROOT, -1, -1, -1, -1, {0, 0, 0}, 0, "head (Link Head orientation delta; the position pair is a weak fallback)"},
    {SORA_RAG_LIMB, ROXAS_J_CLAV_L, LINK_J_CLAVICLE_L, ROXAS_J_SHOULDER_L, LINK_J_ARM_1_L, -1, -1, -1, -1, {0, 0, 0}, 0, "clavicle L"},
    {SORA_RAG_LIMB, ROXAS_J_SHOULDER_L, LINK_J_ARM_1_L, ROXAS_J_ELBOW_L, LINK_J_ARM_2_L, -1, -1, -1, -1, {0, 0, 0}, 0, "upper arm L"},
    {SORA_RAG_LIMB, ROXAS_J_ELBOW_L, LINK_J_ARM_2_L, ROXAS_J_WRIST_L, LINK_J_WRIST_L, -1, -1, -1, -1, {0, 0, 0}, 0, "forearm L"},
    {SORA_RAG_LIMB, ROXAS_J_WRIST_L, LINK_J_WRIST_L, ROXAS_J_FINGER_MID_L, LINK_J_FINGER_C_1_L, -1, -1, -1, -1, {0, 0, 0}, 0, "hand L (optional: Finger_C = middle finger assumed)"},
    {SORA_RAG_LIMB, ROXAS_J_CLAV_R, LINK_J_CLAVICLE_R, ROXAS_J_SHOULDER_R, LINK_J_ARM_1_R, -1, -1, -1, -1, {0, 0, 0}, 0, "clavicle R"},
    {SORA_RAG_LIMB, ROXAS_J_SHOULDER_R, LINK_J_ARM_1_R, ROXAS_J_ELBOW_R, LINK_J_ARM_2_R, -1, -1, -1, -1, {0, 0, 0}, 0, "upper arm R"},
    {SORA_RAG_LIMB, ROXAS_J_ELBOW_R, LINK_J_ARM_2_R, ROXAS_J_WRIST_R, LINK_J_WRIST_R, -1, -1, -1, -1, {0, 0, 0}, 0, "forearm R"},
    {SORA_RAG_LIMB, ROXAS_J_WRIST_R, LINK_J_WRIST_R, ROXAS_J_FINGER_MID_R, LINK_J_FINGER_C_1_R, -1, -1, -1, -1, {0, 0, 0}, 0, "hand R (optional; the Keyblade rides on 178 under it)"},
    {SORA_RAG_LIMB, ROXAS_J_HIP_L, LINK_J_LEG_1_L, ROXAS_J_KNEE_L, LINK_J_LEG_2_L, -1, -1, -1, -1, {0, 0, 0}, 0, "thigh L"},
    {SORA_RAG_LIMB, ROXAS_J_KNEE_L, LINK_J_LEG_2_L, ROXAS_J_ANKLE_L, LINK_J_ANKLE_L, -1, -1, -1, -1, {0, 0, 0}, 0, "shin L"},
    {SORA_RAG_LIMB, ROXAS_J_ANKLE_L, LINK_J_ANKLE_L, ROXAS_J_TOE_L, LINK_J_TOE_L, -1, -1, -1, -1, {0, 0, 0}, 0, "foot L (ankle -> shoe tip)"},
    {SORA_RAG_LIMB, ROXAS_J_HIP_R, LINK_J_LEG_1_R, ROXAS_J_KNEE_R, LINK_J_LEG_2_R, -1, -1, -1, -1, {0, 0, 0}, 0, "thigh R"},
    {SORA_RAG_LIMB, ROXAS_J_KNEE_R, LINK_J_LEG_2_R, ROXAS_J_ANKLE_R, LINK_J_ANKLE_R, -1, -1, -1, -1, {0, 0, 0}, 0, "shin R"},
    {SORA_RAG_LIMB, ROXAS_J_ANKLE_R, LINK_J_ANKLE_R, ROXAS_J_TOE_R, LINK_J_TOE_R, -1, -1, -1, -1, {0, 0, 0}, 0, "foot R"},
};
#define ROXAS_RAGMAP_N 18
