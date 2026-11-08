/* fxrec_ldr.c - headless loader for the KH2 FM PC exe (s156). See fxrec_ldr.h. Written from scratch. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <intrin.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "fxrec_ldr.h"

/* ---------------------------------------------------------------- state */
enum { TK_FREE, TK_IMPORT, TK_FATAL, TK_HOOK, TK_THUNK };
typedef struct Thunk {
    char       name[96];
    int        kind;
    LdrHandler h;
    void      *user;
    uint64_t   calls;
    uint64_t   last[6];
    uint32_t   rva;            /* hooks: patched rva */
} Thunk;

#define MAX_THUNKS 8192
#define THUNK_BYTES 32
static Thunk    g_th[MAX_THUNKS];
static uint32_t g_nth;
static uint8_t *g_thunk_code;           /* RWX, MAX_THUNKS * THUNK_BYTES */

typedef struct Patch { uint32_t rva; uint8_t orig[16]; int used; } Patch;
#define MAX_PATCH 1024
static Patch    g_patch[MAX_PATCH];
static uint32_t g_npatch;

static uint8_t *g_base;
static uint64_t g_size;
static int      g_verbose;

/* fake TLS for KH2 code: an array of block pointers (every index -> the same block = KH2's TLS template) */
static uint64_t *g_fake_tls;
static uint64_t  g_real_tls;

/* fault guard */
static CONTEXT          g_ctx;
static volatile LONG    g_in_call;
static volatile LONG    g_faulted;
static LdrFault         g_fault;
static const char      *g_fatal_name;

/* ---------------------------------------------------------------- dispatcher (asm) */
/* thunk: mov r10, id ; mov rax, ldr_rec_common ; jmp rax.  ldr_rec_common saves the argument registers into a
 * LdrRegs on its own stack and calls ldr_dispatch(id, regs); rax (and xmm0 = same bits) is the return value. */
uint64_t ldr_dispatch(uint32_t id, LdrRegs *r) __attribute__((used));
void ldr_rec_common(void);
__asm__(
    ".text\n"
    ".globl ldr_rec_common\n"
    "ldr_rec_common:\n"
    ".intel_syntax noprefix\n"
    "  sub rsp, 0x68\n"
    "  mov [rsp+0x20], rcx\n"
    "  mov [rsp+0x28], rdx\n"
    "  mov [rsp+0x30], r8\n"
    "  mov [rsp+0x38], r9\n"
    "  movq [rsp+0x40], xmm0\n"
    "  movq [rsp+0x48], xmm1\n"
    "  movq [rsp+0x50], xmm2\n"
    "  movq [rsp+0x58], xmm3\n"
    "  lea rax, [rsp+0x70]\n"
    "  mov [rsp+0x60], rax\n"
    "  mov ecx, r10d\n"
    "  lea rdx, [rsp+0x20]\n"
    "  call ldr_dispatch\n"
    "  movq xmm0, rax\n"
    "  add rsp, 0x68\n"
    "  ret\n"
    ".att_syntax prefix\n");

uint64_t ldr_dispatch(uint32_t id, LdrRegs *r)
{
    if (id >= g_nth) __builtin_trap();
    Thunk *t = &g_th[id];
    t->calls++;
    t->last[0] = r->rcx; t->last[1] = r->rdx; t->last[2] = r->r8; t->last[3] = r->r9;
    t->last[4] = r->stk[4]; t->last[5] = r->stk[5];
    if (t->kind == TK_FATAL) { g_fatal_name = t->name; __builtin_trap(); }
    if (t->h) return t->h(id, r, t->user);
    return 0;
}

