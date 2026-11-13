// host test for runtime/sora_melty_test.h (s201): the Melty test mode's pure core (names, lines, path guard, the test file's
// JSON, "until", the check table + deadlines, the self-drive, the state JSON) on fake files, then the real file functions in a
// scratch folder (argv[1], default: $TMP).
//   clang -O1 -g -Wall -Wextra -DSORA_MTEST_TEST -Iruntime runtime/test/melty_test_test.c -o build_h/melty_test_test.exe
//   (Linux: the same; the real-file part then uses fopen / rename)
#include <stdio.h>
#ifndef SORA_MTEST_TEST
#define SORA_MTEST_TEST
#endif
#include "sora_win.h"  // (first: winsock2.h before windows.h; sora_dream.h below needs it)
#include "sora_melty_test.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) fails++, printf("FAIL line %d: %s\n", __LINE__, #c); } while (0)

// ---- fake files
typedef struct { char path[MT_PATH]; char data[65536]; size_t n; bool exists; int writes; } FFile;
static struct { FFile f[16]; int nf, reads, appends, replaces, outside; bool fail_append, quiet; char dir[MT_PATH]; } FS;
static FFile *ff(const char *p, bool make) {
    for (int i = 0; i < FS.nf; i++) if (!strcmp(FS.f[i].path, p)) return &FS.f[i];
    if (!make || FS.nf == 16) return NULL;
    FFile *f = &FS.f[FS.nf++];
    memset(f, 0, sizeof *f);
    snprintf(f->path, sizeof f->path, "%s", p);
    return f;
}
static int f_read(void *ud, const char *p, char *buf, int cap) {
    (void)ud;
    FS.reads++;
    FFile *f = ff(p, false);
    if (!f || !f->exists) return -1;
    int n = (int)f->n < cap ? (int)f->n : cap;
    memcpy(buf, f->data, (size_t)n);
    return n;
}
static int f_append(void *ud, const char *p, const char *t) {
    (void)ud;
    if (!mt_path_under(p, FS.dir)) FS.outside++;
    if (FS.fail_append) return -1;
    FS.appends++;
    FFile *f = ff(p, true);
    size_t L = strlen(t);
    if (f->n + L >= sizeof f->data) return -1;
    memcpy(f->data + f->n, t, L), f->n += L, f->data[f->n] = 0, f->exists = true, f->writes++;
    return 0;
}
static int f_replace(void *ud, const char *p, const char *t) {
    (void)ud;
    if (!mt_path_under(p, FS.dir)) FS.outside++;
    FS.replaces++;
    FFile *f = ff(p, true);
    snprintf(f->data, sizeof f->data, "%s", t), f->n = strlen(t), f->exists = true, f->writes++;
    return 0;
}
static void f_log(void *ud, const char *line) { (void)ud; if (!FS.quiet) printf("  log: %s", line); }
static void f_put(const char *p, const char *t) {
    FFile *f = ff(p, true);
    snprintf(f->data, sizeof f->data, "%s", t), f->n = strlen(t), f->exists = true;
}
static void f_gone(const char *p) { FFile *f = ff(p, false); if (f) f->exists = false, f->n = 0; }
static void fs_reset(void) { char d[MT_PATH]; snprintf(d, sizeof d, "%s", FS.dir); memset(&FS, 0, sizeof FS); snprintf(FS.dir, sizeof FS.dir, "%s", d); FS.quiet = !getenv("MTEST_VERBOSE"); }
// how many lines of the checks file start with "<verdict> <name>" (verdict "pass" / "fail")
// ---- the REAL Dream Weapon picker (runtime/sora_dream.h, s204), with the three things it calls in the game stubbed
#define LOG(...) (FS.quiet ? 0 : printf("  game: " __VA_ARGS__))  // (an expression: sora.c's LOG is used after a comma)
#include "sora_hud.h"
static int linkpos_link_heal(int n) { (void)n; return 12; }
int sora_sfx_event(const char *name, float gain) { (void)name, (void)gain; return 0; }
static int g_panels;
void sora_hud_dream(const SoraHudDream *d) { (void)d; g_panels++; }
#include "sora_dream.h"
_Static_assert(MT_PK_OFF == DR_OFF && MT_PK_LIST == DR_LIST && MT_PK_ASK == DR_ASK && MT_PK_GOT == DR_GOT && MT_PK_GLIST == DR_GLIST && MT_PK_GASK == DR_GASK, "MT_PK_* = DR_*");
_Static_assert(MT_BTN_A == DREAM_BTN_A && MT_BTN_UP == DREAM_BTN_UP && MT_BTN_DOWN == DREAM_BTN_DOWN, "MT_BTN_* = DREAM_BTN_*");

static int nlines(const char *path, const char *verdict, const char *name) {
    FFile *f = ff(path, false);
    if (!f) return 0;
    char pre[96];
    int pl = snprintf(pre, sizeof pre, "%s %s", verdict, name), n = 0;
    for (const char *l = f->data; l && *l; l = strchr(l, '\n') ? strchr(l, '\n') + 1 : NULL)
        if (!strncmp(l, pre, (size_t)pl) && (l[pl] == '\n' || l[pl] == ':')) n++;
    return n;
}
static int alllines(const char *path) {
    FFile *f = ff(path, false);
    int n = 0;
    if (f) for (size_t i = 0; i < f->n; i++) n += f->data[i] == '\n';
    return n;
}

#define DIR_ "C:\\Users\\Tester\\AppData\\Local\\Melty\\test"
#define JSON_ DIR_ "\\legend-of-kairi.json"
#define CHK_ DIR_ "\\legend-of-kairi-checks.txt"
#define STA_ DIR_ "\\legend-of-kairi-state.json"
static const char *TJSON =
    "{\"testId\":\"456e8969-19fa-48f7-b84c-156298fdb0a9\",\"mod\":\"legend-of-kairi\",\"release\":\"0.1.5-alpha\",\"run\":%d,\"runs\":2,"
    "\"seed\":1859213070,\"checks\":\"C:\\\\Users\\\\Tester\\\\AppData\\\\Local\\\\Melty\\\\test\\\\legend-of-kairi-checks.txt\","
    "\"state\":\"C:\\\\Users\\\\Tester\\\\AppData\\\\Local\\\\Melty\\\\test\\\\legend-of-kairi-state.json\","
    "\"input\":\"C:\\\\Users\\\\Tester\\\\AppData\\\\Local\\\\Melty\\\\test\\\\legend-of-kairi-input.txt\","
    "\"played\":\"C:\\\\Users\\\\Tester\\\\AppData\\\\Local\\\\Melty\\\\test\\\\legend-of-kairi-played.json\",\"until\":\"%s\"}";
static void put_json(int run, const char *until) {
    char b[4096];
    snprintf(b, sizeof b, TJSON, run, until);
    f_put(JSON_, b);
}
#define UNTIL_ "2026-11-11T21:26:42.291Z"
#define MODUNTIL_ "\"mod\":\"legend-of-kairi\",\"until\":\"" UNTIL_ "\""  // (s202: both are required by mt_parse)
static long long g_until;  // UNTIL_ in unix ms

