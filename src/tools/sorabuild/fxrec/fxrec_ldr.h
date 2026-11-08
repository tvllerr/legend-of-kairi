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
#define KH2_SHA1      "2548F1191FE6587A34947952763CE6BB4F3B5C1A"   /* Steam KH2 FM exe; the only build our RVAs fit */
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