static void *new_thunk(const char *name, int kind, LdrHandler h, void *user, uint32_t *id_out)
{
    if (g_nth >= MAX_THUNKS) return NULL;
    uint32_t id = g_nth++;
    Thunk *t = &g_th[id];
    memset(t, 0, sizeof *t);
    snprintf(t->name, sizeof t->name, "%s", name);
    t->kind = kind; t->h = h; t->user = user;
    uint8_t *p = g_thunk_code + (size_t)id * THUNK_BYTES;
    uint64_t imm = id, tgt = (uint64_t)(uintptr_t)&ldr_rec_common;
    p[0] = 0x49; p[1] = 0xBA; memcpy(p + 2, &imm, 8);        /* mov r10, id */
    p[10] = 0x48; p[11] = 0xB8; memcpy(p + 12, &tgt, 8);     /* mov rax, ldr_rec_common */
    p[20] = 0xFF; p[21] = 0xE0;                              /* jmp rax */
    if (id_out) *id_out = id;
    return p;
}

void *ldr_make_thunk(const char *name, LdrHandler h, void *user) { return new_thunk(name, TK_THUNK, h, user, NULL); }

/* ---------------------------------------------------------------- small helpers */
void ldr_set_verbose(int v) { g_verbose = v; }
int ldr_in_image(uint64_t va) { return g_base && va >= (uint64_t)(uintptr_t)g_base && va < (uint64_t)(uintptr_t)g_base + g_size; }
uint32_t ldr_rva(uint64_t va) { return ldr_in_image(va) ? (uint32_t)(va - (uint64_t)(uintptr_t)g_base) : 0xFFFFFFFFu; }
uint64_t ldr_image_size(void) { return g_size; }

static int sha1_hex(const uint8_t *d, size_t n, char out[41])
{
    BCRYPT_ALG_HANDLE alg = NULL; BCRYPT_HASH_HANDLE hh = NULL; uint8_t dig[20]; int ok = 0;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA1_ALGORITHM, NULL, 0) != 0) return 0;
    if (BCryptCreateHash(alg, &hh, NULL, 0, NULL, 0, 0) == 0 &&
        BCryptHashData(hh, (PUCHAR)d, (ULONG)n, 0) == 0 &&
        BCryptFinishHash(hh, dig, 20, 0) == 0) ok = 1;
    if (hh) BCryptDestroyHash(hh);
    BCryptCloseAlgorithmProvider(alg, 0);
    for (int i = 0; ok && i < 20; i++) sprintf(out + 2 * i, "%02X", dig[i]);
    return ok;
}

static void lower(char *s) { for (; *s; s++) *s = (char)tolower((unsigned char)*s); }
static int starts(const char *s, const char *p) { return strncmp(s, p, strlen(p)) == 0; }

/* ---------------------------------------------------------------- import policy
 * REAL = resolve from the real CRT (pure functions: math, rand, memory/string, heap, sprintf, _errno).
 * FATAL = KH2 would never come back (throw/abort/exit): stop the call as a fault.
 * everything else = counting stub that returns 0 (D3D, window, file, Steam, threads, locks, timers...). */
enum { POL_STUB, POL_REAL, POL_FATAL };
static int import_policy(const char *dll_lc, const char *name)
{
    static const char *fatal[] = { "abort", "exit", "_exit", "terminate", "_invalid_parameter_noinfo_noreturn",
        "_CxxThrowException", "__std_terminate", "_purecall", "?_Throw_C_error@std@@YAXH@Z",
        "?_Xbad_function_call@std@@YAXXZ", "RaiseException", "ExitProcess", "TerminateProcess", NULL };
    for (int i = 0; fatal[i]; i++) if (strcmp(name, fatal[i]) == 0) return POL_FATAL;
    if (starts(dll_lc, "api-ms-win-crt-math") || starts(dll_lc, "api-ms-win-crt-utility") ||
        starts(dll_lc, "api-ms-win-crt-string") || starts(dll_lc, "api-ms-win-crt-heap") ||
        starts(dll_lc, "api-ms-win-crt-convert")) return POL_REAL;
    if (starts(dll_lc, "api-ms-win-crt-stdio") && starts(name, "__stdio_common_vs")) return POL_REAL;  /* s(w)printf/sscanf to buffers */
    if (starts(dll_lc, "api-ms-win-crt-runtime") && strcmp(name, "_errno") == 0) return POL_REAL;
    if (starts(dll_lc, "vcruntime140")) {
        if (starts(name, "mem") || starts(name, "str") || starts(name, "wcs") ||
            strcmp(name, "__intrinsic_setjmp") == 0 || strcmp(name, "longjmp") == 0) return POL_REAL;
    }
    return POL_STUB;
}

