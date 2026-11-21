/* fxrec_ldr.h - our own headless loader for the KH2 FM PC exe (s156, written from scratch; no PHH code).
 *
 * Maps the exe at its preferred base 0x140000000 WITHOUT running its entry point, binds every import to a
 * counting stub (only pure CRT functions - math, rand, mem/str, heap, sprintf - go to the real ucrtbase /
 * vcruntime), gives KH2 code its own fake TLS array while it runs (so its thread-safe statics work), registers
 * its .pdata, patches/hooks functions, and runs KH2 functions under a fault guard.
 * Windows x64 only (MSYS2 UCRT64 clang: the MS x64 calling convention is native, hooks jump straight to C).
 */
#ifndef FXREC_LDR_H
#define FXREC_LDR_H
#include <stdint.h>
#include <stddef.h>

#define KH2_BASE      0x140000000ull
#define KH2_SHA1_STEAM "2548F1191FE6587A34947952763CE6BB4F3B5C1A"   /* Steam KH2 FM exe (comments name its RVAs) */
#define KH2_SHA1_EPIC  "F9758C7782C4DD03C5BF8A4BCCE9D897CB21CD62"   /* Epic Games Store KH2 FM exe */

/* Every KH2 exe address fxrec uses: X(name, Steam RVA, Epic RVA). ldr_map picks the column by the exe's sha1;
 * an exe that is in no column is refused. Epic column: the same functions found in the Epic exe with headless
 * Ghidra 12.1.2 (identical instruction shape, paired through unique shapes and matched callers) and the same
 * data addresses (the same instruction in matched functions references them, every reference agreeing).
 * Comments elsewhere in fxrec keep naming the Steam RVAs. */
#define KH2_RVAS(X) \
    X(DEV_GET,        0x11C150,  0x120780)  /* returns &device (thread-safe static 0x8A0970, ctor 0x10A270) */ \
    X(FX_INIT,        0x2BDB50,  0x2BCFF0)  /* (allocator*, pool 0x80000, sort 0x20000) */ \
    X(ALLOC_G,        0xB63620,  0xB630A0)  /* allocator global (0x2C4330 writes it) */ \
    X(FX_ALLOC,       0xB1DA20,  0xB1D4A0)  /* 0x2BDB50: allocator, +8 pool, +0x10 sort buffer */ \
    X(PPP_MGR,        0x2B0EC08, 0x2B0EA48) /* 0x2C2480: = &PPP_MGR_VAL after init */ \
    X(PPP_MGR_VAL,    0xB60808,  0xB60288) \
    X(TEXCOMMON,      0x2BEC60,  0x2BE100)  /* texcommon(dpd) */ \
    X(DRAW_FLAG,      0xB1D984,  0xB1D404)  /* "drawing on" flag, set by texcommon */ \
    X(HD_LOOKUP,      0x12AA20,  0x104FB0) \
    X(VRAM_LOOKUP,    0x12C0F0,  0x106680) \
    X(VRAM_REGISTER,  0x12C620,  0x106BB0) \
    X(Q_LAMBDA,       0x119BA0,  0x11E1D0) \
    X(Q_LAMBDA2,      0x123700,  0x127D30) \
    X(CTX_GET,        0x11BB30,  0x120160) \
    X(DRAW_CORE,      0x11A330,  0x11E960) \
    X(MODE_Q,         0x121310,  0x125940) \
    X(BIND_TEX,       0x127240,  0x12B870) \
    X(PREVTEX,        0x126C30,  0x12B260) \
    X(FRAMETEX,       0x126890,  0x12AEC0) \
    X(PAX_FIX,        0x2C4520,  0x2C39C0) \
    X(FX_START,       0x2C6B30,  0x2C5FD0) \
    X(FX_UPDATE,      0x2BD190,  0x2BC630) \
    X(FX_DRAW,        0x2BD830,  0x2BCCD0) \
    X(FX_LOOP_END,    0x2BE8E0,  0x2BDD80) \
    X(PM_FREE1,       0x4B01C0,  0x4B1BB0) \
    X(PM_FREE2,       0x2C1F50,  0x2C13F0) \
    X(SORT_INSERT,    0x4AF7D0,  0x4B11C0) \
    X(FRAME_TIME,     0x717480,  0x717270) \
    X(B63628,         0xB63628,  0xB630A8)  /* == 1 -> 0x2C6B30 calls 0x3C8000(owner + 0x158): keep it 0 */ \
    X(CAMBLK,         0x2B0EC20, 0x2B0EA60) \
    X(SORT_SCALE,     0x5A9C9C,  0x5AA344)  /* float const */ \
    X(TLS_NOTIFY_RET, 0x439FC8,  0x439650)  /* _Init_thread_notify head, patched to ret */

