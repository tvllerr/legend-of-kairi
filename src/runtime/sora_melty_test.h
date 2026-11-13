// sora_melty_test.h (s201, DLL side): the mod's own "Melty test mode": test checks + a state file for Melty's test runs.
// While Melty runs a test it writes %LOCALAPPDATA%\Melty\test\legend-of-kairi.json (testId, run, runs, seed, until and the paths
// of the "checks" / "state" / "input" / "played" files); the file is gone when no test runs. Then this mod:
//   - APPENDS (only: Melty's own client appends "in-room" / "in-world" to the same file) one line per check to the `checks` file: "pass <name>" or "fail <name>: <why>" (verdict first), each name once per
//     run, names ^[a-z0-9][a-z0-9-]{0,39}$, at most 12. Never quits the game.
//     (s207) The solo build (SAVAGEWIND_SORA_SOLO=1) reports 8: room-seat is skipped (mt_room_of), and Melty's client never
//     writes in-room / in-world: it reads the test file only at host() / join() / readSettings, none of which a solo copy calls.
//   - writes small JSON to the `state` file about once a second (temp name + rename: a reader never sees half a file):
//     {"run":N,"t":s,"driving":0|1,"pos":[x,y,z],"area":"..","hp":N,"peers":N,"room":0|1,"title":0|1,"world":0|1,"self":"..",
//      "checks":{"name":"pass|fail|wait|skip"}}  (title: BotW's title screen is up; world: in the world this frame)
//   - once Sora drives, walks forward ~2.5 s (up to 4 tries, turning 90 degrees each) and swings once (the live "attack" path),
//     through the mod's own input path only (the driving tick's stick + atk_cmd_queue): no Alt, no keyboard / mouse, no focus.
//     (s204) A new game's Dream Weapon picker (sora_dream.h) is answered first, the same way (mt_pick, mtest_press); what Link
//     is carried while he has the pad (a cutscene) is not counted as walked.
// Activation: only when Melty launched us (net_melty_wanted, the connector's rule) AND the test json exists and parses; it is
// read every 2 s while no test runs (one small read), every 1 s while one runs (the state write's own beat). Outside Melty: one
// getenv at the first frame, then nothing (no file reads, no writes). Paths come only from the json and must be under
// %LOCALAPPDATA%\Melty\test\ (mt_path_under): anything else is refused and nothing is written. The json gone / the test's
// "until" passed / live "set mtest 0" = no more writes. Live "set mtestlog 1" logs each decision.
// Deadlines are seconds from the game start (this DLL's first frame; a second run in the same process: from when its json came).
// Pure core (json parse, path guard, line format, the check table, deadlines, self-drive) above SORA_MTEST_TEST; the game binding
// below it. Host test: runtime/test/melty_test_test.c. GUESS marks the numbers no live run has confirmed yet.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#else
#include <time.h>
#endif

#define MT_PATH 1024
#define MT_MAX_CHECKS 12
#define MT_JSON_MAX 16384
#define MT_WHY_MAX 300
#define MT_MOD "legend-of-kairi"
#define MT_JSON_NAME "legend-of-kairi.json"

// ------------------------------------------------------------------ names, lines
static inline bool mt_name_ok(const char *n) {
    if (!n) return false;
    size_t L = strlen(n);
    if (L < 1 || L > 40) return false;
    if (!((n[0] >= 'a' && n[0] <= 'z') || (n[0] >= '0' && n[0] <= '9'))) return false;
    for (size_t i = 1; i < L; i++)
        if (!((n[i] >= 'a' && n[i] <= 'z') || (n[i] >= '0' && n[i] <= '9') || n[i] == '-')) return false;
    return true;
}
// Names Melty's own client (runtime/melty/melty_net.h TestChecks) already writes into the same checks file: never ours.
static inline bool mt_name_reserved(const char *n) { return n && (!strcmp(n, "in-room") || !strcmp(n, "in-world")); }
static inline bool mt_name_usable(const char *n) { return mt_name_ok(n) && !mt_name_reserved(n); }
// "pass <name>\n" / "fail <name>: <why>\n" (why: one line of printable ASCII, cut to MT_WHY_MAX). -> length, -1 = refused.
static inline int mt_line(char *out, int cap, bool pass, const char *name, const char *why) {
    if (!out || cap < 64 || !mt_name_usable(name)) return -1;
    char w[MT_WHY_MAX + 1];
    int k = 0;
    for (const unsigned char *p = (const unsigned char *)(why && *why ? why : "no reason given"); *p && k < MT_WHY_MAX; p++)
        w[k++] = *p >= 0x20 && *p < 0x7f ? (char)*p : ' ';
    while (k && w[k - 1] == ' ') k--;
    w[k] = 0;
    int n = pass ? snprintf(out, (size_t)cap, "pass %s\n", name) : snprintf(out, (size_t)cap, "fail %s: %s\n", name, k ? w : "no reason given");
    return n > 0 && n < cap ? n : -1;
}

