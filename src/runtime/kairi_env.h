// kairi_env.txt (s188, Melty's ask): environment variables from one file beside savagewind.exe, so a launcher (Melty's
// play.py) sets them in one place instead of on the command line. Read by runtime/main.c at the very start of main.
//   KEY=value lines; blank lines and lines starting with # are skipped; spaces / tabs around the key and the value are
//   trimmed (the value also loses a trailing CR / LF); UTF-8, an optional BOM at the start is skipped.
//   A variable already set in the real environment WINS: the file only fills what is unset (launchers and tests override).
//   Only keys that start with SAVAGEWIND_ or SORA_ (letters, digits, _) are taken, so the file can never set PATH,
//   MELTY_NET_DIR and the like; any other line (no '=', other key, empty value, too long, NUL byte) is skipped and counted.
//   The file is capped at KAIRI_ENV_MAX_FILE bytes (bigger: ignored as a whole), a line at KAIRI_ENV_MAX_LINE.
//   The first line for a key wins (a later one counts as "already set").
// Also the "started by Melty" rule (kairi_started_by_melty), shared by the exe (main.c) and sora_mod.dll (sora.c).
// No game includes: runtime/test/kairi_env_test.c covers it on the host.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define KAIRI_ENV_FILE "kairi_env.txt"
#define KAIRI_ENV_MAX_FILE (64 * 1024)
#define KAIRI_ENV_MAX_LINE 4096
#define KAIRI_ENV_MAX_KEY 128

typedef struct {
    int set;      // taken from the file
    int already;  // the key was already set (the real environment, or an earlier line)
    int skipped;  // not taken: no '=', a key outside SAVAGEWIND_ / SORA_, an empty value, too long, a NUL byte
} KairiEnvCounts;

typedef const char *(*KairiGetFn)(const char *key);            // NULL = unset
typedef int (*KairiPutFn)(const char *key, const char *value);  // 0 = done

static inline bool kairi_env_key_ok(const char *k, size_t n) {
    if (n == 0 || n > KAIRI_ENV_MAX_KEY) return false;
    bool sw = n > 11 && !strncmp(k, "SAVAGEWIND_", 11), so = n > 5 && !strncmp(k, "SORA_", 5);
    if (!sw && !so) return false;
    for (size_t i = 0; i < n; i++) {
        char c = k[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

static inline bool kairi_env_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f'; }

// One line (no newline in it; len <= KAIRI_ENV_MAX_LINE)
static inline void kairi_env_line(const char *ln, size_t len, KairiGetFn get, KairiPutFn put, KairiEnvCounts *c) {
    size_t a = 0, b = len;
    while (a < b && kairi_env_space(ln[a])) a++;
    while (b > a && kairi_env_space(ln[b - 1])) b--;
    if (a == b || ln[a] == '#') return;  // blank / comment: not counted
    if (memchr(ln + a, 0, b - a)) { c->skipped++; return; }
    const char *eq = memchr(ln + a, '=', b - a);
    if (!eq) { c->skipped++; return; }
    size_t ka = a, kb = (size_t)(eq - ln), va = kb + 1, vb = b;
    while (kb > ka && kairi_env_space(ln[kb - 1])) kb--;
    while (va < vb && kairi_env_space(ln[va])) va++;
    if (!kairi_env_key_ok(ln + ka, kb - ka) || va == vb) { c->skipped++; return; }  // (an empty value can't be set: _putenv_s removes it)
    char key[KAIRI_ENV_MAX_KEY + 1], val[KAIRI_ENV_MAX_LINE + 1];
    memcpy(key, ln + ka, kb - ka), key[kb - ka] = 0;
    memcpy(val, ln + va, vb - va), val[vb - va] = 0;
    if (get(key)) { c->already++; return; }
    if (put(key, val) == 0) c->set++;
    else c->skipped++;
}

// The whole file's bytes (n <= KAIRI_ENV_MAX_FILE: the caller refuses bigger files)
static inline void kairi_env_apply(const char *buf, size_t n, KairiGetFn get, KairiPutFn put, KairiEnvCounts *c) {
    size_t i = 0;
    if (n >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF) i = 3;
    while (i < n) {
        size_t e = i;
        while (e < n && buf[e] != '\n') e++;
        if (e - i > KAIRI_ENV_MAX_LINE) c->skipped++;
        else kairi_env_line(buf + i, e - i, get, put, c);
        i = e + 1;
    }
}

// Started by Melty: SAVAGEWIND_SORA_MELTY = 0 -> no, = 1 -> yes; otherwise yes only when MELTY_NET_DIR is set and not blank.
static inline bool kairi_started_by_melty(KairiGetFn get) {
    const char *m = get("SAVAGEWIND_SORA_MELTY");
    if (m && !strcmp(m, "0")) return false;
    if (m && !strcmp(m, "1")) return true;
    const char *d = get("MELTY_NET_DIR");
    if (!d) return false;
    for (; *d; d++)
        if (!kairi_env_space(*d)) return true;
    return false;
}