static Mt M;
static MtObs O;
static double T;
static long long W;  // wall ms
static uint64_t PRESENT;
// step the clock by frames at 60 fps with the obs O (present advances unless frozen)
static void step(int frames, bool frozen) {
    for (int i = 0; i < frames; i++) {
        T += 1.0 / 60, W += 16;
        if (!frozen) PRESENT++;
        O.present = PRESENT;
        bool began = O.attack_began;
        mt_tick(&M, T, W, &O);
        if (M.want_attack) M.want_attack = false;
        O.attack_began = began;
    }
}
static void start(void) {
    fs_reset();
    MtIo io = {f_read, f_append, f_replace, f_log, NULL};
    mt_init(&M, DIR_, io, NULL, 0);
    memset(&O, 0, sizeof O);
    T = 0, W = g_until - 3600 * 1000LL, PRESENT = 0;
}
static int idx(const char *name) {
    for (int i = 0; i < M.ndefs; i++) if (!strcmp(M.defs[i].name, name)) return i;
    return -1;
}

static void test_names_lines(void) {
    CHECK(mt_name_ok("mod-loaded") && mt_name_ok("a") && mt_name_ok("0x") && mt_name_ok("stable-5min"));
    CHECK(!mt_name_ok("") && !mt_name_ok(NULL) && !mt_name_ok("-a") && !mt_name_ok("Mod") && !mt_name_ok("a_b") && !mt_name_ok("a b"));
    CHECK(mt_name_ok("a2345678901234567890123456789012345678901") == false);  // 41
    CHECK(mt_name_ok("a234567890123456789012345678901234567890"));            // 40
    char l[MT_WHY_MAX + 80];
    CHECK(mt_line(l, sizeof l, true, "world-up", "ignored") > 0 && !strcmp(l, "pass world-up\n"));
    CHECK(mt_line(l, sizeof l, false, "world-up", "no body\nsecond") > 0 && !strcmp(l, "fail world-up: no body second\n"));
    CHECK(mt_line(l, sizeof l, false, "world-up", "") > 0 && !strcmp(l, "fail world-up: no reason given\n"));
    CHECK(mt_line(l, sizeof l, true, "Bad_Name", NULL) == -1);
    // Melty's own client writes "in-room" / "in-world" into the same file: reserved, never ours
    CHECK(mt_name_reserved("in-room") && mt_name_reserved("in-world") && !mt_name_reserved("world-up") && !mt_name_reserved(NULL));
    CHECK(mt_name_ok("in-world") && !mt_name_usable("in-world") && !mt_name_usable("in-room") && mt_name_usable("room-seat"));
    CHECK(mt_line(l, sizeof l, true, "in-room", NULL) == -1 && mt_line(l, sizeof l, false, "in-world", "x") == -1);
    for (int i = 0; i < MT_NCHECKS; i++) CHECK(mt_name_usable(MT_CHECKS[i].name));  // (the shipped table: all usable)
    char big[600];
    memset(big, 'x', sizeof big - 1), big[sizeof big - 1] = 0;
    int n = mt_line(l, sizeof l, false, "x", big);
    CHECK(n == (int)strlen("fail x: ") + MT_WHY_MAX + 1);
}

static void test_paths(void) {
    const char *d = DIR_;
    CHECK(mt_path_under(DIR_ "\\a.txt", d));
    CHECK(mt_path_under("c:/users/tester/appdata/local/melty/test/a.txt", d));  // (slashes + case)
    CHECK(mt_path_under(DIR_ "\\sub\\a.txt", d));
    CHECK(mt_path_under(DIR_ "\\a.txt", DIR_ "\\"));  // (the dir with a trailing separator)
    CHECK(!mt_path_under(DIR_, d));                       // (the folder itself)
    CHECK(!mt_path_under(DIR_ "\\", d));
    CHECK(!mt_path_under(DIR_ "X\\a.txt", d));            // (Melty\testX)
    CHECK(!mt_path_under(DIR_ "\\..\\a.txt", d));
    CHECK(!mt_path_under(DIR_ "\\sub\\..\\..\\a.txt", d));
    CHECK(!mt_path_under(DIR_ "\\.\\a.txt", d));
    CHECK(!mt_path_under(DIR_ "\\\\a.txt", d));           // (an empty component)
    CHECK(!mt_path_under(DIR_ "\\a.txt\\", d));
    CHECK(!mt_path_under(DIR_ "\\a.txt:stream", d));
    CHECK(!mt_path_under(DIR_ "\\a.txt.", d));
    CHECK(!mt_path_under(DIR_ "\\a.txt ", d));
    CHECK(!mt_path_under(DIR_ "\\a\x01.txt", d));
    CHECK(!mt_path_under(DIR_ "\\a?.txt", d));
    CHECK(!mt_path_under("C:\\Users\\Tester\\Desktop\\a.txt", d));
    CHECK(!mt_path_under("D:\\Users\\Tester\\AppData\\Local\\Melty\\test\\a.txt", d));
    CHECK(!mt_path_under("\\\\server\\share\\Melty\\test\\a.txt", d));
    CHECK(!mt_path_under("\\\\?\\" DIR_ "\\a.txt", d));
    CHECK(!mt_path_under("Melty\\test\\a.txt", "Melty\\test"));  // (relative)
    CHECK(!mt_path_under("", d) && !mt_path_under(NULL, d) && !mt_path_under(DIR_ "\\a.txt", NULL) && !mt_path_under(DIR_ "\\a.txt", ""));
    char t[MT_PATH];
    CHECK(mt_test_dir(t, sizeof t, "C:\\Users\\Tester\\AppData\\Local") && !strcmp(t, DIR_));
    CHECK(mt_test_dir(t, sizeof t, "C:\\Users\\Tester\\AppData\\Local\\") && !strcmp(t, DIR_));
    CHECK(mt_test_dir(t, sizeof t, "E:\\") && !strcmp(t, "E:\\Melty\\test"));
    CHECK(!mt_test_dir(t, sizeof t, "relative\\x") && !mt_test_dir(t, sizeof t, NULL));
    CHECK(mt_same_path("C:\\A\\b.txt", "c:/a/B.TXT") && !mt_same_path("C:\\A\\b.txt", "C:\\A\\c.txt"));
}