enum {
#define KH2_RV_ENUM(n, s, e) RV_##n,
    KH2_RVAS(KH2_RV_ENUM)
#undef KH2_RV_ENUM
    RV_COUNT
};
extern const uint32_t *g_kh2_rva;   /* the column ldr_map picked */
extern const char *g_kh2_build;     /* "Steam" / "Epic" */
#define R(n)          (g_kh2_rva[RV_##n])

#define KH(rva)       ((void *)(uintptr_t)(KH2_BASE + (uint64_t)(rva)))
#define KHU64(rva)    (*(uint64_t *)KH(rva))
#define KHU32(rva)    (*(uint32_t *)KH(rva))
#define KHF32(rva)    (*(float *)KH(rva))

/* registers seen by a "record" hook / import stub (filled by the asm dispatcher) */
typedef struct LdrRegs {
    uint64_t rcx, rdx, r8, r9;
    uint64_t xmm0, xmm1, xmm2, xmm3;     /* low 64 bits */
    uint64_t *stk;                       /* caller's stack at entry+8: stk[4] = arg 5, stk[5] = arg 6, ... */
} LdrRegs;
typedef uint64_t (*LdrHandler)(uint32_t id, LdrRegs *r, void *user);

/* fault / event log */
typedef struct LdrFault {
    uint32_t code;          /* exception code (0xC0000005 AV, 0xC000001D illegal, ...) */
    uint64_t rip, addr;     /* faulting rip; AV: data address */
    int      write;         /* AV: 1 = write, 0 = read, 8 = DEP */
    uint64_t rsp;
    uint64_t stk[256];      /* copy of the stack at the fault (it is overwritten once we resume) */
    int      nstk;
} LdrFault;

/* load: returns 0 on success, else writes a reason into err */
int  ldr_map(const char *exe_path, int check_sha1, char *err, size_t errsz);
void ldr_set_verbose(int v);

/* hooks: overwrite the function head at rva */
int  ldr_hook(uint32_t rva, void *fn);                                 /* jmp to a C function (same signature) */
int  ldr_hook_rec(uint32_t rva, const char *name, LdrHandler h, void *user); /* jmp to the dispatcher (id = hook) */
int  ldr_patch_ret(uint32_t rva, uint64_t rax);                        /* mov rax,imm; ret */
int  ldr_unhook(uint32_t rva);                                         /* restore the original bytes */
void *ldr_make_thunk(const char *name, LdrHandler h, void *user);      /* callable thunk (fake vtables) */

/* guarded call of KH2 code: up to 8 integer/pointer args (MS x64: extra args are harmless).
 * returns 0 = ok (*ret = rax), -1 = faulted (ldr_last_fault()). */
int  ldr_call(uint64_t va, int nargs, const uint64_t *args, uint64_t *ret);
int  ldr_call_rva(uint32_t rva, int nargs, const uint64_t *args, uint64_t *ret);
const LdrFault *ldr_last_fault(void);
void ldr_print_fault(const LdrFault *f);

/* reports */
void ldr_report_imports(int only_called);       /* stubbed imports that KH2 called (name, count, last args) */
void ldr_report_hooks(void);                    /* record hooks: name, calls, last args */
uint64_t ldr_hook_calls(uint32_t rva);

/* helpers */
int  ldr_in_image(uint64_t va);
uint32_t ldr_rva(uint64_t va);                  /* va -> rva (or 0xFFFFFFFF) */
uint64_t ldr_image_size(void);
#endif
