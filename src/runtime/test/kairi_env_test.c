// host test for runtime/kairi_env.h (s188 kairi_env.txt + the "started by Melty" rule):
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/kairi_env_test.c -o build_h/kairi_env_test.exe && build_h/kairi_env_test.exe
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kairi_env.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

// a fake environment: no process env touched
static struct { char k[KAIRI_ENV_MAX_KEY + 1]; char v[KAIRI_ENV_MAX_LINE + 1]; } E[64];
static int nE, nput;
static void env_reset(void) { nE = 0, nput = 0; }
static const char *fget(const char *k) {
    for (int i = 0; i < nE; i++) if (!strcmp(E[i].k, k)) return E[i].v;
    return NULL;
}
static int fput(const char *k, const char *v) {
    nput++;
    if (nE >= 64) return -1;
    snprintf(E[nE].k, sizeof E[nE].k, "%s", k);
    snprintf(E[nE].v, sizeof E[nE].v, "%s", v);
    nE++;
    return 0;
}
static int fput_fail(const char *k, const char *v) { (void)k, (void)v; return -1; }
static KairiEnvCounts run(const char *text) {
    KairiEnvCounts c = {0};
    kairi_env_apply(text, strlen(text), fget, fput, &c);
    return c;
}

static void test_basic(void) {
    env_reset();
    KairiEnvCounts c = run("SAVAGEWIND_SORA=1\nSORA_ROOT=D:/Kairi\n");
    CHECK(c.set == 2 && c.already == 0 && c.skipped == 0);
    CHECK(fget("SAVAGEWIND_SORA") && !strcmp(fget("SAVAGEWIND_SORA"), "1"));
    CHECK(fget("SORA_ROOT") && !strcmp(fget("SORA_ROOT"), "D:/Kairi"));
}

static void test_trim_crlf_comments_bom(void) {
    env_reset();
    KairiEnvCounts c = run("\xEF\xBB\xBF# a comment\r\n\r\n   \t\r\n  SAVAGEWIND_A  =  x y  \r\n\tSORA_B=v\t\r\n#SORA_C=no\r\n  # SORA_D=no\n");
    CHECK(c.set == 2 && c.already == 0 && c.skipped == 0);
    CHECK(fget("SAVAGEWIND_A") && !strcmp(fget("SAVAGEWIND_A"), "x y"));  // inner space kept, ends trimmed
    CHECK(fget("SORA_B") && !strcmp(fget("SORA_B"), "v"));
    CHECK(!fget("SORA_C") && !fget("SORA_D"));
    // a BOM key: without the skip the first key would be "\xEF\xBB\xBFSAVAGEWIND_X" and refused
    env_reset();
    c = run("\xEF\xBB\xBFSAVAGEWIND_X=1");  // (no newline at the end)
    CHECK(c.set == 1 && fget("SAVAGEWIND_X") && !strcmp(fget("SAVAGEWIND_X"), "1"));
    // a BOM later in the file is not skipped (only at the start)
    env_reset();
    c = run("SORA_A=1\n\xEF\xBB\xBFSORA_B=2\n");
    CHECK(c.set == 1 && c.skipped == 1 && !fget("SORA_B"));
}

static void test_real_env_wins(void) {
    env_reset();
    fput("SAVAGEWIND_SORA_NET", "0");  // (the launcher's own)
    nput = 0;
    KairiEnvCounts c = run("SAVAGEWIND_SORA_NET=1\nSAVAGEWIND_LINK_EDMG=1\n");
    CHECK(c.set == 1 && c.already == 1 && c.skipped == 0);
    CHECK(!strcmp(fget("SAVAGEWIND_SORA_NET"), "0"));
    CHECK(nput == 1);
    // the first line for a key wins
    env_reset();
    c = run("SORA_X=first\nSORA_X=second\n");
    CHECK(c.set == 1 && c.already == 1 && !strcmp(fget("SORA_X"), "first"));
}