static void test_parse(void) {
    char b[4096], why[96];
    MtTestFile tf;
    snprintf(b, sizeof b, TJSON, 1, UNTIL_);
    CHECK(mt_parse(b, strlen(b), &tf, why, sizeof why) == 1);
    CHECK(!strcmp(tf.test_id, "456e8969-19fa-48f7-b84c-156298fdb0a9") && !strcmp(tf.release, "0.1.5-alpha"));
    CHECK(tf.run == 1 && tf.runs == 2 && tf.seed == 1859213070LL && tf.until_ms == g_until);
    CHECK(!strcmp(tf.checks, CHK_) && !strcmp(tf.state, STA_));
    // the exact example from the task (with its own folder)
    const char *ex = "{\"testId\":\"...\",\"mod\":\"legend-of-kairi\",\"release\":\"0.1.5-alpha\",\"run\":1,\"runs\":2,\"seed\":1859213070,"
                     "\"checks\":\"C:\\\\...\\\\legend-of-kairi-checks.txt\",\"state\":\"C:\\\\...\\\\legend-of-kairi-state.json\","
                     "\"input\":\"C:\\\\...\\\\legend-of-kairi-input.txt\",\"played\":\"C:\\\\...\\\\legend-of-kairi-played.json\","
                     "\"until\":\"2026-11-11T21:26:42.291Z\"}";
    CHECK(mt_parse(ex, strlen(ex), &tf, why, sizeof why) == 1 && !strcmp(tf.checks, "C:\\...\\legend-of-kairi-checks.txt"));
    // BOM, spaces, unknown nested values, a long unknown key, escapes (\u00e9 -> UTF-8, a surrogate pair)
    const char *b2 = "\xEF\xBB\xBF { \"x\" : {\"a\":[1,2,{\"b\":null}],\"c\":true} , \"a_very_long_unknown_key_that_is_longer_than_32\":\"v\","
                     "\"testId\":\"t\\u00e9\\ud83d\\ude00\",\"run\":2,\"checks\":\"C:/a/c.txt\",\"state\":\"C:/a/s.json\",\"mod\":\"legend-of-kairi\",\"until\":\"2026-11-11T21:26:42.291Z\"}\n";
    CHECK(mt_parse(b2, strlen(b2), &tf, why, sizeof why) == 1 && !strcmp(tf.test_id, "t\xC3\xA9\xF0\x9F\x98\x80") && tf.run == 2 && tf.until_ms == g_until);
    // (s202 review) "mod" and a readable "until" are required: a file with no end time never counts as a test
#define MU_ "\"mod\":\"legend-of-kairi\",\"until\":\"2026-11-11T21:26:42.291Z\""
    static const char *need[] = {
        "{\"testId\":\"t\",\"run\":1,\"checks\":\"C:/c\",\"state\":\"C:/s\",\"mod\":\"legend-of-kairi\"}",                        // no until
        "{\"testId\":\"t\",\"run\":1,\"checks\":\"C:/c\",\"state\":\"C:/s\",\"mod\":\"legend-of-kairi\",\"until\":\"garbage\"}",  // bad until
        "{\"testId\":\"t\",\"run\":1,\"checks\":\"C:/c\",\"state\":\"C:/s\",\"mod\":\"legend-of-kairi\",\"until\":\"\"}",         // empty until
        "{\"testId\":\"t\",\"run\":1,\"checks\":\"C:/c\",\"state\":\"C:/s\",\"until\":\"2026-11-11T21:26:42.291Z\"}",             // no mod
        "{\"testId\":\"t\\u100g\",\"run\":1,\"checks\":\"C:/c\",\"state\":\"C:/s\"," MU_ "}",                                   // bad hex digit
    };
    for (size_t i = 0; i < sizeof need / sizeof need[0]; i++) {
        int r = mt_parse(need[i], strlen(need[i]), &tf, why, sizeof why);
        if (r) printf("  (need json %zu parsed)\n", i);
        CHECK(r == 0 && why[0]);
    }
    const char *good = "{\"testId\":\"t\",\"run\":1,\"checks\":\"C:/c\",\"state\":\"C:/s\"," MU_ "}";
    CHECK(mt_parse(good, strlen(good), &tf, why, sizeof why) == 1);
#undef MU_
    static const char *bad[] = {
        "", "[]", "{", "{\"testId\":\"t\",\"run\":1,\"checks\":\"C:/c\",\"state\":\"C:/s\"",           // cut off
        "{\"testId\":\"t\",\"run\":1,\"checks\":\"C:/c\",\"state\":\"C:/s\",}",                            // trailing comma
        "{\"testId\":\"t\",\"run\":\"1\",\"checks\":\"C:/c\",\"state\":\"C:/s\"}",                         // run a string
        "{\"testId\":\"t\",\"run\":1.5,\"checks\":\"C:/c\",\"state\":\"C:/s\"}",                           // run not whole
        "{\"testId\":\"t\",\"run\":0,\"checks\":\"C:/c\",\"state\":\"C:/s\"}",                             // run 0
        "{\"testId\":\"t\",\"checks\":\"C:/c\",\"state\":\"C:/s\"}",                                       // no run
        "{\"run\":1,\"checks\":\"C:/c\",\"state\":\"C:/s\"}",                                              // no testId
        "{\"testId\":\"t\",\"run\":1,\"state\":\"C:/s\"}",                                                 // no checks
        "{\"testId\":\"t\",\"run\":1,\"checks\":\"C:/c\",\"state\":\"C:/s\",\"mod\":\"other-mod\"}",       // another mod
        "{\"testId\":\"t\",\"run\":1,\"checks\":\"C:/c\\u0000x\",\"state\":\"C:/s\"}",                     // a NUL
        "{\"testId\":\"t\",\"run\":1,\"checks\":\"C:/c\\q\",\"state\":\"C:/s\"}",                          // bad escape
        "{\"testId\":\"t\",\"run\":1,\"checks\":\"C:/c\",\"state\":\"C:/s\"} x",                           // text after
        "{\"testId\":\"t\" \"run\":1}",                                                                     // no comma
        "{\"testId\":\"t\",\"run\":1,\"checks\":\"C:/c\",\"state\":\"C:/s\",\"x\":[1,2}",                  // bad nesting
        "{\"testId\":\"t\",\"run\":1,\"checks\":\"C:/c\",\"state\":\"C:/s\",\"x\":{{{{{{{{{{{{}}}}}}}}}}}}}",  // too deep
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        int r = mt_parse(bad[i], strlen(bad[i]), &tf, why, sizeof why);
        if (r) printf("  (bad json %zu parsed)\n", i);
        CHECK(r == 0 && why[0]);
    }
    // a path longer than MT_PATH: refused, not cut
    static char huge[MT_PATH * 2 + 200];
    int k = snprintf(huge, sizeof huge, "{\"testId\":\"t\",\"run\":1,\"state\":\"C:/s\",\"checks\":\"C:/");
    for (int i = 0; i < MT_PATH + 10; i++) huge[k++] = 'a';
    snprintf(huge + k, sizeof huge - (size_t)k, "\"}");
    CHECK(mt_parse(huge, strlen(huge), &tf, why, sizeof why) == 0);
    // until forms
    long long ms;
    CHECK(mt_iso_ms("1970-01-01T00:00:00Z", &ms) && ms == 0);
    CHECK(mt_iso_ms("1970-01-01T00:00:01.5Z", &ms) && ms == 1500);
    CHECK(mt_iso_ms("2000-03-01T00:00:00Z", &ms) && ms == 951868800000LL);
    CHECK(mt_iso_ms("2026-11-11T22:26:42.291+01:00", &ms) && ms == g_until);
    CHECK(!mt_iso_ms("2026-13-11T21:26:42Z", &ms) && !mt_iso_ms("2026-11-11", &ms) && !mt_iso_ms("2026-11-11T21:26:42Q", &ms));
}