static HMODULE g_ucrt, g_vcrt;
static void *real_import(const char *dll_lc, const char *name)
{
    void *p = NULL;
    if (starts(dll_lc, "vcruntime140")) { if (g_vcrt) p = (void *)GetProcAddress(g_vcrt, name); }
    if (!p && g_ucrt) p = (void *)GetProcAddress(g_ucrt, name);
    return p;
}

/* ---------------------------------------------------------------- fault handler */
static LONG CALLBACK veh(EXCEPTION_POINTERS *ep)
{
    if (!g_in_call) return EXCEPTION_CONTINUE_SEARCH;
    DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (c == 0x406D1388u || c == 0x40010006u || c == 0x4001000Au) return EXCEPTION_CONTINUE_SEARCH; /* thread name, OutputDebugString */
    memset(&g_fault, 0, sizeof g_fault);
    g_fault.code = c;
    g_fault.rip = ep->ContextRecord->Rip;
    g_fault.rsp = ep->ContextRecord->Rsp;
    if (c == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2) {
        g_fault.write = (int)ep->ExceptionRecord->ExceptionInformation[0];
        g_fault.addr = ep->ExceptionRecord->ExceptionInformation[1];
    }
    {   /* snapshot the stack (stops at the end of the committed region) */
        MEMORY_BASIC_INFORMATION mbi;
        uint64_t sp = g_fault.rsp;
        if (sp && VirtualQuery((void *)(uintptr_t)sp, &mbi, sizeof mbi) && mbi.State == MEM_COMMIT) {
            uint64_t end = (uint64_t)(uintptr_t)mbi.BaseAddress + mbi.RegionSize;
            uint64_t n = (end - sp) / 8;
            if (n > 256) n = 256;
            memcpy(g_fault.stk, (void *)(uintptr_t)sp, n * 8);
            g_fault.nstk = (int)n;
        }
    }
    g_in_call = 0;
    g_faulted = 1;
    *ep->ContextRecord = g_ctx;                    /* resume right after RtlCaptureContext in ldr_call */
    ep->ContextRecord->ContextFlags = g_ctx.ContextFlags & ~(DWORD)CONTEXT_XSTATE;
    return EXCEPTION_CONTINUE_EXECUTION;
}

const LdrFault *ldr_last_fault(void) { return &g_fault; }

void ldr_print_fault(const LdrFault *f)
{
    uint32_t r = ldr_rva(f->rip);
    printf("FAULT code %08X at rip %016llX", f->code, (unsigned long long)f->rip);
    if (r != 0xFFFFFFFFu) printf(" (KH2 rva 0x%X)", r);
    else printf(" (outside KH2)");
    if (f->code == EXCEPTION_ACCESS_VIOLATION)
        printf(" %s %016llX", f->write == 1 ? "write" : f->write == 8 ? "exec" : "read", (unsigned long long)f->addr);
    if (g_fatal_name && f->code == EXCEPTION_ILLEGAL_INSTRUCTION) printf(" [fatal import %s]", g_fatal_name);
    printf("\n");
    /* return-address scan of the stack: KH2 code addresses = likely callers */
    if (f->nstk) {
        printf("  stack (KH2 code addresses):");
        int shown = 0;
        for (int i = 0; i < f->nstk && shown < 12; i++) {
            uint32_t rr = ldr_rva(f->stk[i]);
            if (rr != 0xFFFFFFFFu && rr >= 0x1000 && rr < 0x579000) { printf(" 0x%X", rr); shown++; }
        }
        printf("\n");
    }
}

typedef uint64_t (*Fn8)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