// ------------------------------------------------------------------ paths
static inline char mt_fold(char c) { return c == '/' ? '\\' : (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }
static inline bool mt_sep(char c) { return c == '\\' || c == '/'; }
static inline bool mt_abs(const char *p) {
    return p && ((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':' && mt_sep(p[2]);
}
// path is a file strictly under dir (both "X:\..." absolute; / and \ alike; ASCII case ignored). Refused: anything relative, UNC /
// \\?\ forms, ':' after the drive (streams), control chars, * ? " < > |, empty / "." / ".." components, a component ending in
// '.' or ' ' (Windows drops those: another file), a trailing separator, too long.
static inline bool mt_path_under(const char *path, const char *dir) {
    if (!path || !dir) return false;
    size_t lp = strlen(path), ld = strlen(dir);
    while (ld && mt_sep(dir[ld - 1])) ld--;
    if (lp == 0 || lp >= MT_PATH - 8 || ld < 3 || !mt_abs(path) || !mt_abs(dir)) return false;
    for (size_t i = 0; i < lp; i++) {
        unsigned char c = (unsigned char)path[i];
        if (c < 0x20 || c == 0x7f || strchr("*?\"<>|", c) || (c == ':' && i != 1)) return false;
    }
    for (size_t i = 0; i < ld; i++) {
        unsigned char c = (unsigned char)dir[i];
        if (c < 0x20 || (c == ':' && i != 1)) return false;
    }
    if (lp <= ld + 1) return false;
    for (size_t i = 0; i < ld; i++)
        if (mt_fold(path[i]) != mt_fold(dir[i])) return false;
    if (!mt_sep(path[ld])) return false;
    const char *c = path + 3, *e = path + lp;
    while (c <= e) {
        const char *s = c;
        while (c < e && !mt_sep(*c)) c++;
        size_t n = (size_t)(c - s);
        if (n == 0) return false;  // ("a\\b", a trailing separator)
        if ((n == 1 && s[0] == '.') || (n == 2 && s[0] == '.' && s[1] == '.')) return false;
        if (s[n - 1] == '.' || s[n - 1] == ' ') return false;
        c++;
    }
    return true;
}
static inline bool mt_same_path(const char *a, const char *b) {
    for (;; a++, b++) {
        if (mt_fold(*a) != mt_fold(*b)) return false;
        if (!*a) return true;
    }
}
// "<LOCALAPPDATA>\Melty\test" (false: no / not an absolute LOCALAPPDATA)
static inline bool mt_test_dir(char *out, size_t cap, const char *lad) {
    if (!lad || !mt_abs(lad)) return false;
    size_t n = strlen(lad);
    while (n > 3 && mt_sep(lad[n - 1])) n--;
    int w = snprintf(out, cap, "%.*s%sMelty\\test", (int)n, lad, mt_sep(lad[n - 1]) ? "" : "\\");
    return w > 0 && (size_t)w < cap - 32;
}

// ------------------------------------------------------------------ the test file (flat JSON object)
typedef struct {
    char test_id[96], mod[64], release[48];
    int run, runs;
    long long seed;
    char checks[MT_PATH], state[MT_PATH];
    long long until_ms;  // unix ms (0 = none / unreadable)
} MtTestFile;

typedef struct { const char *p, *e; } MtJ;
static inline void mtj_ws(MtJ *j) { while (j->p < j->e && (*j->p == ' ' || *j->p == '\t' || *j->p == '\r' || *j->p == '\n')) j->p++; }
static inline int mtj_hex4(const char *p) {
    int v = 0;
    for (int i = 0; i < 4; i++) {
        char c = p[i];
        int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d < 0) return -1;  // (s202 review: a bad last digit, "100g", was read as 3096)
        v = v * 16 + d;
    }
    return v;
}
// a string at j->p (the opening quote) -> UTF-8 into out (NULL = skip). false: bad escape / NUL / control char / too long.
static inline bool mtj_str(MtJ *j, char *out, size_t cap) {
    if (j->p >= j->e || *j->p != '"') return false;
    j->p++;
    size_t n = 0;
#define MTJ_PUT(ch) do { if (out) { if (n + 1 >= cap) return false; out[n] = (char)(ch); } n++; } while (0)
    while (j->p < j->e && *j->p != '"') {
        unsigned char c = (unsigned char)*j->p++;
        if (c < 0x20) return false;
        if (c != '\\') { MTJ_PUT(c); continue; }
        if (j->p >= j->e) return false;
        char x = *j->p++;
        if (x == '"' || x == '\\' || x == '/') MTJ_PUT(x);
        else if (x == 'b') MTJ_PUT('\b');
        else if (x == 'f') MTJ_PUT('\f');
        else if (x == 'n') MTJ_PUT('\n');
        else if (x == 'r') MTJ_PUT('\r');
        else if (x == 't') MTJ_PUT('\t');
        else if (x == 'u') {
            if (j->e - j->p < 4) return false;
            int u = mtj_hex4(j->p);
            j->p += 4;
            if (u <= 0) return false;  // (bad hex, or \u0000)
            uint32_t cp = (uint32_t)u;
            if (u >= 0xD800 && u <= 0xDBFF) {
                if (j->e - j->p < 6 || j->p[0] != '\\' || j->p[1] != 'u') return false;
                int lo = mtj_hex4(j->p + 2);
                if (lo < 0xDC00 || lo > 0xDFFF) return false;
                j->p += 6;
                cp = 0x10000 + (((uint32_t)u - 0xD800) << 10) + ((uint32_t)lo - 0xDC00);
            } else if (u >= 0xDC00 && u <= 0xDFFF) return false;
            if (cp < 0x80) MTJ_PUT(cp);
            else if (cp < 0x800) { MTJ_PUT(0xC0 | (cp >> 6)); MTJ_PUT(0x80 | (cp & 0x3F)); }
            else if (cp < 0x10000) { MTJ_PUT(0xE0 | (cp >> 12)); MTJ_PUT(0x80 | ((cp >> 6) & 0x3F)); MTJ_PUT(0x80 | (cp & 0x3F)); }
            else { MTJ_PUT(0xF0 | (cp >> 18)); MTJ_PUT(0x80 | ((cp >> 12) & 0x3F)); MTJ_PUT(0x80 | ((cp >> 6) & 0x3F)); MTJ_PUT(0x80 | (cp & 0x3F)); }
        } else return false;
    }
#undef MTJ_PUT
    if (j->p >= j->e) return false;
    j->p++;
    if (out) out[n] = 0;
    return true;
}
static inline bool mtj_num(MtJ *j, double *v) {
    char t[48];
    size_t n = 0;
    while (j->p + n < j->e && strchr("-+0123456789.eE", j->p[n]) && j->p[n]) {
        if (n + 1 >= sizeof t) return false;
        t[n] = j->p[n], n++;
    }
    if (!n) return false;
    t[n] = 0;
    char *end;
    *v = strtod(t, &end);
    if (end != t + n || !(*v == *v)) return false;
    j->p += n;
    return true;
}
static inline bool mtj_skip(MtJ *j, int depth) {
    mtj_ws(j);
    if (j->p >= j->e || depth > 8) return false;
    char c = *j->p;
    if (c == '"') return mtj_str(j, NULL, 0);
    if (c == '{' || c == '[') {
        char close = c == '{' ? '}' : ']';
        j->p++, mtj_ws(j);
        if (j->p < j->e && *j->p == close) { j->p++; return true; }
        for (;;) {
            if (c == '{') {
                mtj_ws(j);
                if (!mtj_str(j, NULL, 0)) return false;
                mtj_ws(j);
                if (j->p >= j->e || *j->p++ != ':') return false;
            }
            if (!mtj_skip(j, depth + 1)) return false;
            mtj_ws(j);
            if (j->p >= j->e) return false;
            if (*j->p == ',') { j->p++; continue; }
            if (*j->p == close) { j->p++; return true; }
            return false;
        }
    }
    static const char *const lit[] = {"true", "false", "null"};
    for (int i = 0; i < 3; i++) {
        size_t L = strlen(lit[i]);
        if ((size_t)(j->e - j->p) >= L && !memcmp(j->p, lit[i], L)) { j->p += L; return true; }
    }
    double v;
    return mtj_num(j, &v);
}

// days since 1970-01-01 of a civil date (H. Hinnant's days_from_civil)
static inline long long mt_days_civil(long long y, int m, int d) {
    y -= m <= 2;
    long long era = (y >= 0 ? y : y - 399) / 400;
    long long yoe = y - era * 400;
    long long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
static inline bool mt_digits(const char *s, int n, int *v) {
    *v = 0;
    for (int i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return false;
        *v = *v * 10 + (s[i] - '0');
    }
    return true;
}
// "2026-11-11T21:26:42.291Z" (also no fraction, +hh:mm / -hh:mm; no zone = UTC, GUESS) -> unix ms
static inline bool mt_iso_ms(const char *s, long long *ms) {
    int Y, M, D, h, mi, se;
    if (!s || strlen(s) < 19 || !mt_digits(s, 4, &Y) || s[4] != '-' || !mt_digits(s + 5, 2, &M) || s[7] != '-' || !mt_digits(s + 8, 2, &D) ||
        (s[10] != 'T' && s[10] != 't' && s[10] != ' ') || !mt_digits(s + 11, 2, &h) || s[13] != ':' || !mt_digits(s + 14, 2, &mi) ||
        s[16] != ':' || !mt_digits(s + 17, 2, &se))
        return false;
    if (M < 1 || M > 12 || D < 1 || D > 31 || h > 23 || mi > 59 || se > 60) return false;
    const char *p = s + 19;
    int frac = 0;
    if (*p == '.') {
        p++;
        int k = 0;
        while (*p >= '0' && *p <= '9') { if (k < 3) frac = frac * 10 + (*p - '0'); k++, p++; }
        if (!k) return false;
        while (k < 3) frac *= 10, k++;
    }
    long long off = 0;
    if (*p == 'Z' || *p == 'z') p++;
    else if (*p == '+' || *p == '-') {
        int oh, om;
        if (!mt_digits(p + 1, 2, &oh) || p[3] != ':' || !mt_digits(p + 4, 2, &om)) return false;
        off = (oh * 60LL + om) * 60000LL * (*p == '-' ? -1 : 1);
        p += 6;
    }
    if (*p) return false;
    *ms = ((mt_days_civil(Y, M, D) * 86400LL + h * 3600LL + mi * 60LL + se) * 1000LL + frac) - off;
    return true;
}

// The test file -> *tf. 1 = good; 0 = malformed (why says what). Required: testId, run >= 1, checks, state, "mod" (must be
// legend-of-kairi) and a readable "until" (s202 review: a file with no end time, left behind by a test that died, would keep
// the self-drive on for a real player; melty-setup/play.py has the same rule). Unknown keys (input, played, seed, ...) and
// nested values are skipped.
static inline int mt_parse(const char *buf, size_t n, MtTestFile *tf, char *why, size_t wcap) {
    memset(tf, 0, sizeof *tf);
#define MT_BAD(s) do { if (why) snprintf(why, wcap, "%s", s); return 0; } while (0)
    if (!buf || n == 0 || n > MT_JSON_MAX) MT_BAD("empty or too big");
    MtJ j = {buf, buf + n};
    if (n >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF) j.p += 3;
    mtj_ws(&j);
    if (j.p >= j.e || *j.p != '{') MT_BAD("not a JSON object");
    j.p++;
    bool have_run = false, have_mod = false, have_until = false;
    mtj_ws(&j);
    if (j.p < j.e && *j.p == '}') j.p++;
    else
        for (;;) {
            char key[32];
            mtj_ws(&j);
            const char *kp = j.p;
            if (!mtj_str(&j, key, sizeof key)) {
                j.p = kp;
                if (!mtj_str(&j, NULL, 0)) MT_BAD("bad key");
                key[0] = 0;  // (a key longer than ours: not one we read)
            }
            mtj_ws(&j);
            if (j.p >= j.e || *j.p++ != ':') MT_BAD("no ':' after a key");
            mtj_ws(&j);
            char *sdst = NULL;
            size_t scap = 0;
            if (!strcmp(key, "testId")) sdst = tf->test_id, scap = sizeof tf->test_id;
            else if (!strcmp(key, "mod")) sdst = tf->mod, scap = sizeof tf->mod, have_mod = true;
            else if (!strcmp(key, "release")) sdst = tf->release, scap = sizeof tf->release;
            else if (!strcmp(key, "checks")) sdst = tf->checks, scap = sizeof tf->checks;
            else if (!strcmp(key, "state")) sdst = tf->state, scap = sizeof tf->state;
            char ubuf[48];
            if (!strcmp(key, "until")) sdst = ubuf, scap = sizeof ubuf;
            if (sdst) {
                if (j.p >= j.e || *j.p != '"') MT_BAD("a string field is not a string");
                if (!mtj_str(&j, sdst, scap)) MT_BAD("a string field is bad or too long");
                if (sdst == ubuf) {
                    if (!mt_iso_ms(ubuf, &tf->until_ms) || tf->until_ms <= 0) MT_BAD("\"until\" can't be read");
                    have_until = true;
                }
            } else if (!strcmp(key, "run") || !strcmp(key, "runs") || !strcmp(key, "seed")) {
                double v;
                if (!mtj_num(&j, &v)) MT_BAD("run / runs / seed is not a number");
                if (!strcmp(key, "seed")) tf->seed = v >= -9.2e18 && v <= 9.2e18 ? (long long)v : 0;
                else {
                    if (v != floor(v) || v < 0 || v > 1000000) MT_BAD("run / runs is not a whole number");
                    if (key[3]) tf->runs = (int)v;
                    else tf->run = (int)v, have_run = true;
                }
            } else if (!mtj_skip(&j, 0)) MT_BAD("a value could not be read");
            mtj_ws(&j);
            if (j.p >= j.e) MT_BAD("cut off");
            if (*j.p == ',') { j.p++; continue; }
            if (*j.p == '}') { j.p++; break; }
            MT_BAD("no ',' or '}' after a value");
        }
    mtj_ws(&j);
    if (j.p != j.e) MT_BAD("text after the object");
    if (!tf->test_id[0]) MT_BAD("no testId");
    if (!have_run || tf->run < 1) MT_BAD("no run number");
    if (!tf->checks[0] || !tf->state[0]) MT_BAD("no checks / state path");
    if (!have_mod) MT_BAD("no mod");
    if (strcmp(tf->mod, MT_MOD)) MT_BAD("the test is for another mod");
    if (!have_until) MT_BAD("no until");
    return 1;
#undef MT_BAD
}

// ------------------------------------------------------------------ what the game shows this frame (the binding fills it)
typedef struct {
    bool mod_data;        // Sora mode on and its data files loaded (motion bake + hit bake)
    bool in_world;        // BotW's world: Link's body read, not a load
    bool drawn;           // Sora's body was posed this frame and the exe drew a new frame of it
    bool hud;             // the KH2 command menu was handed to the HUD this frame
    int room;             // -1 no room wanted (the solo build / SAVAGEWIND_SORA_NET=0: mt_room_of), 0 not in the room, 1 in the room
    bool driving;         // the driving path ran (Sora drives the player)
    bool have_pos;
    float pos[3];         // Link's feet (= Sora's), BotW metres
    const char *area;     // NULL / "" = not known
    int hp, peers;
    bool attack_began;    // an attack motion started this frame
    bool attack_playing;  // Sora is in an attack motion now
    float attack_clock;   // its clock (KH frames)
    bool diag_up;         // the "Sora's model can't be drawn" notice is up
    bool title;           // BotW's title screen is up (UI screen 0x1, sora_title.h)
    int picker;           // the new game's Dream Weapon picker (sora_dream.h DR.st: MT_PK_*, 0 = not up); only known on a driving frame
    int picker_cur;       // its cursor row
    uint64_t present;     // presented frames so far
} MtObs;

// MtObs.room for room-seat: -1 = no room wanted (s207: the solo build, SAVAGEWIND_SORA_SOLO=1; or SAVAGEWIND_SORA_NET=0), while
// this copy is not in a room: the check is skipped and never written. 1 = seated in the Melty room (in_room: connected, a
// seat), 0 = not (yet). The solo build never joins, so a solo run reports no room check of ours.
static inline int mt_room_of(bool solo, const char *net_env, bool melty, bool in_room) {
    if ((solo || (net_env && atoi(net_env) == 0)) && !melty) return -1;
    return melty && in_room ? 1 : 0;
}

// ------------------------------------------------------------------ the core
enum { MT_WAIT, MT_PASS, MT_FAIL, MT_SKIP };
enum { SD_IDLE, SD_SETTLE, SD_WALK, SD_GAP, SD_ATTACK, SD_SWING, SD_DONE };
typedef struct Mt Mt;
// 1 pass now, 0 not yet, -1 skip (never written), -2 fail now (why in m->why_now)
typedef int (*MtCond)(Mt *m, const MtObs *o, double t);
typedef struct { const char *name; MtCond cond; double deadline_s; const char *why; bool world; } MtCheckDef;  // world: needs BotW's world

typedef struct {
    int (*read)(void *ud, const char *path, char *buf, int cap);  // bytes read, -1 = missing / unreadable
    int (*append)(void *ud, const char *path, const char *text);  // 0 = done
    int (*replace)(void *ud, const char *path, const char *text);  // 0 = done (write a temp file, rename over)
    void (*log)(void *ud, const char *line);
    void *ud;
} MtIo;

#define MT_WALK_S 2.5       // seconds of driven walking per try (GUESS: Sora's run is ~5 m/s, 2 m takes < 1 s)
#define MT_WALK_TRIES 4
#define MT_MOVE_M 2.0f
#define MT_SWING_WAIT_S 6.0 // driven seconds for the queued press to start an attack
#define MT_SWING_TRIES 3
#define MT_SWING_CLOCK 8.0f // KH frames into the attack motion = it played
#define MT_PICK_GAP_S 0.5   // seconds between two presses into the weapon pick (its "decide" needs a frame; GUESS)
#define MT_PICK_MAX 24      // presses into the weapon pick before the self-drive gives it up (a pick with the give-up step takes 7)
// the weapon pick's states (= sora_dream.h DR_*) and the pad bits the self-drive presses into it (= sora.c BTN_ATTACK, D-pad up / down)
enum { MT_PK_OFF, MT_PK_LIST, MT_PK_ASK, MT_PK_GOT, MT_PK_GLIST, MT_PK_GASK };
#define MT_BTN_A (1ull << 0)
#define MT_BTN_UP (1ull << 13)
#define MT_BTN_DOWN (1ull << 15)
#define MT_FREEZE_S 15.0    // a gap this long between two frames = the game froze (GUESS: loads keep the frames coming)
#define MT_STABLE_S 300.0

struct Mt {
    int on, verbose;  // live "set mtest" (default 1), "set mtestlog"
    bool wanted;      // Melty launched us and the test folder is known
    char dir[MT_PATH], json[MT_PATH];
    MtIo io;
    const MtCheckDef *defs;
    int ndefs;
    // the session (one test run)
    bool active, had, over;
    MtTestFile tf;
    double base, next_probe, next_state, last_t;
    int st[MT_MAX_CHECKS];
    bool written[MT_MAX_CHECKS], wfail_logged[MT_MAX_CHECKS];
    char why[MT_MAX_CHECKS][MT_WHY_MAX + 1];
    char why_now[MT_WHY_MAX + 1];
    bool ran_out, state_fail_logged, bad_logged, refused_logged;
    char bad_last[96];
    uint32_t last_hash;
    // what was seen (this session)
    int n_world, n_drawn, n_hud, n_room, drive_run, drive_best;
    double world_since, present_t, max_gap, gap_at, diag_at, title_t;  // (title_t: last time the title was seen, -1 never)
    bool title_now;
    uint64_t present_last;
    float last_pos[3];
    // the self-drive
    int sd, sd_try, atk_try;
    double sd_t, sd_walked, atk_driven;
    int sd_next;
    float sd_p0[3], sd_best;
    bool moved, atk_began, swung, want_attack;
    uint64_t want_press;  // pad bits for the next driven frame (the weapon pick's answers); the caller takes + clears them
    // the new game's weapon pick (s204) + the walk's "the pad went to Link" gap
    bool pick_up, pick_stuck, sd_lost;
    int pick_n;
    double pick_t;
    float sd_last[3];
    float stick[2];
    bool stick_on;
};

static void mt_log(Mt *m, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void mt_log(Mt *m, const char *fmt, ...) {
    if (!m->io.log) return;
    char b[MT_PATH * 2 + 256];
    int n = snprintf(b, sizeof b, "[mtest] ");
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b + n, sizeof b - (size_t)n, fmt, ap);
    va_end(ap);
    m->io.log(m->io.ud, b);
}
static inline double mt_dist(const float a[3], const float b[3]) {
    double dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return sqrt(dx * dx + dy * dy + dz * dz);
}

// ---- the check table (GUESS: every deadline; the first live test run should tune them). Needs a playable scene (Sora drives):
// sora-drives, sora-moves, attack-swings. The rest also pass in the new game's opening (Link's pad, Sora as his puppet).
static int mtc_loaded(Mt *m, const MtObs *o, double t) { (void)m, (void)t; return o->mod_data; }
static int mtc_world(Mt *m, const MtObs *o, double t) { (void)o, (void)t; return m->n_world >= 10; }
static int mtc_drawn(Mt *m, const MtObs *o, double t) { (void)o, (void)t; return m->n_drawn >= 60; }
static int mtc_hud(Mt *m, const MtObs *o, double t) { (void)o, (void)t; return m->n_hud >= 30; }
static int mtc_room(Mt *m, const MtObs *o, double t) { (void)t; return o->room < 0 ? -1 : m->n_room >= 1; }
static int mtc_drives(Mt *m, const MtObs *o, double t) { (void)o, (void)t; return m->drive_best >= 30; }
static int mtc_moves(Mt *m, const MtObs *o, double t) { (void)o, (void)t; return m->moved; }
static int mtc_swing(Mt *m, const MtObs *o, double t) { (void)o, (void)t; return m->swung; }
static int mtc_stable(Mt *m, const MtObs *o, double t) {
    (void)o;
    if (m->diag_at >= 0) {
        snprintf(m->why_now, sizeof m->why_now, "the \"Sora's model can't be drawn\" notice came up at t=%.0f s", m->diag_at);
        return -2;
    }
    if (m->max_gap >= MT_FREEZE_S) {
        snprintf(m->why_now, sizeof m->why_now, "the game froze %.0f s at t=%.0f s", m->max_gap, m->gap_at);
        return -2;
    }
    return m->world_since >= 0 && t - m->world_since >= MT_STABLE_S && t - m->present_t <= 2.0;
}
// (Melty's own client writes "in-room" and "in-world" into the same file: ours are "world-up" and "room-seat", mt_name_reserved)
static const MtCheckDef MT_CHECKS[] = {
    {"mod-loaded", mtc_loaded, 90, "Sora mode is off or its data files (sora_motion.bin / sora_hits.bin) were not found", false},
    {"world-up", mtc_world, 300, "BotW's world never came up (no Link body outside a load)", true},
    {"sora-drawn", mtc_drawn, 330, "Sora's body was not drawn for 60 frames", true},
    {"hud-up", mtc_hud, 330, "the KH2 command menu was not drawn for 30 frames", true},
    {"room-seat", mtc_room, 150, "the mod's packets have no seat in the Melty room (Melty Net did not start, or no seat)", false},
    {"sora-drives", mtc_drives, 420, "Sora never drove the player for 30 frames in a row (Link kept the pad: a cutscene / the opening?)", true},
    {"sora-moves", mtc_moves, 480, "Sora did not move 2 m under the test's own stick", true},
    {"attack-swings", mtc_swing, 540, "the test's own attack press did not play an attack", true},
    {"stable-5min", mtc_stable, 900, "not 5 minutes in the world with frames still presented", true},
};
#define MT_NCHECKS ((int)(sizeof MT_CHECKS / sizeof MT_CHECKS[0]))

static const char *mt_st_name(int s) { return s == MT_PASS ? "pass" : s == MT_FAIL ? "fail" : s == MT_SKIP ? "skip" : "wait"; }
static const char *mt_sd_name(int s) {
    static const char *const n[] = {"idle", "settle", "walk", "gap", "attack", "swing", "done"};
    return s >= 0 && s <= SD_DONE ? n[s] : "?";
}

// dir = "<LOCALAPPDATA>\Melty\test" (NULL / "" = never active). defs NULL = MT_CHECKS.
static void mt_init(Mt *m, const char *dir, MtIo io, const MtCheckDef *defs, int ndefs) {
    memset(m, 0, sizeof *m);
    m->on = 1, m->io = io;
    m->defs = defs ? defs : MT_CHECKS, m->ndefs = defs ? ndefs : MT_NCHECKS;
    if (m->ndefs > MT_MAX_CHECKS) m->ndefs = MT_MAX_CHECKS;
    if (!dir || !*dir || strlen(dir) > MT_PATH - 64) return;
    snprintf(m->dir, sizeof m->dir, "%s", dir);
    snprintf(m->json, sizeof m->json, "%s\\%s", dir, MT_JSON_NAME);
    m->wanted = true;
}

static void mt_session_reset(Mt *m, const MtTestFile *tf, double t) {
    m->tf = *tf;
    m->base = m->had ? t : 0;  // (the first run: from the game start; a later one in the same process: from now)
    m->had = true, m->over = false;
    for (int i = 0; i < m->ndefs; i++) {
        m->st[i] = mt_name_usable(m->defs[i].name) ? MT_WAIT : MT_SKIP;
        m->written[i] = m->wfail_logged[i] = false, m->why[i][0] = 0;
        if (m->st[i] == MT_SKIP)
            mt_log(m, "check \"%s\": %s, never written\n", m->defs[i].name ? m->defs[i].name : "(null)",
                   mt_name_reserved(m->defs[i].name) ? "a name Melty's own client writes" : "not a valid name");
    }
    m->ran_out = m->state_fail_logged = false;
    m->n_world = m->n_drawn = m->n_hud = m->n_room = m->drive_run = m->drive_best = 0;
    m->world_since = m->diag_at = m->title_t = -1, m->title_now = false, m->present_t = t, m->max_gap = 0, m->gap_at = 0, m->present_last = 0;
    memset(m->last_pos, 0, sizeof m->last_pos);
    m->sd = SD_IDLE, m->sd_try = m->atk_try = 0, m->sd_t = t, m->sd_walked = m->atk_driven = 0, m->sd_best = 0;
    m->moved = m->atk_began = m->swung = m->want_attack = false, m->stick_on = false;
    m->want_press = 0, m->pick_up = m->pick_stuck = m->sd_lost = false, m->pick_n = 0, m->pick_t = t;
    m->next_state = t;
}

static void mt_stop(Mt *m, const char *why) {
    if (m->active) mt_log(m, "test mode off: %s (no more writes)\n", why);
    m->active = false, m->stick_on = false, m->want_attack = false, m->want_press = 0;
}

// write one verdict line (once): false = the append failed (retried at the next probe)
static bool mt_write(Mt *m, int i) {
    if (m->written[i] || (m->st[i] != MT_PASS && m->st[i] != MT_FAIL)) return true;
    char line[MT_WHY_MAX + 80];
    if (mt_line(line, (int)sizeof line, m->st[i] == MT_PASS, m->defs[i].name, m->why[i]) < 0) return true;
    if (m->io.append && m->io.append(m->io.ud, m->tf.checks, line) == 0) {
        m->written[i] = true;
        mt_log(m, "%s", line);  // (a verdict: always logged, at most 12 a run)
        return true;
    }
    if (!m->wfail_logged[i]) m->wfail_logged[i] = true, mt_log(m, "could not append to %s (check %s): retrying\n", m->tf.checks, m->defs[i].name);
    return false;
}
static void mt_verdict(Mt *m, int i, int st, const char *why) {
    m->st[i] = st;
    snprintf(m->why[i], sizeof m->why[i], "%s", why ? why : "");
    mt_write(m, i);
}

static uint32_t mt_hash(const char *b, int n) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < n; i++) h = (h ^ (uint8_t)b[i]) * 16777619u;
    return h;
}

// read + check the test file; turns the session on / off. Only called while wanted and on.
static void mt_probe(Mt *m, double t, long long wall_ms) {
    static char buf[MT_JSON_MAX + 1];
    int n = m->io.read ? m->io.read(m->io.ud, m->json, buf, MT_JSON_MAX + 1) : -1;
    if (n < 0) {
        if (m->active) mt_stop(m, "the test file is gone");
        else if (m->verbose) mt_log(m, "probe: no %s\n", m->json);
        m->bad_logged = m->refused_logged = false, m->last_hash = 0;
        return;
    }
    uint32_t h = mt_hash(buf, n);
    bool changed = h != m->last_hash;
    m->last_hash = h;
    MtTestFile tf;
    char why[96] = "";
    if (n > MT_JSON_MAX || !mt_parse(buf, (size_t)n, &tf, why, sizeof why)) {
        if (n > MT_JSON_MAX) snprintf(why, sizeof why, "bigger than %d bytes", MT_JSON_MAX);
        if (changed || !m->bad_logged || m->verbose)  // (a half-written file is read again next probe; an active run goes on)
            mt_log(m, "the test file can't be read (%s)%s\n", why, m->active ? ": the run goes on with the last good one" : ": test mode stays off");
        m->bad_logged = true;
        return;
    }
    m->bad_logged = false;
    char stmp[MT_PATH + 8];  // (s202 review: the state file's temp name, mt_io_replace, must not be the checks / test file)
    snprintf(stmp, sizeof stmp, "%s.tmp", tf.state);
    const char *bad = !mt_path_under(tf.checks, m->dir) ? "checks" : !mt_path_under(tf.state, m->dir) ? "state"
                    : mt_same_path(tf.checks, tf.state) || mt_same_path(tf.checks, m->json) || mt_same_path(tf.state, m->json)
                      || mt_same_path(tf.checks, stmp) || mt_same_path(m->json, stmp) ? "checks / state (the same file)"
                    : NULL;
    if (bad) {
        if (m->active) mt_stop(m, "a path in the test file is outside the test folder");
        if (changed || !m->refused_logged)
            mt_log(m, "REFUSED: the %s path is not a file under %s: nothing is written (checks \"%s\", state \"%s\")\n", bad, m->dir, tf.checks, tf.state);
        m->refused_logged = true;
        return;
    }
    m->refused_logged = false;
    bool same = m->had && !strcmp(tf.test_id, m->tf.test_id) && tf.run == m->tf.run;
    if (tf.until_ms && wall_ms >= tf.until_ms) {
        if (m->active) mt_stop(m, "the test's time is over (until)");
        else if (m->verbose) mt_log(m, "probe: the test's time is over (until), staying off\n");
        if (same) m->over = true;
        return;
    }
    if (same && m->over) return;
    if (same) {
        m->tf = tf;  // (same run: the checks / state paths may only have been rewritten the same)
        if (!m->active) {
            int nw = 0;
            for (int i = 0; i < m->ndefs; i++) nw += m->written[i];
            m->active = true, m->last_t = t;  // (no freeze counted for the time it was off)
            mt_log(m, "test mode on again (run %d, the same test): %d check line(s) already written stay written\n", tf.run, nw);
        }
        return;
    }
    mt_session_reset(m, &tf, t);
    m->active = true;
    int left = tf.until_ms ? (int)((tf.until_ms - wall_ms) / 1000) : -1;
    mt_log(m, "test mode on: test %s run %d/%d release %s at t=%.0f s, %d checks -> %s, state -> %s\n", tf.test_id, tf.run, tf.runs,
           tf.release[0] ? tf.release : "?", t, m->ndefs, tf.checks, tf.state);
    if (left < 0) mt_log(m, "no \"until\" in the test file: each check waits for its own deadline\n");
    else {
        mt_log(m, "run time left: %d s (checks still waiting 3 s before its end are failed then)\n", left);
        for (int i = 0; i < m->ndefs; i++)
            if (m->defs[i].deadline_s > left + (t - m->base)) mt_log(m, "check %s: its deadline %.0f s is after the run's end\n", m->defs[i].name, m->defs[i].deadline_s);
    }
}

static void mt_observe(Mt *m, const MtObs *o, double t) {
    double gap = m->last_t > 0 ? t - m->last_t : 0;
    if (gap > m->max_gap && m->world_since >= 0) m->max_gap = gap, m->gap_at = m->last_t - m->base;
    if (o->present != m->present_last) m->present_last = o->present, m->present_t = t;
    if (o->in_world) {
        m->n_world++;
        if (m->world_since < 0) m->world_since = t;
    }
    m->n_drawn += o->drawn, m->n_hud += o->hud, m->n_room += o->room > 0;
    m->drive_run = o->driving ? m->drive_run + 1 : 0;
    if (m->drive_run > m->drive_best) m->drive_best = m->drive_run;
    if (o->diag_up && m->diag_at < 0 && m->world_since >= 0) m->diag_at = t - m->base;
    m->title_now = o->title;
    if (o->title) m->title_t = t;
    if (o->have_pos) memcpy(m->last_pos, o->pos, sizeof m->last_pos);
}

static void mt_sd_go(Mt *m, int s, double t) {
    if (m->verbose) mt_log(m, "self-drive: %s -> %s at t=%.1f s\n", mt_sd_name(m->sd), mt_sd_name(s), t - m->base);
    m->sd = s, m->sd_t = t;
}
// (s204) A new game opens KH2's Dream Weapon picker (sora_dream.h) once Sora stands; while it is up it owns the pad and the
// stick, so a walk moves nothing and an attack press only moves the picker (test bc7266ba: list -> ask -> list -> ask, "played
// no attack"). The self-drive answers it first, one press every MT_PICK_GAP_S: A on the list (the cursor's weapon), the cursor
// to "You bet it is!" then A, A on the "Received ..." line; with the give-up step (live "set dreamgu 1") A on that list, the
// cursor to "Not really." then A. The picker's state is only known on a driving frame (dream_tick runs in the driving path).
// -> true: the picker is up and this frame is its own (no walk, no attack). After MT_PICK_MAX presses it is given up: the
// self-drive goes on as before and the failed checks' reasons name the picker.
static bool mt_pick(Mt *m, const MtObs *o, double t) {
    if (!o->driving) return false;
    if (o->picker == MT_PK_OFF) {
        if (m->pick_up) m->pick_up = false, mt_log(m, "self-drive: the weapon pick is closed (%d presses)\n", m->pick_n);
        return false;
    }
    if (!m->pick_up) {
        m->pick_up = true, m->pick_t = t;
        mt_log(m, "self-drive: the new game's weapon pick is open: answering it first\n");
    }
    if (m->pick_stuck) return false;
    if (t - m->pick_t < MT_PICK_GAP_S) return true;
    if (m->pick_n >= MT_PICK_MAX) {
        m->pick_stuck = true;
        mt_log(m, "self-drive: the weapon pick is still open after %d presses (state %d, cursor %d): going on without it\n", m->pick_n, o->picker, o->picker_cur);
        return false;
    }
    uint64_t b = MT_BTN_A;
    if (o->picker == MT_PK_ASK && o->picker_cur != 1) b = MT_BTN_DOWN;        // (rows: "Not really." / "You bet it is!")
    else if (o->picker == MT_PK_GASK && o->picker_cur != 0) b = MT_BTN_UP;    // (give-up: "Not really." gives it up)
    m->want_press = b, m->pick_n++, m->pick_t = t;
    if (m->verbose) mt_log(m, "self-drive: weapon pick press %d: %s (state %d, cursor %d)\n", m->pick_n, b == MT_BTN_A ? "A" : b == MT_BTN_UP ? "up" : "down", o->picker, o->picker_cur);
    return true;
}
// the self-drive: the weapon pick first, then walk (up to MT_WALK_TRIES directions), then one attack (up to MT_SWING_TRIES presses)
static void mt_selfdrive(Mt *m, const MtObs *o, double t, double dt) {
    m->stick_on = false;
    if (mt_pick(m, o, t)) return;
    switch (m->sd) {
    case SD_IDLE:
        if (m->drive_best >= 30) mt_sd_go(m, SD_SETTLE, t);
        break;
    case SD_SETTLE:
        if (t - m->sd_t >= 1.0 && o->driving && o->have_pos) {
            memcpy(m->sd_p0, o->pos, sizeof m->sd_p0), m->sd_walked = 0, m->sd_best = 0;
            memcpy(m->sd_last, o->pos, sizeof m->sd_last), m->sd_lost = false;
            mt_sd_go(m, SD_WALK, t);
        }
        break;
    case SD_WALK: {
        static const float dirs[MT_WALK_TRIES][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};  // (forward, right, back, left)
        m->stick[0] = dirs[m->sd_try % MT_WALK_TRIES][0], m->stick[1] = dirs[m->sd_try % MT_WALK_TRIES][1], m->stick_on = true;
        if (o->driving) m->sd_walked += dt;
        // (s204, test bc7266ba: the wake-up cutscene took the pad mid-walk and put Link on the bed 3.28 m away = a false
        // "moved") what he was carried while Link had the pad is not the stick's distance: the start point moves with it
        if (!o->driving) m->sd_lost = true;
        else if (o->have_pos) {
            if (m->sd_lost) {
                m->sd_lost = false;
                if (m->verbose) mt_log(m, "self-drive: carried %.2f m while Link had the pad (not counted)\n", mt_dist(o->pos, m->sd_last));
                for (int i = 0; i < 3; i++) m->sd_p0[i] += o->pos[i] - m->sd_last[i];
            }
            memcpy(m->sd_last, o->pos, sizeof m->sd_last);
        }
        if (o->driving && o->have_pos) {
            float d = (float)mt_dist(o->pos, m->sd_p0);
            if (d > m->sd_best) m->sd_best = d;
        }
        if (m->sd_best >= MT_MOVE_M) {
            m->moved = true;
            mt_log(m, "self-drive: moved %.2f m (try %d)\n", m->sd_best, m->sd_try + 1);
            m->sd_next = SD_ATTACK, m->stick_on = false, mt_sd_go(m, SD_GAP, t);
        } else if (m->sd_walked >= MT_WALK_S) {
            mt_log(m, "self-drive: try %d moved only %.2f m\n", m->sd_try + 1, m->sd_best);
            m->sd_try++, m->stick_on = false;
            m->sd_next = m->sd_try < MT_WALK_TRIES ? SD_SETTLE : SD_ATTACK;
            mt_sd_go(m, SD_GAP, t);
        }
        break;
    }
    case SD_GAP:
        if (t - m->sd_t >= 0.8) mt_sd_go(m, m->sd_next, t);
        break;
    case SD_ATTACK:
        if (o->driving) {
            m->want_attack = true, m->atk_try++, m->atk_driven = 0, m->atk_began = false;
            mt_log(m, "self-drive: attack press %d (the live \"attack\" path)\n", m->atk_try);
            mt_sd_go(m, SD_SWING, t);
        }
        break;
    case SD_SWING:
        if (o->driving) m->atk_driven += dt;
        if (o->attack_began) m->atk_began = true;
        if (m->atk_began && o->attack_playing && o->attack_clock >= MT_SWING_CLOCK) {
            m->swung = true;
            mt_log(m, "self-drive: the attack played (clock %.0f)\n", o->attack_clock);
            mt_sd_go(m, SD_DONE, t);
        } else if (m->atk_driven >= MT_SWING_WAIT_S) {
            mt_log(m, "self-drive: press %d played no attack (%s)\n", m->atk_try, m->atk_began ? "started, cut short" : "none started");
            mt_sd_go(m, m->atk_try < MT_SWING_TRIES ? SD_ATTACK : SD_DONE, t);
        }
        break;
    default: break;
    }
}

static void mt_json_str(char **w, char *end, const char *s) {
    if (*w >= end) return;
    *(*w)++ = '"';
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p && *w < end - 8; p++) {
        if (*p == '"' || *p == '\\') *(*w)++ = '\\', *(*w)++ = (char)*p;
        else *(*w)++ = *p < 0x20 || *p >= 0x7f ? '?' : (char)*p;
    }
    if (*w < end) *(*w)++ = '"';
}
// the state JSON -> out; -> length (-1 = did not fit)
static int mt_state_json(Mt *m, const MtObs *o, double t, char *out, int cap) {
    char *w = out, *end = out + cap - 2;
#define MT_ADD(...) do { if (w < end) { int k_ = snprintf(w, (size_t)(end - w), __VA_ARGS__); if (k_ < 0 || k_ >= end - w) return -1; w += k_; } } while (0)
    const float *p = o->have_pos ? o->pos : m->last_pos;
    MT_ADD("{\"run\":%d,\"t\":%.1f,\"driving\":%d,\"pos\":[%.1f,%.1f,%.1f],\"area\":", m->tf.run, t - m->base, o->driving ? 1 : 0, p[0], p[1], p[2]);
    mt_json_str(&w, end, o->area && *o->area ? o->area : "-");
    MT_ADD(",\"hp\":%d,\"peers\":%d,\"room\":%d,\"title\":%d,\"world\":%d,\"self\":\"%s\",\"checks\":{", o->hp, o->peers, o->room > 0,
           o->title ? 1 : 0, o->in_world ? 1 : 0, mt_sd_name(m->sd));
    for (int i = 0, k = 0; i < m->ndefs; i++) {
        if (!mt_name_usable(m->defs[i].name)) continue;
        MT_ADD("%s\"%s\":\"%s\"", k++ ? "," : "", m->defs[i].name, mt_st_name(m->st[i]));
    }
    MT_ADD("}}\n");
#undef MT_ADD
    *w = 0;
    return (int)(w - out);
}