// one full run: activation, each line once, deadlines, skip, the state file, the self-drive, stable
static void test_run(void) {
    start();
    // not launched by Melty / no folder: nothing at all
    {
        Mt off;
        MtIo io = {f_read, f_append, f_replace, f_log, NULL};
        mt_init(&off, NULL, io, NULL, 0);
        for (int i = 0; i < 600; i++) mt_tick(&off, i / 60.0, W, &O);
        CHECK(FS.reads == 0 && FS.appends == 0 && FS.replaces == 0 && !off.active);
    }
    // no test file: reads every 2 s, no writes
    step(600, false);  // 10 s
    CHECK(FS.reads >= 5 && FS.reads <= 6 && FS.appends == 0 && FS.replaces == 0 && !M.active);
    // the test file comes; the game is still on its title: active, state written ~once a second, no verdicts yet
    put_json(1, UNTIL_);
    O.mod_data = true, O.room = -1;  // (no room wanted: room-seat is skipped)
    step(1, false);
    CHECK(!M.active);  // (the next probe: within 2 s)
    step(125, false);
    CHECK(M.active && M.base == 0);
    CHECK(nlines(CHK_, "pass", "mod-loaded") == 1);
    int rep0 = FS.replaces;
    step(600, false);  // 10 s more
    CHECK(FS.replaces - rep0 >= 9 && FS.replaces - rep0 <= 11);
    CHECK(nlines(CHK_, "pass", "mod-loaded") == 1);  // (once only)
    CHECK(M.st[idx("room-seat")] == MT_SKIP && nlines(CHK_, "pass", "room-seat") == 0 && nlines(CHK_, "fail", "room-seat") == 0);
    FFile *sf = ff(STA_, false);
    CHECK(sf && strstr(sf->data, "\"run\":1,") && strstr(sf->data, "\"mod-loaded\":\"pass\"") && strstr(sf->data, "\"world-up\":\"wait\"") &&
          strstr(sf->data, "\"room-seat\":\"skip\"") && strstr(sf->data, "\"area\":\"-\""));
    // the world comes (the opening: Link's pad, Sora his puppet): world-up, sora-drawn, hud-up
    O.in_world = true, O.drawn = true, O.hud = true, O.have_pos = true, O.area = "Main\"Field", O.hp = 12;
    O.pos[0] = -1133.0f, O.pos[1] = 236.8f, O.pos[2] = 1917.6f;
    step(120, false);
    CHECK(nlines(CHK_, "pass", "world-up") == 1 && nlines(CHK_, "pass", "sora-drawn") == 1 && nlines(CHK_, "pass", "hud-up") == 1);
    sf = ff(STA_, false);
    CHECK(sf && strstr(sf->data, "\"area\":\"Main\\\"Field\"") && strstr(sf->data, "\"pos\":[-1133.0,236.8,1917.6]") && strstr(sf->data, "\"hp\":12"));
    // never driving: sora-drives / sora-moves / attack-swings fail at their deadlines, once each, with a reason
    while (T < 419.0) step(60, false);
    CHECK(nlines(CHK_, "fail", "sora-drives") == 0);
    step(120, false);
    CHECK(nlines(CHK_, "fail", "sora-drives") == 1 && strstr(ff(CHK_, false)->data, "fail sora-drives: Sora never drove"));
    while (T < 541.0) step(60, false);
    CHECK(nlines(CHK_, "fail", "sora-moves") == 1 && nlines(CHK_, "fail", "attack-swings") == 1);
    CHECK(!M.stick_on);  // (the self-drive never started)
    // stable-5min: 300 s since the world came at t ~ 20 -> passes near t 320 (already happened above)
    CHECK(nlines(CHK_, "pass", "stable-5min") == 1);
    step(6000, false);  // (100 s more: nothing new)
    CHECK(alllines(CHK_) == 8);  // 4 pass + 3 fail + stable; room skipped
    CHECK(FS.outside == 0);
}

static void test_selfdrive(void) {
    start();
    put_json(1, UNTIL_);
    O.mod_data = true, O.room = 1, O.in_world = O.drawn = O.hud = O.have_pos = true;
    step(60, false);
    CHECK(nlines(CHK_, "pass", "room-seat") == 1);
    // Sora drives; the body follows the self-drive's stick at 4 m/s
    O.driving = true;
    int walks = 0, attacks = 0;
    for (int i = 0; i < 60 * 30 && !M.swung; i++) {
        T += 1.0 / 60, W += 16, O.present = ++PRESENT;
        mt_tick(&M, T, W, &O);
        O.attack_began = false;  // (one frame only)
        if (M.stick_on) {
            walks++;
            O.pos[0] += M.stick[0] * 4.0f / 60, O.pos[2] += M.stick[1] * 4.0f / 60;
            CHECK(fabsf(M.stick[0]) + fabsf(M.stick[1]) > 0.99f);
        }
        static int playing;
        if (M.want_attack) M.want_attack = false, attacks++, O.attack_began = true, playing = 1;
        if (playing) O.attack_playing = true, O.attack_clock = (float)playing++;
    }
    step(2, false);  // (the verdict is written at the next frame's check pass)
    CHECK(nlines(CHK_, "pass", "sora-drives") == 1 && nlines(CHK_, "pass", "sora-moves") == 1 && nlines(CHK_, "pass", "attack-swings") == 1);
    CHECK(walks > 20 && walks < 60 && attacks == 1 && M.sd == SD_DONE && !M.stick_on);
    // a body that never moves (a wall): 4 tries, then the attack anyway; sora-moves fails at its deadline
    start();
    put_json(1, UNTIL_);
    O = (MtObs){.mod_data = true, .room = 1, .in_world = true, .drawn = true, .hud = true, .have_pos = true, .driving = true};
    int tries_dirs = 0;
    float last[2] = {9, 9};
    attacks = 0;
    while (T < 500) {
        T += 1.0 / 60, W += 16, O.present = ++PRESENT;
        mt_tick(&M, T, W, &O);
        if (M.stick_on && (M.stick[0] != last[0] || M.stick[1] != last[1])) tries_dirs++, last[0] = M.stick[0], last[1] = M.stick[1];
        if (M.want_attack) M.want_attack = false, attacks++;  // (no attack ever starts)
    }
    CHECK(tries_dirs == MT_WALK_TRIES && attacks == MT_SWING_TRIES && M.sd == SD_DONE);
    CHECK(nlines(CHK_, "fail", "sora-moves") == 1 && nlines(CHK_, "pass", "sora-moves") == 0 && nlines(CHK_, "pass", "sora-drives") == 1);
    // not driving: the walk time does not run (Link's pad), the stick is only offered
    start();
    put_json(1, UNTIL_);
    O = (MtObs){.mod_data = true, .room = 1, .in_world = true, .have_pos = true, .driving = true};
    step(120, false);  // drives 2 s -> settle -> walk
    CHECK(M.sd == SD_WALK);
    double walked = M.sd_walked;
    O.driving = false;
    step(600, false);
    CHECK(M.sd == SD_WALK && M.sd_walked == walked && !M.moved);
}

