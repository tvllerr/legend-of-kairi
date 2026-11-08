// Addresses in BOTW's main module the Sora mod calls, hooks or reads, per game version (recon/S61_HELPER_SITES.md;
// tools/sites/sora_sites.py finds them by pattern, KNOWN = these rows). Field offsets are the same on 1.8.2 and 1.9.0
// (all 59 with a pattern checked), so they stay literals at their uses. linkpos.c picks the row for g_game_version at
// linkpos_init; another version gets all zeros and every hook / call / read through them is skipped.
#pragma once
#include <stdint.h>

//  X(name,                    1.8.2,           1.9.0)
#define SORA_SITES(X)                                               \
    /* linkpos.c: velocity servo, EVEL, air hold */                 \
    X(cc_vel_step,             0x70011c0830ull, 0x70012d4680ull)    \
    X(cc_vel_caller,           0x70011c3a98ull, 0x70012d78e8ull)    \
    X(rb_set_linear_velocity,  0x70011bf3c0ull, 0x70012d3210ull)    \
    X(rb_set_angular_velocity, 0x70011bf458ull, 0x70012d32a8ull)    \
    X(phys_system_got,         0x7001c05ae0ull, 0x7001d66ab0ull)    \
    /* linkpos.c: hide Link */                                      \
    X(opacity_apply,           0x7001412f68ull, 0x7001527108ull)    \
    X(opacity_call_a,          0x700068bd3cull, 0x700079f26cull)    \
    X(opacity_call_b,          0x7001414a1cull, 0x7001528bbcull)    \
    X(opacity_call_c,          0x700141bdf8ull, 0x700152ff98ull)    \
    /* linkpos.c: damage, deaths */                                 \
    X(dmg_add_enemy,           0x7000a82004ull, 0x7000b95514ull)    \
    X(dmg_fall_gather,        0x7000a7a010ull, 0x7000b8d520ull)    \
    X(dmg_add_player,          0x7000bb1adcull, 0x7000cc4fbcull)    \
    X(die_enemy,               0x70004682bcull, 0x700057b7ecull)    \
    X(die_base,                0x700141c4a8ull, 0x7001530648ull)    \
    X(die_enemy_caller_a,      0x700047bd60ull, 0x700058f290ull)    \
    X(die_enemy_caller_b,      0x7000684c6cull, 0x700079819cull)    \
    /* linkpos.c: ray cast, gdt, fire, enemy target redirect */     \
    X(ray_ground_seg,          0x7000ac4390ull, 0x7000bd78a0ull)    \
    X(gdt_mgr,                 0x7001cd9908ull, 0x7001e3b188ull)    \
    X(gdt_get_bool,            0x700102385cull, 0x70011375ecull)    \
    X(gdt_get_s32,             0x70010238f4ull, 0x7001137684ull)    \
    X(safestring_vt_got,       0x7001c05290ull, 0x7001d66260ull)    \
    X(fire_ignite,             0x7000fddd24ull, 0x70010f1ab4ull)    \
    X(fire_spark,              0x70012f24a0ull, 0x7001406600ull)    \
    X(fire_burst,              0x7001318da4ull, 0x700142cf04ull)    \
    X(tgt_update,              0x7000690228ull, 0x70007a3758ull)    \
    X(tgt_update_caller,       0x700046a780ull, 0x700057dcb0ull)    \
    X(ai_get_name,             0x7000f6227cull, 0x700107600cull)    \
    /* linkpos.c + sora.c */                                        \
    X(pause_state,             0x7001cb48a4ull, 0x7001e1611cull)    \
    X(player_info,             0x7001cb2658ull, 0x7001e13ed0ull)    \
    X(screen_mgr_got,          0x7001c05d88ull, 0x7001d66d58ull)    \
    X(stamina_show_check,      0x7000c5f90cull, 0x7000d72bdcull)    \
    X(stamina_gauge_vptr,      0x7001b7ad70ull, 0x7001cdbc58ull)    \
    X(stamina_cores,           0x7001c4bb00ull, 0x7001dad1a0ull)    \
    /* main's .data .. end (upstream's main_data / main_end): the VFR and vtable scans */ \
    X(main_data,               0x7001a1f000ull, 0x7001b7a000ull)    \
    X(main_end,                0x7001d37000ull, 0x7001e98000ull)    \
    /* s90b: Guardian beam fire action vslot 0x100 = the reflect check (rec+0x18 & 0xA) != 0 (recon/S90_HELPER_PARRY.md); */ \
    /* 1.9.0 row: same 5 words, unique in main, slot 0x100 of the same 4 beam vtables (checked s90b) */                   \
    X(beam_reflect_chk,        0x70004f1e08ull, 0x7000605338ull)    \
    /* s98 b13-pouch: ui::PauseMenuDataMgr::removeItem(this, const SafeString&) + the GOT slot of PauseMenuDataMgr::sInstance */ \
    /* (recon/S98_HELPER_POUCH.md; 1.9.0 VERIFIED static; 1.8.2 = the same code, found by its "PouchDelete" xrefs) */          \
    X(pouch_remove_item,       0x7000c80050ull, 0x7000d93320ull)    \
    X(pmdm_got,                0x7001c06b20ull, 0x7001d67af0ull)    \
    /* s99 b14-runes: the Sheikah Slate rune wheel object (+0x240 requested / +0x248 current rune id 0..7) and its setter */ \
    /* f(obj, id) (locks +0x200, writes +0x240; the object's per-frame update then applies it and writes the gdt flag */      \
    /* PlayerEquipItemType). rune_sel_ptr = the global holding the object pointer (recon/S99_HELPER_RUNES.md). 1.9.0 VERIFIED */ \
    /* static (wheel screen confirm tail 0x7100e25900 loads [0x7101e0cd40], branches to 0x7100b33d6c); 1.8.2 = the same tail */   \
    /* at 0x7100d12600 (pointer var 0x7101cab4e0, setter 0x7100a2085c, unique 7-word match of the 1.9.0 prologue) */           \
    X(rune_set_req,            0x7000a2085cull, 0x7000b33d6cull)    \
    X(rune_sel_ptr,            0x7001cab4e0ull, 0x7001e0cd40ull)    \
    /* s100 slatehip2: the Actor calc's bl of gsys::Model::calcWorld(model), right after the actor's mMtx was copied to */ \
    /* model+0x58..0x87. slate_model_call = that bl (inside the Actor calc), slate_model_target = the callee (ldr x1,[x0,#0xd8]; b). */ \
    /* 1.8.2 by hand, 1.9.0 by an instruction-word pattern of the 12 words around the bl (one hit); both capstone-checked */ \
    X(slate_model_call,        0x7001411f54ull, 0x70015260f4ull)    \
    X(slate_model_target,      0x7000e645ccull, 0x7000f782bcull)    \
    /* s108 slatexlu: ModelMgr apply-alpha-to-every-material (applyOpacityIfDirty's bl, FAINT_LINK_RECON.md) and the GOT */ \
    /* entry of the ModelMgr* it is called with (x0 = [[got]]); 1.9.0 read off applyOpacityIfDirty's capstone listing */ \
    /* (adrp 0x7001d66000 + ldr #0x210, bl 0x700124cd10); 1.8.2 GOT not looked up (0 = the fix is skipped there) */ \
    X(model_alpha_apply,       0x7001138f80ull, 0x700124cd10ull)    \
    X(model_mgr_got,           0x0000000000ull, 0x7001d66210ull)    \
    /* s122 kbworld (recon/S122_HELPER_TREES.md, S122_HELPER_STASIS.md, 1.9.0: S122_HELPER_SITES190.md): the LumberjackTree */ \
    /* AI's fell(ai) + its vtable (check only); StasisInfo::add, the stasis pool's global, the dm vtable slot 17 (check only) */ \
    X(tree_fell,               0x700083e06cull, 0x700095157cull)    \
    X(tree_ai_vt,              0x7001b103f0ull, 0x7001c71338ull)    \
    X(stasis_add,              0x7000a7ce38ull, 0x7000b90348ull)    \
    X(stasis_pool,             0x7001cabb40ull, 0x7001e0d3a0ull)    \
    X(stasis_dm_slot17,        0x7000a7b42cull, 0x7000b8e93cull)    \
    /* s137b FPS camera: sead::LookAtCamera::doUpdateMatrix(this, Matrix34f *dst) (vtable slot 4 of 1.9.0's 0x7001d05158 / 0x7001d1eee0; */ \
    /* 1.9.0 VERIFIED (decomp recon/ghidra/out/s137cam2.c); 1.8.2 = the same 0xF0 bytes, unique in that image) */                          \
    X(lookat_update_mtx,       0x7000d9f70cull, 0x7000eb2d5cull)    \
    /* NEW ROWS GO HERE, AT THE END: sora_mod.dll reads SoraSites by field offset (sora_sites()), so a row in the middle */ \
    /* shifts gdt_mgr / pause_state / ... for a DLL built against the old header (s166 review). */                       \
    /* s166 edmg: applyDamage (DamageManager vslot 15, shared by all 7 enemy/animal/weapon dm vtables): bool(dm x0, s32 *life x1). */ \
    /* pattern (first 12 words, unique in both mains): d10143ff a9027bfd f9001bf5 a9044ff4 910083fd f9400c08 aa0003f4 f942b908 f9405508 b942b109 f9415d08 7100053f */ \
    X(dmg_apply_enemy,         0x7000a81da8ull, 0x7000b952b8ull)    \
    /* s168 esync (recon/S168_HELPER_ESYNC_EXE.md; R1 = S168_HELPER_ESYNC_R1.md). Patterns = instruction words, bl masked (-> 94000000), */ \
    /* each unique in the 1.9.0 main (1.8.2: the same pattern is unique there too). A 0 in the 1.8.2 column = not looked up: the feature stays OFF there. */ \
    /* rootai_calc = RootAi::calc (RootAi vtable slot 20): a9bd7bfd f9000bf5 a9024ff4 910003fd aa0003f3 94000000 7942da68 37400368 aa1303e0 94000000 f9400668 f942cd08 */ \
    X(rootai_calc,             0x7000fb4a84ull, 0x70010c8814ull)    \
    /* actor_set_mtx = Actor vslot 85 setMtx(actor, Matrix34f*, w2 model, w3 snap): a9bd7bfd a90157f6 a9024ff4 910003fd 2a0303f3 aa0103f4 aa0003f5 36000282 3dc00280 910e62a8 3d800100 3dc00680 */ \
    X(actor_set_mtx,           0x700141f60cull, 0x70015337acull)    \
    /* as_start = ASList startAnimation(s0 blend, s1 phase, ASList, SafeString*, slot, bank, flag): f9406c08 b4000328 d10203ff 6d0423e9 a9057bfd a90657f6 a9074ff4 910143fd b940b808 aa0003f3 2a0203f5 6b02011f */ \
    X(as_start,                0x700138b0f0ull, 0x700149f250ull)    \
    /* ActivateAttackSensor enter_ / calc_ / leave_ (vslots 12 / 31 / 14 of the class vtable atk_vptr): */ \
    /* enter_ d10103ff a9027bfd a9034ff4 910083fd f9403008 aa0003f3 bd400100 52b7f008 3902901f b900a008 f9403808 2d130000 */ \
    /* calc_  a9be7bfd a9014ff4 910003fd 39429008 34000088 a9414ff4 a8c27bfd d65f03c0 aa0003f3 91026000 94000000 bd409a60 */ \
    /* leave_ a9be7bfd a9014ff4 910003fd aa0003f3 f9400400 91020274 aa1403e1 94000000 f9400660 aa1403e1 aa1f03e2 a9414ff4 */ \
    X(atk_enter,               0x70004969acull, 0x70005a9edcull)    \
    X(atk_calc,                0x7000496a90ull, 0x70005a9fc0ull)    \
    X(atk_leave,               0x7000496b10ull, 0x70005aa040ull)    \
    /* the sensor helpers those three call (1.9.0 only; the 12-word patterns of rec / on / layer_off are NOT unique, these 20-word ones are): */ \
    /* atk_rec (actor) -> AttackInfo rec*: a9be7bfd f9000bf3 910003fd f9400008 f941f508 d63f0100 aa0003f3 94000000 b4000133 f9400268 aa0003e1 aa1303e0 f9400108 d63f0100 36000060 f9402260 14000000 aa1f03e0 f9400bf3 a8c27bfd */ \
    /* atk_on (actor, SafeString* name, mtx*): a9bd7bfd a90157f6 a9024ff4 910003fd aa0203f3 aa0103f4 aa0003f5 94000000 f9400008 aa0003f6 f9400d08 d63f0100 f94006c1 aa1503e0 94000000 b50000a0 a9424ff4 a94157f6 a8c37bfd d65f03c0 */ \
    /* atk_layer_off (actor, SafeString*, 0): a9bd7bfd a90157f6 a9024ff4 910003fd aa0203f3 aa0103f4 aa0003f5 94000000 f9400008 aa0003f6 f9400d08 d63f0100 f94006c1 aa1503e0 94000000 b4000140 aa1403e1 94000000 b40000e0 aa1303e2 */ \
    /* atk_off (actor, SafeString*): its 12 words with the bl UNMASKED are unique (two siblings differ only in the bl): */ \
    /*   a9bd7bfd f9000bf5 a9024ff4 910003fd aa0103f3 aa0003f4 9417648a f9400008 aa0003f5 f9400d08 d63f0100 f94006a1 */ \
    /* atk_layer_on (actor, SafeString*, 0): d10103ff a9017bfd a90257f6 a9034ff4 910043fd f942bc14 b4000294 aa0203f3 aa0103f5 94000000 f9400008 aa0003f6 */ \
    X(atk_rec,                 0x0000000000ull, 0x7000c38d40ull)    \
    X(atk_on,                  0x0000000000ull, 0x7000c38e30ull)    \
    X(atk_off,                 0x0000000000ull, 0x7000c38f80ull)    \
    X(atk_layer_on,            0x0000000000ull, 0x7000c391c0ull)    \
    X(atk_layer_off,           0x0000000000ull, 0x7000c393a0ull)    \
    /* data (1.9.0 only): the RootAi vtable (the object's vptr; slot 20 = rootai_calc), the ActivateAttackSensor vtable (slots 12 / 14 / 31 = */ \
    /* enter_ / leave_ / calc_; both slot checks done in the image), the GOT slot of the 'no ASList' sentinel (actor+0x560 == [slot] means none) */ \
    X(rootai_vptr,             0x0000000000ull, 0x7001d1dec0ull)    \
    X(atk_vptr,                0x0000000000ull, 0x7001bc93e8ull)    \
    X(aslist_none_got,         0x0000000000ull, 0x7001d77810ull)    \
    /* s174 R2 (recon/s174/S174_ATK_VPTRS.md): attack ACTION class vtables (1.9.0 only), found via the AI factory table crc32(name) -> ctor -> vtable */ \
    /* (the same method gives atk_vptr above). Slots 12 / 14 / 31 = enter_ / leave_ / calc_. A puppet runs its own node of these classes. */ \
    X(atkact_attack_vt,        0x0000000000ull, 0x7001bd63e0ull)    \
    X(atkact_stepdouble_vt,    0x0000000000ull, 0x7001c2e1d0ull)    \
    X(atkact_large_vt,         0x0000000000ull, 0x7001c0e9c0ull)    \
    X(atkact_jump_vt,          0x0000000000ull, 0x7001c0cb30ull)    \
    X(atkact_forkweapon_vt,    0x0000000000ull, 0x7001bfdab0ull)    \
    X(atkact_animtiming_vt,    0x0000000000ull, 0x7001bd2668ull)

typedef struct {
#define SORA_SITE_FIELD(n, a, b) uint64_t n;
    SORA_SITES(SORA_SITE_FIELD)
#undef SORA_SITE_FIELD
} SoraSites;

// exe (linkpos.c), also for sora_mod.dll: the running game version's row (all zeros for a version not listed)
const SoraSites *sora_sites(void);