// Once a game frame. t = seconds since the game start (monotonic), wall_ms = unix ms (the test's "until"), o = this frame
// (only read while active). Out: m->stick_on / m->stick (the self-drive's stick, -1..1), m->want_attack (the caller queues one
// attack and clears it).
static void mt_tick(Mt *m, double t, long long wall_ms, const MtObs *o) {
    if (!m->wanted) return;
    if (!m->on) {
        if (m->active) mt_stop(m, "set mtest 0");
        m->last_t = t;
        return;
    }
    if (t >= m->next_probe) {
        m->next_probe = t + (m->active ? 1.0 : 2.0);
        mt_probe(m, t, wall_ms);
        if (m->active)
            for (int i = 0; i < m->ndefs; i++) mt_write(m, i);  // (retry an append that failed)
    }
    if (!m->active) { m->last_t = t; return; }
    double dt = m->last_t > 0 ? t - m->last_t : 0;
    if (dt > 0.1) dt = 0.1;
    mt_observe(m, o, t);
    m->last_t = t;
    double ts = t - m->base;
    bool run_out = m->tf.until_ms && wall_ms >= m->tf.until_ms - 3000;
    for (int i = 0; i < m->ndefs; i++) {
        if (m->st[i] != MT_WAIT) continue;
        m->why_now[0] = 0;
        int r = m->defs[i].cond(m, o, t);
        if (r == 1) mt_verdict(m, i, MT_PASS, NULL);
        else if (r == -1) {
            m->st[i] = MT_SKIP;
            mt_log(m, "check %s: skipped (not wanted in this launch), never written\n", m->defs[i].name);
        } else if (r == -2) mt_verdict(m, i, MT_FAIL, m->why_now);
        else if (ts >= m->defs[i].deadline_s || run_out) {
            char w[MT_WHY_MAX + 1], where[96] = "";
            if (m->defs[i].world && m->title_now)  // (s201 test run 2 sat on the title the whole run: say so)
                snprintf(where, sizeof where, m->world_since < 0 ? ", still on BotW's title screen after %.0f s, the world never came" : ", on BotW's title screen at %.0f s", ts);
            else if (m->defs[i].world && m->world_since < 0 && m->title_t >= 0)
                snprintf(where, sizeof where, ", the world never came (title screen last seen at %.0f s)", m->title_t - m->base);
            else if (m->defs[i].world && m->world_since < 0)
                snprintf(where, sizeof where, ", the world never came");
            else if (m->defs[i].world && m->pick_up)  // (s204: it owns the pad + the stick while up)
                snprintf(where, sizeof where, ", the new game's weapon pick was open (%d presses of the test did not close it)", m->pick_n);
            snprintf(w, sizeof w, "%s (%s at t=%.0f s%s)", m->defs[i].why, run_out && ts < m->defs[i].deadline_s ? "the test's time ran out" : "deadline", ts, where);
            mt_verdict(m, i, MT_FAIL, w);
        }
    }
    if (run_out && !m->ran_out) m->ran_out = true, mt_log(m, "the test's time is nearly over: every waiting check failed\n");
    mt_selfdrive(m, o, t, dt);
    if (t >= m->next_state) {
        m->next_state = t + 1.0;
        char js[2048];
        int n = mt_state_json(m, o, t, js, (int)sizeof js);
        if (n > 0 && m->io.replace) {
            if (m->io.replace(m->io.ud, m->tf.state, js) != 0) {
                if (!m->state_fail_logged) m->state_fail_logged = true, mt_log(m, "could not write %s: retrying each second\n", m->tf.state);
            } else if (m->verbose) mt_log(m, "state: %s", js);
        }
    }
}