static void test_hardening(void) {
    env_reset();
    KairiEnvCounts c = run("PATH=C:/evil\nMELTY_NET_DIR=x\nsavagewind_lower=1\nSAVAGEWIND_=1\nSORA_=1\nSAVAGEWIND\n"
                           "SAVAGEWIND_EMPTY=\nSAVAGEWIND_EMPTY2=   \nSAVAGE WIND_X=1\nSAVAGEWIND_A-B=1\nSAVAGEWIND_OK=1\n"
                           "=novalue\nSORA_Q=a=b\nSAVAGEWINDX=1\n");
    CHECK(!fget("PATH") && !fget("MELTY_NET_DIR") && !fget("savagewind_lower"));
    CHECK(!fget("SAVAGEWIND_EMPTY") && !fget("SAVAGEWIND_EMPTY2") && !fget("SAVAGEWIND_A-B"));
    CHECK(fget("SAVAGEWIND_OK") && fget("SORA_Q") && !strcmp(fget("SORA_Q"), "a=b"));  // (the first = splits)
    CHECK(c.set == 2 && c.already == 0 && c.skipped == 12);
    // a key that is too long
    char t[600];
    memset(t, 0, sizeof t);
    strcpy(t, "SORA_");
    memset(t + 5, 'K', 300);
    strcat(t, "=1\nSORA_SHORT=1\n");
    env_reset();
    c = run(t);
    CHECK(c.set == 1 && c.skipped == 1 && fget("SORA_SHORT"));
    // a NUL byte in a line: that line skipped, the rest read
    static const char nul[] = "SORA_A=x\0y\nSORA_B=2\n";
    env_reset();
    c = (KairiEnvCounts){0};
    kairi_env_apply(nul, sizeof nul - 1, fget, fput, &c);
    CHECK(c.set == 1 && c.skipped == 1 && !fget("SORA_A") && fget("SORA_B"));
    // put fails: counted as skipped
    c = (KairiEnvCounts){0};
    env_reset();
    kairi_env_apply("SORA_A=1\n", 9, fget, fput_fail, &c);
    CHECK(c.set == 0 && c.skipped == 1);
}

static void test_long_lines(void) {
    // a line exactly at the cap is read; one byte more is skipped, and the next line still read
    size_t cap = KAIRI_ENV_MAX_LINE;
    char *t = malloc(3 * cap + 64);
    strcpy(t, "SORA_LONG=");
    size_t n = strlen(t);
    memset(t + n, 'v', cap - n), n = cap;
    t[n++] = '\n';
    strcpy(t + n, "SORA_TOOLONG=");
    size_t m = n + strlen("SORA_TOOLONG=");
    memset(t + m, 'w', cap + 1 - (m - n)), m = n + cap + 1;
    t[m++] = '\n';
    strcpy(t + m, "SORA_AFTER=1\n");
    env_reset();
    KairiEnvCounts c = run(t);
    CHECK(c.set == 2 && c.skipped == 1);
    CHECK(fget("SORA_LONG") && strlen(fget("SORA_LONG")) == cap - strlen("SORA_LONG="));
    CHECK(!fget("SORA_TOOLONG") && fget("SORA_AFTER"));
    free(t);
    // an empty buffer / only newlines
    env_reset();
    c = run("");
    CHECK(c.set == 0 && c.already == 0 && c.skipped == 0);
    c = run("\n\n\r\n");
    CHECK(c.set == 0 && c.skipped == 0);
}

static void test_melty_rule(void) {
    env_reset();
    CHECK(!kairi_started_by_melty(fget));
    fput("MELTY_NET_DIR", "   ");
    CHECK(!kairi_started_by_melty(fget));  // blank
    env_reset();
    fput("MELTY_NET_DIR", "");
    CHECK(!kairi_started_by_melty(fget));
    env_reset();
    fput("MELTY_NET_DIR", "C:/melty/net");
    CHECK(kairi_started_by_melty(fget));
    fput("SAVAGEWIND_SORA_MELTY", "0");
    CHECK(!kairi_started_by_melty(fget));  // 0 overrides MELTY_NET_DIR
    env_reset();
    fput("SAVAGEWIND_SORA_MELTY", "1");
    CHECK(kairi_started_by_melty(fget));  // 1 without MELTY_NET_DIR
    env_reset();
    fput("SAVAGEWIND_SORA_MELTY", "yes");  // anything else: the MELTY_NET_DIR rule
    CHECK(!kairi_started_by_melty(fget));
    fput("MELTY_NET_DIR", "x");
    CHECK(kairi_started_by_melty(fget));
    // the file can set SAVAGEWIND_SORA_MELTY, but never MELTY_NET_DIR
    env_reset();
    run("MELTY_NET_DIR=x\n");
    CHECK(!kairi_started_by_melty(fget));
    run("SAVAGEWIND_SORA_MELTY=1\n");
    CHECK(kairi_started_by_melty(fget));
}

int main(void) {
    test_basic();
    test_trim_crlf_comments_bom();
    test_real_env_wins();
    test_hardening();
    test_long_lines();
    test_melty_rule();
    printf("kairi_env_test: %d checks, %d failed\n", runs, fails);
    return fails ? 1 : 0;
}