__attribute__((noinline)) int ldr_call(uint64_t va, int nargs, const uint64_t *args, uint64_t *ret)
{
    volatile uint64_t a[8] = { 0 };
    for (int i = 0; i < nargs && i < 8; i++) a[i] = args[i];
    g_faulted = 0;
    g_fatal_name = NULL;
    RtlCaptureContext(&g_ctx);
    if (g_faulted) {                                /* came back here from veh() */
        __writegsqword(0x58, g_real_tls);
        return -1;
    }
    g_real_tls = __readgsqword(0x58);
    __writegsqword(0x58, (uint64_t)(uintptr_t)g_fake_tls);
    g_in_call = 1;
    uint64_t r = ((Fn8)(uintptr_t)va)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
    g_in_call = 0;
    __writegsqword(0x58, g_real_tls);
    if (ret) *ret = r;
    return 0;
}

int ldr_call_rva(uint32_t rva, int nargs, const uint64_t *args, uint64_t *ret)
{
    return ldr_call(KH2_BASE + rva, nargs, args, ret);
}

/* ---------------------------------------------------------------- patching */
static Patch *patch_slot(uint32_t rva)
{
    for (uint32_t i = 0; i < g_npatch; i++) if (g_patch[i].used && g_patch[i].rva == rva) return &g_patch[i];
    if (g_npatch >= MAX_PATCH) return NULL;
    Patch *p = &g_patch[g_npatch++];
    p->rva = rva; p->used = 1;
    memcpy(p->orig, KH(rva), 16);
    return p;
}

static int write_code(uint32_t rva, const uint8_t *b, size_t n)
{
    if (!g_base || (uint64_t)rva + n > g_size) return -1;
    if (!patch_slot(rva)) return -1;
    memcpy(KH(rva), b, n);
    FlushInstructionCache(GetCurrentProcess(), KH(rva), n);
    return 0;
}

int ldr_hook(uint32_t rva, void *fn)
{
    uint8_t b[12] = { 0x48, 0xB8 };
    uint64_t t = (uint64_t)(uintptr_t)fn;
    memcpy(b + 2, &t, 8); b[10] = 0xFF; b[11] = 0xE0;        /* mov rax, fn ; jmp rax */
    return write_code(rva, b, sizeof b);
}

int ldr_hook_rec(uint32_t rva, const char *name, LdrHandler h, void *user)
{
    char nm[96];
    if (name) snprintf(nm, sizeof nm, "%s", name); else snprintf(nm, sizeof nm, "rva_%X", rva);
    uint32_t id;
    void *th = new_thunk(nm, TK_HOOK, h, user, &id);
    if (!th) return -1;
    g_th[id].rva = rva;
    return ldr_hook(rva, th);
}

int ldr_patch_ret(uint32_t rva, uint64_t rax)
{
    uint8_t b[11] = { 0x48, 0xB8 };
    memcpy(b + 2, &rax, 8); b[10] = 0xC3;                    /* mov rax, imm ; ret */
    return write_code(rva, b, sizeof b);
}

int ldr_unhook(uint32_t rva)
{
    for (uint32_t i = 0; i < g_npatch; i++)
        if (g_patch[i].used && g_patch[i].rva == rva) {
            memcpy(KH(rva), g_patch[i].orig, 16);
            FlushInstructionCache(GetCurrentProcess(), KH(rva), 16);
            g_patch[i].used = 0;
            return 0;
        }
    return -1;
}

/* ---------------------------------------------------------------- reports */
void ldr_report_imports(int only_called)
{
    printf("imports KH2 called (stubs return 0):\n");
    int n = 0;
    for (uint32_t i = 0; i < g_nth; i++) {
        Thunk *t = &g_th[i];
        if (t->kind != TK_IMPORT && t->kind != TK_FATAL) continue;
        if (only_called && !t->calls) continue;
        printf("  %-60s %8llu  rcx=%llX rdx=%llX\n", t->name, (unsigned long long)t->calls,
               (unsigned long long)t->last[0], (unsigned long long)t->last[1]);
        n++;
    }
    if (!n) printf("  (none)\n");
}