static void test_stable(void) {
    // the draw notice in the world: stable fails at once (not at its deadline)
    start();
    put_json(1, UNTIL_);
    O = (MtObs){.mod_data = true, .room = 1, .in_world = true, .drawn = true, .hud = true, .have_pos = true};
    step(600, false);
    O.diag_up = true;
    step(2, false);
    CHECK(nlines(CHK_, "fail", "stable-5min") == 1 && strstr(ff(CHK_, false)->data, "can't be drawn"));
    // a 20 s freeze between two frames: fail
    start();
    put_json(1, UNTIL_);
    O = (MtObs){.mod_data = true, .room = 1, .in_world = true, .drawn = true, .hud = true, .have_pos = true};
    step(600, false);
    T += 20, W += 20000;
    step(2, false);
    CHECK(nlines(CHK_, "fail", "stable-5min") == 1 && strstr(ff(CHK_, false)->data, "froze 20 s"));
    // frames stop being presented (the game still calls the mod): no pass after 300 s, fail at the deadline
    start();
    put_json(1, UNTIL_);
    O = (MtObs){.mod_data = true, .room = 1, .in_world = true, .drawn = true, .hud = true, .have_pos = true};
    step(60, false);
    while (T < 899) step(60, true);
    CHECK(nlines(CHK_, "pass", "stable-5min") == 0 && nlines(CHK_, "fail", "stable-5min") == 0);
    step(120, true);
    CHECK(nlines(CHK_, "fail", "stable-5min") == 1);
}

static void test_gone_second_run(void) {
    start();
    put_json(1, UNTIL_);
    O = (MtObs){.mod_data = true, .room = 0};
    step(60, false);
    CHECK(M.active && nlines(CHK_, "pass", "mod-loaded") == 1);
    // the test file disappears mid-run: within 1 s no more writes, whatever happens after
    f_gone(JSON_);
    step(65, false);
    CHECK(!M.active);
    int a = FS.appends, r = FS.replaces;
    O.in_world = O.drawn = O.hud = O.have_pos = true, O.room = 1;
    while (T < 1000) step(60, false);  // (checks would pass, deadlines would hit)
    CHECK(FS.appends == a && FS.replaces == r);
    // the same run's file again (e.g. Melty rewrote it): on again, lines already written are NOT written again
    put_json(1, UNTIL_);
    step(130, false);
    CHECK(M.active && nlines(CHK_, "pass", "mod-loaded") == 1);
    CHECK(nlines(CHK_, "fail", "world-up") == 1);  // (its deadline passed while the file was gone: failed once, on the first frame back)
    // a second run number (in the same process): a new session, base = now, every check once more for run 2
    int before = alllines(CHK_);
    put_json(2, UNTIL_);
    step(130, false);
    CHECK(M.tf.run == 2 && M.base > 900 && nlines(CHK_, "pass", "mod-loaded") == 2 && nlines(CHK_, "pass", "world-up") == 1);
    CHECK(alllines(CHK_) > before);
    CHECK(strstr(ff(STA_, false)->data, "\"run\":2,") != NULL);
    // run 2's deadlines count from its own start: world-up passed; sora-drives fails 420 s after base, not before
    double b2 = M.base;
    while (T < b2 + 419) step(60, false);
    CHECK(nlines(CHK_, "fail", "sora-drives") == 1);  // (run 1's fail only)
    step(180, false);
    CHECK(nlines(CHK_, "fail", "sora-drives") == 2);
    CHECK(FS.outside == 0);
}

static void test_bad_inputs(void) {
    // malformed json: stays off, no writes, one log line
    start();
    f_put(JSON_, "{\"testId\":\"t\",\"run\":1,");
    O = (MtObs){.mod_data = true};
    step(600, false);
    CHECK(!M.active && FS.appends == 0 && FS.replaces == 0);
    // malformed while a run is going (Melty rewriting it): the run goes on
    put_json(1, UNTIL_);
    step(130, false);
    CHECK(M.active);
    f_put(JSON_, "{\"testId\":");
    step(300, false);
    CHECK(M.active && FS.replaces > 0);
    // a checks path outside the test folder: refused, nothing written (the state path is fine but not written either)
    start();
    f_put(JSON_, "{\"testId\":\"t\",\"run\":1,\"checks\":\"C:\\\\Users\\\\Tester\\\\Desktop\\\\x.txt\",\"state\":\"" "C:\\\\Users\\\\Tester\\\\AppData\\\\Local\\\\Melty\\\\test\\\\s.json\"," MODUNTIL_ "}");
    step(600, false);
    CHECK(!M.active && FS.appends == 0 && FS.replaces == 0);
    f_put(JSON_, "{\"testId\":\"t\",\"run\":1,\"checks\":\"C:\\\\Users\\\\Tester\\\\AppData\\\\Local\\\\Melty\\\\test\\\\..\\\\..\\\\x.txt\",\"state\":\"C:\\\\Users\\\\Tester\\\\AppData\\\\Local\\\\Melty\\\\test\\\\s.json\"," MODUNTIL_ "}");
    step(600, false);
    CHECK(!M.active && FS.appends == 0 && FS.replaces == 0);
    f_put(JSON_, "{\"testId\":\"t\",\"run\":1,\"checks\":\"" "C:\\\\Users\\\\Tester\\\\AppData\\\\Local\\\\Melty\\\\test\\\\legend-of-kairi.json\",\"state\":\"C:\\\\Users\\\\Tester\\\\AppData\\\\Local\\\\Melty\\\\test\\\\s.json\"," MODUNTIL_ "}");
    step(600, false);
    CHECK(!M.active && FS.appends == 0 && FS.replaces == 0);  // (the checks path = the test file itself)
    f_put(JSON_, "{\"testId\":\"t\",\"run\":1,\"checks\":\"C:/Users/Tester/AppData/Local/Melty/test/s.json.tmp\",\"state\":\"C:/Users/Tester/AppData/Local/Melty/test/s.json\"," MODUNTIL_ "}");
    step(600, false);
    CHECK(!M.active && FS.appends == 0 && FS.replaces == 0);  // (s202 review: the checks path = the state file's temp name)
    f_put(JSON_, "{\"testId\":\"t\",\"run\":1,\"checks\":\"C:/Users/Tester/AppData/Local/Melty/test/c.txt\",\"state\":\"C:/Users/Tester/AppData/Local/Melty/test/s.json\"," MODUNTIL_ "}");
    step(130, false);
    CHECK(M.active);  // (the same file with good paths IS taken: the refusals above came from the path guard, not the parser)
    mt_stop(&M, "test");
    f_put(JSON_, "");
    step(130, false);
    // an active run whose file switches to an outside path: stops at once
    put_json(1, UNTIL_);
    step(130, false);
    CHECK(M.active);
    int a = FS.appends, r = FS.replaces;
    f_put(JSON_, "{\"testId\":\"456e8969-19fa-48f7-b84c-156298fdb0a9\",\"run\":1,\"checks\":\"D:\\\\x.txt\",\"state\":\"D:\\\\s.json\"," MODUNTIL_ "}");
    step(130, false);
    CHECK(!M.active && FS.appends == a && FS.replaces <= r + 1);
    CHECK(FS.outside == 0);
    // bad check names in a table: never written, the others still are; a 13th check is not taken
    start();
    static const MtCheckDef defs[14] = {
        {"Bad_Name", mtc_loaded, 10, "x", false}, {"ok-one", mtc_loaded, 10, "x", false}, {"-bad", mtc_loaded, 10, "x", false},
        {"c3", mtc_world, 10, "x", false}, {"in-room", mtc_loaded, 10, "x", false}, {"in-world", mtc_loaded, 10, "x", false}, {"c6", mtc_world, 10, "x", false},
        {"c7", mtc_world, 10, "x", false}, {"c8", mtc_world, 10, "x", false}, {"c9", mtc_world, 10, "x", false}, {"c10", mtc_world, 10, "x", false},
        {"c11", mtc_world, 10, "x", false}, {"c12", mtc_world, 10, "x", false}, {"c13", mtc_world, 10, "x", false},
    };
    MtIo io = {f_read, f_append, f_replace, f_log, NULL};
    mt_init(&M, DIR_, io, defs, 14);
    CHECK(M.ndefs == MT_MAX_CHECKS);
    put_json(1, UNTIL_);
    O = (MtObs){.mod_data = true};
    step(60 * 12, false);
    CHECK(nlines(CHK_, "pass", "ok-one") == 1 && nlines(CHK_, "fail", "c3") == 1 && nlines(CHK_, "fail", "c12") == 0 && nlines(CHK_, "fail", "c11") == 1);
    CHECK(!strstr(ff(CHK_, false)->data, "Bad_Name") && !strstr(ff(CHK_, false)->data, "-bad") && alllines(CHK_) == 8);
    CHECK(!strstr(ff(CHK_, false)->data, "in-room") && !strstr(ff(CHK_, false)->data, "in-world"));  // (reserved: Melty's own)
    CHECK(!strstr(ff(STA_, false)->data, "Bad_Name") && !strstr(ff(STA_, false)->data, "in-room") && !strstr(ff(STA_, false)->data, "in-world"));
    // an append that fails: retried, then written exactly once
    start();
    put_json(1, UNTIL_);
    FS.fail_append = true;
    O = (MtObs){.mod_data = true};
    step(180, false);
    CHECK(M.st[idx("mod-loaded")] == MT_PASS && nlines(CHK_, "pass", "mod-loaded") == 0);
    FS.fail_append = false;
    step(180, false);
    CHECK(nlines(CHK_, "pass", "mod-loaded") == 1);
    step(600, false);
    CHECK(nlines(CHK_, "pass", "mod-loaded") == 1);
}