// live "set mtest 0|1" / "set mtestlog 0|1"
static bool mt_set_key(Mt *m, const char *key, int v) {
    if (!strcmp(key, "mtest")) {
        m->on = v != 0;
        if (m->on) m->next_probe = 0;  // (look for the test file at once)
        return true;
    }
    if (!strcmp(key, "mtestlog")) { m->verbose = v != 0; return true; }
    return false;
}

// ------------------------------------------------------------------ the real files (the DLL; the host test runs them too)
#ifdef _WIN32
static bool mt_wide(const char *u8, wchar_t *w, int cap) {
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, u8, -1, w, cap);
    return n > 0;
}
#endif
static int mt_io_read(void *ud, const char *path, char *buf, int cap) {
    (void)ud;
    FILE *f = NULL;
#ifdef _WIN32
    wchar_t w[MT_PATH];
    if (!mt_wide(path, w, MT_PATH)) return -1;
    f = _wfopen(w, L"rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f) return -1;
    int n = (int)fread(buf, 1, (size_t)cap, f);
    fclose(f);
    return n;
}
// APPEND only (never truncate / rewrite / rename: Melty's own client appends to the same checks file in this process): one
// line = one write. Windows: FILE_APPEND_DATA, so the OS puts each write at the end even with another writer's handle open.
static int mt_io_append(void *ud, const char *path, const char *text) {
    (void)ud;
    size_t L = strlen(text);
#ifdef _WIN32
    wchar_t w[MT_PATH];
    if (!mt_wide(path, w, MT_PATH) || L > 0x7fffffff) return -1;
    HANDLE h = CreateFileW(w, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return -1;
    DWORD put = 0;
    bool ok = WriteFile(h, text, (DWORD)L, &put, NULL) && put == (DWORD)L;
    ok = CloseHandle(h) && ok;
    return ok ? 0 : -1;
#else
    FILE *f = fopen(path, "ab");  // (O_APPEND: each write at the end)
    if (!f) return -1;
    setvbuf(f, NULL, _IOFBF, L + 1);  // (the whole line in one write at fclose)
    bool ok = fwrite(text, 1, L, f) == L;
    ok = fclose(f) == 0 && ok;
    return ok ? 0 : -1;
#endif
}
static int mt_io_replace(void *ud, const char *path, const char *text) {
    (void)ud;
    char tmp[MT_PATH + 8];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    size_t L = strlen(text);
#ifdef _WIN32
    wchar_t w[MT_PATH], wt[MT_PATH + 8];
    if (!mt_wide(path, w, MT_PATH) || !mt_wide(tmp, wt, MT_PATH + 8)) return -1;
    FILE *f = _wfopen(wt, L"wb");
    if (!f) return -1;
    bool ok = fwrite(text, 1, L, f) == L;
    ok = fclose(f) == 0 && ok;
    if (ok && MoveFileExW(wt, w, MOVEFILE_REPLACE_EXISTING)) return 0;
    DeleteFileW(wt);
    return -1;
#else
    FILE *f = fopen(tmp, "wb");
    if (!f) return -1;
    bool ok = fwrite(text, 1, L, f) == L;
    ok = fclose(f) == 0 && ok;
    if (ok && rename(tmp, path) == 0) return 0;
    remove(tmp);
    return -1;
#endif
}
// LOCALAPPDATA as UTF-8 (false: not set)
static bool mt_localappdata(char *out, size_t cap) {
#ifdef _WIN32
    const wchar_t *w = _wgetenv(L"LOCALAPPDATA");
    if (!w || !*w) return false;
    return WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)cap, NULL, NULL) > 0;
#else
    const char *e = getenv("LOCALAPPDATA");  // (the Deck: not set = test mode never on)
    if (!e || !*e) return false;
    snprintf(out, cap, "%s", e);
    return true;
#endif
}
static long long mt_wall_ms(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    unsigned long long v = ((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return (long long)(v / 10000ULL) - 11644473600000LL;
#else
    return (long long)time(NULL) * 1000LL;
#endif
}

#ifndef SORA_MTEST_TEST
// ------------------------------------------------------------------ the game side (sora.c: LOG, net_melty_wanted, NET, S, ...)
static void mtb_log(void *ud, const char *line) { (void)ud; LOG("%s", line); }
static struct {
    bool inited;
    uint64_t t0_ms, drawn_last;
    Mt m;
} MTB;
static MtObs g_mto;  // this frame's hooks (sora.c sets in_world / driving / hud / attack_began; mtest_frame clears them)

static int g_mt_pend_on = -1, g_mt_pend_log = -1;  // (a live key before the first mtest_frame: applied at its init)
static bool mtest_set_key(const char *key, int v) {
    if (strcmp(key, "mtest") && strcmp(key, "mtestlog")) return false;
    if (!MTB.inited) {
        if (!strcmp(key, "mtest")) g_mt_pend_on = v != 0;
        else g_mt_pend_log = v != 0;
        return true;
    }
    mt_set_key(&MTB.m, key, v);
    LOG("[mtest] live: %s %d%s\n", key, v, MTB.m.wanted ? "" : " (not launched by Melty: test mode never runs here)");
    return true;
}
// (s209 review 2) Melty's autopilot took its first command (sora.c np_took_first): from then on the stick and the attack
// presses are the autopilot's for the rest of the session; the self-drive stands down (its checks may then not complete).
static bool g_mt_yield;
static void mtest_yield(const char *why) {
    if (g_mt_yield) return;
    g_mt_yield = true;
    MTB.m.stick_on = false, MTB.m.want_attack = false, MTB.m.want_press = 0;
    LOG("[mtest] self-drive stands down for this session: %s (the stick and attack presses are the autopilot's now)\n", why);
}
// the self-drive's stick over the pad's (driving tick, before KH2's dead zone): only while it walks
static void mtest_stick(int32_t *lx, int32_t *ly) {
    if (g_mt_yield || !MTB.m.active || !MTB.m.stick_on) return;
    *lx = (int32_t)lroundf(MTB.m.stick[0] * 32767.0f), *ly = (int32_t)lroundf(MTB.m.stick[1] * 32767.0f);
}
// (s204) the self-drive's answers to the new game's weapon pick: pad bits into the driven frame's press set, next to
// atk_cmd_press and like it not on a fresh frame (its presses are dropped). Never set outside a Melty test run.
_Static_assert(MT_PK_OFF == DR_OFF && MT_PK_LIST == DR_LIST && MT_PK_ASK == DR_ASK && MT_PK_GOT == DR_GOT && MT_PK_GLIST == DR_GLIST && MT_PK_GASK == DR_GASK, "MT_PK_* = sora_dream.h DR_*");
_Static_assert(MT_BTN_A == BTN_ATTACK && MT_BTN_UP == BTN_UP && MT_BTN_DOWN == BTN_DOWN && MT_BTN_A == DREAM_BTN_A && MT_BTN_UP == DREAM_BTN_UP && MT_BTN_DOWN == DREAM_BTN_DOWN, "MT_BTN_* = the pad bits");
static uint64_t g_mt_press;
static uint64_t mtest_press(bool fresh) {
    if (g_mt_yield) g_mt_press = 0;  // (s209 review 2)
    if (!g_mt_press) return 0;
    if (!MTB.m.active) { g_mt_press = 0; return 0; }
    if (fresh) return 0;
    uint64_t b = g_mt_press;
    g_mt_press = 0;
    return b;
}
// Once a frame from sora_mod_frame, after the body and BEFORE dg_tick (it clears g_dg_posed). posed = g_dg_posed,
// exe_diag = KDG.exe (NULL: an older exe, posed counts as drawn), diag_kind = KDG.shown_kind, title = TITLE.scr.
static void mtest_frame(bool posed, int (*exe_diag)(char *, int, uint64_t *), int diag_kind, bool title) {
    if (!MTB.inited) {
        MTB.inited = true;
        MTB.t0_ms = GetTickCount64();
        char lad[MT_PATH], dir[MT_PATH];
        bool want = net_melty_wanted(), have = want && mt_localappdata(lad, sizeof lad) && mt_test_dir(dir, sizeof dir, lad);
        MtIo io = {mt_io_read, mt_io_append, mt_io_replace, mtb_log, NULL};
        mt_init(&MTB.m, have ? dir : NULL, io, NULL, 0);
        if (g_mt_pend_on >= 0) MTB.m.on = g_mt_pend_on;
        if (g_mt_pend_log >= 0) MTB.m.verbose = g_mt_pend_log;
        if (want) LOG("[mtest] Melty test mode: %s\n", have ? "watching for a test file (every 2 s)" : "no LOCALAPPDATA: off");
    }
    if (!MTB.m.wanted) return;  // (not launched by Melty: nothing at all)
    MtObs *o = &g_mto;
    if (MTB.m.active) {
        uint64_t drawn = 0;
        if (exe_diag) exe_diag(NULL, 0, &drawn);
        o->drawn = posed && (!exe_diag || drawn != MTB.drawn_last);
        MTB.drawn_last = drawn;
        o->mod_data = g_on && g_nmots > 0 && g_nhslots > 0;
        o->room = mt_room_of(net_solo(), getenv("SAVAGEWIND_SORA_NET"), NET.melty, NET.melty && sora_melty_connected() && NET.melty_self);
        o->have_pos = o->in_world || o->driving;
        o->pos[0] = g_body[0], o->pos[1] = g_body[1], o->pos[2] = g_body[2];
        o->area = NET.area;
        o->hp = o->have_pos ? linkpos_link_life() : 0;
        o->peers = netslot_count(NET.slot, NET_MAX_PEERS);
        o->attack_playing = S.special == SP_ATTACK, o->attack_clock = S.clock;
        o->diag_up = diag_kind != 0;
        o->title = g_on && title;  // (TITLE.scr: load_probe reads it on every Sora-mode frame, also the first boot's title)
        o->present = vk_frame_counter();
    }
    mt_tick(&MTB.m, (double)(GetTickCount64() - MTB.t0_ms) / 1000.0, mt_wall_ms(), o);
    if (g_mt_yield) MTB.m.stick_on = false, MTB.m.want_attack = false, MTB.m.want_press = 0;  // (s209 review 2: the autopilot's now)
    if (MTB.m.want_attack) MTB.m.want_attack = false, atk_cmd_queue(1);
    if (MTB.m.want_press) g_mt_press = MTB.m.want_press, MTB.m.want_press = 0;  // (s204: the weapon pick's answer, mtest_press)
    memset(o, 0, sizeof *o);
}
#endif