void ldr_report_hooks(void)
{
    printf("record hooks:\n");
    for (uint32_t i = 0; i < g_nth; i++) {
        Thunk *t = &g_th[i];
        if (t->kind != TK_HOOK && t->kind != TK_THUNK) continue;
        printf("  %-40s %8llu  last %llX %llX %llX %llX\n", t->name, (unsigned long long)t->calls,
               (unsigned long long)t->last[0], (unsigned long long)t->last[1],
               (unsigned long long)t->last[2], (unsigned long long)t->last[3]);
    }
}

uint64_t ldr_hook_calls(uint32_t rva)
{
    for (uint32_t i = 0; i < g_nth; i++) if (g_th[i].kind == TK_HOOK && g_th[i].rva == rva) return g_th[i].calls;
    return 0;
}

/* ---------------------------------------------------------------- map */
#define ERR(...) do { snprintf(err, errsz, __VA_ARGS__); return -1; } while (0)

int ldr_map(const char *exe_path, int check_sha1, char *err, size_t errsz)
{
    FILE *f = fopen(exe_path, "rb");
    if (!f) ERR("cannot open %s", exe_path);
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *file = (uint8_t *)malloc((size_t)n);
    if (!file || fread(file, 1, (size_t)n, f) != (size_t)n) { fclose(f); ERR("read failed"); }
    fclose(f);

    if (check_sha1) {
        char hex[41] = { 0 };
        if (!sha1_hex(file, (size_t)n, hex)) ERR("sha1 failed");
        if (strcmp(hex, KH2_SHA1) != 0) ERR("unsupported KH2 build: sha1 %s (need %s, the Steam KH2 FM exe)", hex, KH2_SHA1);
    }

    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)file;
    if (n < 0x400 || dos->e_magic != IMAGE_DOS_SIGNATURE) ERR("not an MZ file");
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(file + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) ERR("not a PE32+ x64 image");
    if (nt->OptionalHeader.ImageBase != KH2_BASE) ERR("unexpected image base %llX", (unsigned long long)nt->OptionalHeader.ImageBase);

    g_size = nt->OptionalHeader.SizeOfImage;
    g_base = (uint8_t *)VirtualAlloc((void *)(uintptr_t)KH2_BASE, g_size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (g_base != (uint8_t *)(uintptr_t)KH2_BASE)
        ERR("cannot reserve 0x%llX bytes at 0x140000000 (error %lu; something else is mapped there)",
            (unsigned long long)g_size, GetLastError());

    memcpy(g_base, file, nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        uint32_t raw = sec[i].SizeOfRawData, vs = sec[i].Misc.VirtualSize;
        uint32_t cp = raw < vs || vs == 0 ? raw : vs;
        if (sec[i].PointerToRawData + (uint64_t)cp > (uint64_t)n) ERR("section %d out of file", i);
        memcpy(g_base + sec[i].VirtualAddress, file + sec[i].PointerToRawData, cp);
    }
    /* mapped at the preferred base: relocations are a no-op */

    /* thunk area */
    g_thunk_code = (uint8_t *)VirtualAlloc(NULL, (size_t)MAX_THUNKS * THUNK_BYTES, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!g_thunk_code) ERR("thunk alloc failed");

    /* real CRT (load BEFORE any KH2 call: no LoadLibrary while the fake TLS is in place) */
    g_ucrt = LoadLibraryA("ucrtbase.dll");
    g_vcrt = LoadLibraryA("vcruntime140.dll");
    if (!g_ucrt) ERR("ucrtbase.dll not found");
    {   /* same sinf/cosf bits on every CPU: no FMA3 paths */
        typedef int (*SetFma)(int);
        SetFma sf = (SetFma)(void *)GetProcAddress(g_ucrt, "_set_FMA3_enable");
        if (sf) sf(0);
    }

    /* imports */
    IMAGE_DATA_DIRECTORY *idd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    int n_real = 0, n_stub = 0, n_fatal = 0, n_missing = 0;
    for (IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)(g_base + idd->VirtualAddress); d->Name; d++) {
        char dll[128];
        snprintf(dll, sizeof dll, "%s", (const char *)(g_base + d->Name));
        char dll_lc[128]; strcpy(dll_lc, dll); lower(dll_lc);
        uint64_t *iat = (uint64_t *)(g_base + d->FirstThunk);
        uint64_t *ilt = (uint64_t *)(g_base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        for (int k = 0; ilt[k]; k++) {
            char name[96];
            if (ilt[k] & IMAGE_ORDINAL_FLAG64) snprintf(name, sizeof name, "#%u", (unsigned)(ilt[k] & 0xFFFF));
            else snprintf(name, sizeof name, "%s", ((IMAGE_IMPORT_BY_NAME *)(g_base + (uint32_t)ilt[k]))->Name);
            char full[96];
            snprintf(full, sizeof full, "%s!%s", dll, name);
            int pol = name[0] == '#' ? POL_STUB : import_policy(dll_lc, name);
            void *p = NULL;
            if (pol == POL_REAL) {
                p = real_import(dll_lc, name);
                if (!p) { pol = POL_STUB; n_missing++; if (g_verbose) printf("  real import missing: %s (stubbed)\n", full); }
            }
            if (pol == POL_REAL) n_real++;
            else {
                p = new_thunk(full, pol == POL_FATAL ? TK_FATAL : TK_IMPORT, NULL, NULL, NULL);
                if (!p) ERR("out of thunks");
                if (pol == POL_FATAL) n_fatal++; else n_stub++;
            }
            iat[k] = (uint64_t)(uintptr_t)p;
        }
    }
    if (g_verbose) printf("imports: %d real, %d stubs, %d fatal (%d real ones missing)\n", n_real, n_stub, n_fatal, n_missing);

    /* TLS: KH2's _tls_index = 0, our fake array[0..63] -> one block = a copy of KH2's TLS template */
    IMAGE_DATA_DIRECTORY *tdd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
    g_fake_tls = (uint64_t *)VirtualAlloc(NULL, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!g_fake_tls) ERR("tls alloc failed");
    uint8_t *blk = (uint8_t *)g_fake_tls + 0x1000;      /* block: 0x1000.. (60 KB, zeroed) */
    if (tdd->VirtualAddress) {
        IMAGE_TLS_DIRECTORY64 *td = (IMAGE_TLS_DIRECTORY64 *)(g_base + tdd->VirtualAddress);
        uint64_t len = td->EndAddressOfRawData - td->StartAddressOfRawData;
        if (len > 0xE000) ERR("TLS template too big (%llu)", (unsigned long long)len);
        memcpy(blk, (void *)(uintptr_t)td->StartAddressOfRawData, len);
        if (td->AddressOfIndex) *(uint32_t *)(uintptr_t)td->AddressOfIndex = 0;
        if (g_verbose) printf("tls: template %llu bytes, epoch word (+0x10) = %08X\n", (unsigned long long)len, *(uint32_t *)(blk + 0x10));
    }
    for (int i = 0; i < 64; i++) g_fake_tls[i] = (uint64_t)(uintptr_t)blk;

    /* unwind info, so real longjmp / stack walks work through KH2 frames */
    IMAGE_DATA_DIRECTORY *edd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    if (edd->VirtualAddress && edd->Size)
        if (!RtlAddFunctionTable((PRUNTIME_FUNCTION)(g_base + edd->VirtualAddress), edd->Size / sizeof(RUNTIME_FUNCTION),
                                 (DWORD64)(uintptr_t)g_base) && g_verbose) printf("  RtlAddFunctionTable failed\n");

    /* thread-safe statics: KH2's own _Init_thread_header/footer run as they are (lock = stub, guards + TLS epoch
     * work on our fake TLS block); only _Init_thread_notify calls a wake pointer the skipped CRT init would set
     * (rax = -1 at _guard_dispatch_icall 0x5669F0) -> make it return. RVA confirmed in kh2proj (s156_d5). */
    { static const uint8_t ret1[1] = { 0xC3 }; write_code(0x439FC8, ret1, 1); }

    AddVectoredExceptionHandler(1, veh);
    free(file);
    return 0;
}