static void test_until_and_key(void) {
    // until: 3 s before it every waiting check fails ("time ran out"); after it nothing is written; a file left behind does not
    // turn it on again
    start();
    W = g_until - 60 * 1000LL;
    put_json(1, UNTIL_);
    O = (MtObs){.mod_data = true, .room = 1};
    step(60, false);
    CHECK(M.active);
    while (W < g_until - 3000) step(1, false);
    step(2, false);
    CHECK(nlines(CHK_, "fail", "world-up") == 1 && strstr(ff(CHK_, false)->data, "the test's time ran out"));
    int n = alllines(CHK_);
    CHECK(n == M.ndefs);  // (mod-loaded + room-seat pass, the rest fail)
    while (W < g_until + 5000) step(1, false);
    CHECK(!M.active);
    int a = FS.appends, r = FS.replaces;
    step(600, false);
    CHECK(FS.appends == a && FS.replaces == r && !M.active);
    // a file whose until is already over at the start: never on
    start();
    W = g_until + 1000;
    put_json(1, UNTIL_);
    step(600, false);
    CHECK(!M.active && FS.appends == 0 && FS.replaces == 0);
    // set mtest 0: no writes; set mtest 1: the same session again, no line twice
    start();
    put_json(1, UNTIL_);
    O = (MtObs){.mod_data = true, .room = 1};
    step(60, false);
    CHECK(M.active && nlines(CHK_, "pass", "mod-loaded") == 1);
    CHECK(mt_set_key(&M, "mtest", 0) && !mt_set_key(&M, "other", 1));
    step(1, false);
    CHECK(!M.active);
    a = FS.appends, r = FS.replaces;
    O.in_world = true;
    step(600, false);
    CHECK(FS.appends == a && FS.replaces == r);
    mt_set_key(&M, "mtest", 1);
    step(60, false);
    CHECK(M.active && nlines(CHK_, "pass", "mod-loaded") == 1 && nlines(CHK_, "pass", "world-up") == 1);
    CHECK(mt_set_key(&M, "mtestlog", 1) && M.verbose == 1);
}

// (s201 test run 2) the game sat on BotW's title screen the whole run: every world check fails at its own deadline and says so;
// the state file shows "title":1. mod-loaded still passes (Sora mode is on on the title).
static void test_title(void) {
    start();
    put_json(1, UNTIL_);
    O = (MtObs){.mod_data = true, .room = 1, .title = true};
    step(130, false);
    CHECK(M.active && strstr(ff(STA_, false)->data, "\"title\":1,\"world\":0,") != NULL);
    while (T < 299) step(60, false);
    CHECK(nlines(CHK_, "fail", "world-up") == 0);
    step(120, false);
    CHECK(nlines(CHK_, "fail", "world-up") == 1 && strstr(ff(CHK_, false)->data, "fail world-up: BotW's world never came up") &&
          strstr(ff(CHK_, false)->data, "still on BotW's title screen after 300 s, the world never came)"));
    while (T < 1000) step(60, false);
    const char *world[] = {"world-up", "sora-drawn", "hud-up", "sora-drives", "sora-moves", "attack-swings", "stable-5min"};
    for (int i = 0; i < 7; i++) CHECK(nlines(CHK_, "fail", world[i]) == 1);
    CHECK(nlines(CHK_, "pass", "mod-loaded") == 1 && nlines(CHK_, "pass", "room-seat") == 1 && alllines(CHK_) == 9);
    // the title went away (a load) but the world never came: the fail says when the title was last seen
    start();
    put_json(1, UNTIL_);
    O = (MtObs){.mod_data = true, .room = 1, .title = true};
    step(60 * 100, false);
    O.title = false;
    while (T < 302) step(60, false);
    CHECK(strstr(ff(CHK_, false)->data, "the world never came (title screen last seen at 100 s)") != NULL);
    // mod-loaded is not a world check: no title text on its fail
    start();
    put_json(1, UNTIL_);
    O = (MtObs){.title = true, .room = 1};
    while (T < 92) step(60, false);
    CHECK(nlines(CHK_, "fail", "mod-loaded") == 1 && !strstr(ff(CHK_, false)->data, "title"));
}

