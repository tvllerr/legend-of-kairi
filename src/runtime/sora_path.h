// Sora-in-BotW, Deck Linux port: where the mod's own files live.
// Every default path in the Sora code is written the PC way, "D:/SoraBotW/<rel>" (data/, run/, play/, recon/probe/).
// sora_path(p) maps that prefix onto sora_root():
//   SORA_ROOT=<dir>  (env, read once; a trailing / or \ is dropped)
//   default          "D:/SoraBotW" on Windows (= the PC exactly as before: p itself is returned, no copy)
//                    "$HOME/D/SoraBotW" elsewhere (the Deck copy keeps the PC layout under /home/deck/D)
// Any other path (an env override like SAVAGEWIND_SORA_MOTION=/x/y.bin, "0", NULL) comes back as it is, except that on
// Linux a "D:\SoraBotW\..." from a PC-style env value is mapped too and backslashes become '/'.
// Mapped results are interned (one copy per distinct path, never freed): the pointer is safe to keep in a static.
// Where it is applied: each "D:/SoraBotW/..." default right after it is picked, plus the loaders every bake path goes
// through (sora.c mot_file_load / hits_file / ffps_path, sora_draw.c read_all = every draw actor's model + pose,
// sora_hud.c face_load), so the FM_DIR / SKT_DATA / SM_DATA tables keep their literals.
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static inline const char *sora_root(void) {
    static char root[512];
    static volatile int done;  // (racy first call is harmless: every thread computes the same string)
    if (done) return root;
    const char *e = getenv("SORA_ROOT");
    if (e && *e) snprintf(root, sizeof root, "%s", e);
    else {
#ifdef _WIN32
        snprintf(root, sizeof root, "%s", "D:/SoraBotW");
#else
        const char *h = getenv("HOME");
        snprintf(root, sizeof root, "%s/D/SoraBotW", h && *h ? h : "/home/deck");
#endif
    }
    size_t n = strlen(root);
    while (n > 1 && (root[n - 1] == '/' || root[n - 1] == '\\')) root[--n] = 0;
    done = 1;
    return root;
}

// p starts with D:/SoraBotW (either slash, either drive case) followed by a separator or the end: the length of that
// prefix (11), else 0.
static inline size_t sora_path_prefix(const char *p) {
    if (!p || (p[0] != 'D' && p[0] != 'd') || p[1] != ':' || (p[2] != '/' && p[2] != '\\')) return 0;
    if (strncmp(p + 3, "SoraBotW", 8)) return 0;
    return (p[11] == 0 || p[11] == '/' || p[11] == '\\') ? 11 : 0;
}

// (s182) p already starts with root (either slash, any case) + a separator or the end: it is mapped already. Without this a
// SORA_ROOT that itself lies under D:/SoraBotW (a test install in D:/SoraBotW/sb/...) was mapped twice -> no Sora.
static inline bool sora_path_under_root(const char *p, const char *root) {
    size_t i = 0;
    for (; root[i]; i++) {
        char a = p[i], b = root[i];
        if (a == '\\') a = '/';
        if (b == '\\') b = '/';
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return false;
    }
    return p[i] == 0 || p[i] == '/' || p[i] == '\\';
}

static inline const char *sora_path(const char *p) {
    if (!p) return p;
    size_t pre = sora_path_prefix(p);
    const char *root = sora_root();
    if (pre && sora_path_under_root(p, root)) return p;
#ifdef _WIN32
    if (!pre || !strcmp(root, "D:/SoraBotW")) return p;  // (PC default: unchanged)
#else
    if (!pre && !strchr(p, '\\')) return p;
#endif
    char buf[1024];
    int n = pre ? snprintf(buf, sizeof buf, "%s%s", root, p + pre) : snprintf(buf, sizeof buf, "%s", p);
    if (n < 0 || n >= (int)sizeof buf) return p;
#ifndef _WIN32
    for (char *c = buf; *c; c++) if (*c == '\\') *c = '/';
#endif
    static struct { char *in, *out; } tab[512];
    static int ntab;
    static volatile char lock;
    while (__atomic_test_and_set(&lock, __ATOMIC_ACQUIRE)) {}
    const char *r = NULL;
    for (int i = 0; i < ntab && !r; i++) if (!strcmp(tab[i].in, p)) r = tab[i].out;
    if (!r) {
        char *o = (char *)malloc((size_t)n + 1), *in = ntab < 512 ? (char *)malloc(strlen(p) + 1) : NULL;
        if (o) memcpy(o, buf, (size_t)n + 1), r = o;
        if (o && in) strcpy(in, p), tab[ntab].in = in, tab[ntab].out = o, ntab++;
        else free(in);  // (table full: this copy just isn't shared)
    }
    __atomic_clear(&lock, __ATOMIC_RELEASE);
    return r ? r : p;
}