static void test_real_files(const char *base) {
    char dir[MT_PATH], p[MT_PATH], q[MT_PATH], buf[256];
    snprintf(dir, sizeof dir, "%s/mtest_real", base);
#ifdef _WIN32
    CreateDirectoryA(dir, NULL);
#else
    char cmd[MT_PATH + 16];
    snprintf(cmd, sizeof cmd, "mkdir -p '%s'", dir);
    if (system(cmd)) {}
#endif
    snprintf(p, sizeof p, "%s/checks-\xC3\xA9.txt", dir);  // (a non-ASCII name: UTF-8 -> the wide API)
    snprintf(q, sizeof q, "%s/state.json", dir);
    remove(p);
#ifdef _WIN32
    { wchar_t w[MT_PATH]; if (mt_wide(p, w, MT_PATH)) _wremove(w); }
#endif
    remove(q);
    CHECK(mt_io_read(NULL, p, buf, sizeof buf) == -1);
    CHECK(mt_io_append(NULL, p, "pass a\n") == 0 && mt_io_append(NULL, p, "fail b: why\n") == 0);
    int n = mt_io_read(NULL, p, buf, sizeof buf - 1);
    CHECK(n == 19 && !memcmp(buf, "pass a\nfail b: why\n", 19));
    // another writer (Melty's own client) appends to the same file with its own handle kept open: nothing is lost or overwritten
    FILE *other = NULL;
#ifdef _WIN32
    { wchar_t w[MT_PATH]; if (mt_wide(p, w, MT_PATH)) other = _wfopen(w, L"ab"); }
#else
    other = fopen(p, "ab");
#endif
    CHECK(other != NULL);
    if (other) {
        setvbuf(other, NULL, _IONBF, 0);
        fputs("pass in-room\n", other);
        CHECK(mt_io_append(NULL, p, "pass c\n") == 0);
        fputs("pass in-world\n", other);
        fclose(other);
    }
    n = mt_io_read(NULL, p, buf, sizeof buf - 1);
    CHECK(n == 19 + 13 + 7 + 14 && !memcmp(buf, "pass a\nfail b: why\npass in-room\npass c\npass in-world\n", (size_t)n));
    CHECK(mt_io_replace(NULL, q, "{\"run\":1}\n") == 0 && mt_io_replace(NULL, q, "{\"run\":2}\n") == 0);
    n = mt_io_read(NULL, q, buf, sizeof buf - 1);
    CHECK(n == 10 && !memcmp(buf, "{\"run\":2}\n", 10));
    char tmp[MT_PATH + 8];
    snprintf(tmp, sizeof tmp, "%s.tmp", q);
    CHECK(mt_io_read(NULL, tmp, buf, sizeof buf) == -1);  // (no temp file left)
    snprintf(tmp, sizeof tmp, "%s/no/such/dir/x.txt", dir);
    CHECK(mt_io_append(NULL, tmp, "x") != 0 && mt_io_replace(NULL, tmp, "x") != 0);
    char lad[MT_PATH];
    bool have = mt_localappdata(lad, sizeof lad);
    printf("  LOCALAPPDATA %s, wall clock %lld ms\n", have ? "found" : "not set", mt_wall_ms());
    CHECK(mt_wall_ms() > 1700000000000LL);
}

// ---- s204: the new game's weapon pick (test bc7266ba: the 3 attack presses went into the picker; "sora-moves" passed on the
// wake-up cutscene's 3.28 m, not on the stick)
static struct { uint64_t pend; bool atk_pend, fresh, was_ui, wall; int attacks, picks, playing, atk_in_picker; } G;
static void picker_reset(int give_up) {
    SetEnvironmentVariableA("SORA_DREAM_PICK", NULL);
    memset(&DR, 0, sizeof DR);
    DR.auto_on = 1, DR.wait = 90, DR.loaded = DR.tried = true, DR.fn = sora_hud_dream;
    dream_weapon = dream_giveup = -1;
    dream_live_set("dreamgu", give_up);
    memset(&G, 0, sizeof G);
}
// one game frame in sora.c's order: the driving path (the queued presses -> the picker, which owns pad + stick while up -> the
// attack / the walk at 4 m/s), then mtest_frame (mt_tick; its attack / pad presses are queued for the next driven frame)
static void game_frame(bool driving) {
    T += 1.0 / 60, W += 16, O.present = ++PRESENT;
    O.driving = driving, O.picker = 0, O.picker_cur = 0, O.attack_began = false;
    if (driving) {
        uint64_t press = 0, held = 0;
        bool fresh = G.fresh, walking = M.stick_on && !G.was_ui && !G.wall;
        G.fresh = false;
        if (!fresh) press |= G.pend, G.pend = 0;                             // (mtest_press)
        if (G.atk_pend && !fresh) press |= DREAM_BTN_A, G.atk_pend = false;  // (atk_cmd_press)
        bool ui = dream_tick(PRESENT, !G.playing && !walking, 12, &press, &held);
        O.picker = DR.st, O.picker_cur = DR.cur;
        if (!ui && G.was_ui) G.fresh = true;  // (sora.c: the frame after the picker closed is fresh)
        G.was_ui = ui;
        if (!ui && (press & DREAM_BTN_A) && !G.playing) O.attack_began = true, G.playing = 1;
        if (!ui && M.stick_on && !G.wall) O.pos[0] += M.stick[0] * 4.0f / 60, O.pos[2] += M.stick[1] * 4.0f / 60;
    }
    if (G.playing) O.attack_playing = true, O.attack_clock = (float)G.playing++;
    mt_tick(&M, T, W, &O);
    if (M.want_attack) M.want_attack = false, G.atk_pend = true, G.attacks++, G.atk_in_picker += DR.st != DR_OFF;
    if (M.want_press) G.pend = M.want_press, M.want_press = 0, G.picks++;
}
static void world_obs(void) { O = (MtObs){.mod_data = true, .room = 1, .in_world = true, .drawn = true, .hud = true, .have_pos = true}; }

// the picker opens while Sora stands, before the walk: the self-drive answers it (a weapon is picked), then walks and swings once
static void test_pick_before_walk(void) {
    start(), put_json(1, UNTIL_), picker_reset(0), world_obs();
    bool opened = false;
    for (int i = 0; i < 60 * 60 && M.sd != SD_DONE; i++) game_frame(true), opened |= DR.st != DR_OFF;
    game_frame(true), game_frame(true);
    CHECK(opened && DR.st == DR_OFF && dream_weapon >= 0 && dream_weapon <= 2 && dream_giveup == -1);
    CHECK(g_panels > 0);  // (the real picker drew its panel: it was really up)
    CHECK(G.picks >= 3 && G.picks <= 6);
    CHECK(G.atk_in_picker == 0 && G.attacks == 1);
    CHECK(M.moved && M.swung && M.sd == SD_DONE);
    CHECK(nlines(CHK_, "pass", "sora-moves") == 1 && nlines(CHK_, "pass", "attack-swings") == 1);
}
// test bc7266ba: the picker opens between the walk and the attack press. No attack press goes into it; the swing plays after it.
static void test_pick_before_attack(void) {
    start(), put_json(1, UNTIL_), picker_reset(0), world_obs();
    DR.auto_on = 0, DR.wait = 10;
    bool opened = false;
    for (int i = 0; i < 60 * 60 && M.sd != SD_DONE; i++) {
        if (M.sd == SD_GAP && M.moved) dream_live_set("dreamauto", 1);
        game_frame(true), opened |= DR.st != DR_OFF;
    }
    game_frame(true), game_frame(true);
    CHECK(opened && DR.st == DR_OFF && dream_weapon >= 0);
    CHECK(G.atk_in_picker == 0 && G.attacks == 1);
    CHECK(M.swung && nlines(CHK_, "pass", "attack-swings") == 1);
}
// live "set dreamgu 1" (KH1's give-up step): picked one, gave another up, closed
static void test_pick_give_up_step(void) {
    start(), put_json(1, UNTIL_), picker_reset(1), world_obs();
    for (int i = 0; i < 60 * 60 && M.sd != SD_DONE; i++) game_frame(true);
    CHECK(DR.st == DR_OFF && dream_weapon >= 0 && dream_giveup >= 0 && dream_giveup != dream_weapon);
    CHECK(G.picks <= 10 && G.atk_in_picker == 0 && M.swung);
}
// the pad goes to Link mid-walk (the wake-up cutscene) and he is carried 3.28 m: not the stick's distance, "sora-moves" stays open
static void test_carried_is_not_moved(void) {
    start(), put_json(1, UNTIL_), picker_reset(0), world_obs();
    dream_weapon = 0, G.wall = true;  // (no picker; a wall: the stick moves nothing)
    for (int i = 0; i < 60 * 10 && M.sd != SD_WALK; i++) game_frame(true);
    CHECK(M.sd == SD_WALK);
    for (int i = 0; i < 20; i++) game_frame(true);
    for (int i = 0; i < 120; i++) game_frame(false);  // (Link's pad)
    O.pos[0] += 2.14f, O.pos[2] -= 2.48f;             // (the cutscene put him on the bed)
    for (int i = 0; i < 30; i++) game_frame(true);
    CHECK(!M.moved && nlines(CHK_, "pass", "sora-moves") == 0);
    // real distance still counts across such a gap: 1 m before + 1.2 m after
    start(), put_json(1, UNTIL_), picker_reset(0), world_obs();
    dream_weapon = 0;
    for (int i = 0; i < 60 * 10 && M.sd != SD_WALK; i++) game_frame(true);
    for (int i = 0; i < 15; i++) game_frame(true);  // (1 m)
    CHECK(M.sd == SD_WALK && !M.moved);
    for (int i = 0; i < 60; i++) game_frame(false);
    O.pos[0] += 30;
    for (int i = 0; i < 18 && !M.moved; i++) game_frame(true);
    CHECK(M.moved);
}
// a picker that takes no answer: MT_PICK_MAX presses, then the self-drive goes on and the checks' reasons name the picker
static void test_pick_stuck(void) {
    start(), put_json(1, UNTIL_), world_obs();
    O.driving = true;
    int picks = 0;
    while (T < 560) {
        T += 1.0 / 60, W += 16, O.present = ++PRESENT, O.picker = MT_PK_LIST, O.picker_cur = 0;
        mt_tick(&M, T, W, &O);
        if (M.want_press) { CHECK(M.want_press == MT_BTN_A); M.want_press = 0, picks++; }
        M.want_attack = false;
    }
    CHECK(picks == MT_PICK_MAX);
    FFile *f = ff(CHK_, false);
    CHECK(nlines(CHK_, "fail", "attack-swings") == 1 && nlines(CHK_, "fail", "sora-moves") == 1);
    CHECK(f && strstr(f->data, "weapon pick"));
}

// (s207) the solo build: room-seat's room value, then a whole run that plays well (world, driving, the self-drive's walk and
// swing, 5 minutes): solo = the 8 other checks pass and nothing else is written; co-op = the same 8 + room-seat, as before.
static void play_well(int room) {
    start();
    put_json(1, UNTIL_);
    O = (MtObs){.mod_data = true, .room = room, .in_world = true, .drawn = true, .hud = true, .have_pos = true, .driving = true};
    int playing = 0;
    while (T < 400) {
        T += 1.0 / 60, W += 16, O.present = ++PRESENT;
        mt_tick(&M, T, W, &O);
        O.attack_began = false;
        if (M.stick_on) O.pos[0] += M.stick[0] * 4.0f / 60, O.pos[2] += M.stick[1] * 4.0f / 60;
        if (M.want_attack) M.want_attack = false, O.attack_began = true, playing = 1;
        if (playing) O.attack_playing = true, O.attack_clock = (float)playing++;
    }
}
static void test_solo(void) {
    // mt_room_of: the solo build never wants a room; without it, the old rule (s201) exactly
    CHECK(mt_room_of(true, NULL, false, false) == -1 && mt_room_of(true, "1", false, false) == -1 && mt_room_of(true, "0", false, false) == -1);
    const char *nets[] = {NULL, "", "0", "1", "2", "x"};
    int same = 0, n = 0;
    for (int i = 0; i < 6; i++)
        for (int melty = 0; melty < 2; melty++)
            for (int conn = 0; conn < 2; conn++)
                for (int self = 0; self < 2; self++, n++) {
                    const char *ne = nets[i];
                    int old = ne && atoi(ne) == 0 && !melty ? -1 : melty && conn && self ? 1 : 0;  // (sora_melty_test.h before s207)
                    same += mt_room_of(false, ne, melty, melty && conn && self) == old;
                }
    CHECK(same == n && n == 48);
    // a solo run: the 8 checks pass, room-seat is skipped, nothing of the room is written (Melty's in-room / in-world neither)
    static const char *const eight[] = {"mod-loaded", "world-up", "sora-drawn", "hud-up", "sora-drives", "sora-moves", "attack-swings", "stable-5min"};
    play_well(mt_room_of(true, "0", false, false));
    int pass8 = 0;
    for (int i = 0; i < 8; i++) pass8 += nlines(CHK_, "pass", eight[i]) == 1;
    CHECK(pass8 == 8 && alllines(CHK_) == 8);
    CHECK(M.st[idx("room-seat")] == MT_SKIP && nlines(CHK_, "pass", "room-seat") == 0 && nlines(CHK_, "fail", "room-seat") == 0);
    CHECK(!strstr(ff(CHK_, false)->data, "room") && !strstr(ff(CHK_, false)->data, "fail "));
    CHECK(strstr(ff(STA_, false)->data, "\"room-seat\":\"skip\"") && strstr(ff(STA_, false)->data, "\"room\":0"));
    CHECK(FS.outside == 0);
    // the same run in the co-op build, seated in the room: the 8 + room-seat, as before
    play_well(mt_room_of(false, "1", true, true));
    pass8 = 0;
    for (int i = 0; i < 8; i++) pass8 += nlines(CHK_, "pass", eight[i]) == 1;
    CHECK(pass8 == 8 && nlines(CHK_, "pass", "room-seat") == 1 && alllines(CHK_) == 9);
    // ... and not seated: room-seat fails at its deadline (150 s), the rest pass
    play_well(mt_room_of(false, "1", true, false));
    CHECK(nlines(CHK_, "fail", "room-seat") == 1 && nlines(CHK_, "pass", "stable-5min") == 1 && alllines(CHK_) == 9);
}

int main(int argc, char **argv) {
    snprintf(FS.dir, sizeof FS.dir, "%s", DIR_);
    CHECK(mt_iso_ms(UNTIL_, &g_until) && g_until == 1794432402291LL);
    test_names_lines();
    test_paths();
    test_parse();
    test_run();
    test_selfdrive();
    test_pick_before_walk();
    test_pick_before_attack();
    test_pick_give_up_step();
    test_carried_is_not_moved();
    test_pick_stuck();
    test_stable();
    test_gone_second_run();
    test_bad_inputs();
    test_until_and_key();
    test_title();
    test_solo();
    const char *base = argc > 1 ? argv[1] : getenv("TMP");
    if (base) test_real_files(base);
    else printf("  (no scratch folder: real-file part skipped)\n");
    printf("melty_test_test: %d/%d\n", runs - fails, runs);
    return fails != 0;
}
