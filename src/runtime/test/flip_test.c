// flip_test.c (s290 flip): the room keys the user turned ON by default after the two-copy lab ("Flip the passes ON",
// sb/s290/LAB_RESULTS.md) keep a compiled default of 1 / true. Most of them live in glue headers no host test can include
// (sora_duel.h, sora_revive.h, sora_mgelem.h, the loot / objs glue): so this reads each header's source and checks the one
// declaration's initializer, and that the header still parses the live key ("set <key> 0" turns it off). netgear / netshots /
// netrunes / roomnoauto are also checked at run time in netgear_test.c, netshots_test.c and noauto_test.c.
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/flip_test.c -o build_h/flip_test.exe && build_h/flip_test.exe <repo>/runtime
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

static int runs, fails;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static char *slurp(const char *dir, const char *name) {
    char p[1024];
    snprintf(p, sizeof p, "%s/%s", dir, name);
    FILE *f = fopen(p, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *b = (char *)malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    size_t got = fread(b, 1, (size_t)n, f);
    fclose(f);
    b[got] = 0;
    return b;
}

static bool ident(char c) { return isalnum((unsigned char)c) || c == '_'; }

// the initializer of `var` in a `static ...` declaration line (`static int a = 1, b;` / `static bool v = true;`) -> into out;
// returns how many such declarations the file has (1 = good). No initializer = "0" (a static's zero).
static int decl_init(const char *src, const char *var, char *out, size_t cap) {
    int n = 0;
    size_t vl = strlen(var);
    out[0] = 0;
    for (const char *p = strstr(src, var); p; p = strstr(p + 1, var)) {
        if ((p > src && ident(p[-1])) || ident(p[vl])) continue;
        const char *ls = p;
        while (ls > src && ls[-1] != '\n') ls--;
        if (strncmp(ls, "static ", 7)) continue;  // (only a file-scope static declaration line)
        bool decl = true;  // (before the name: only the type and earlier names, no call / body / initializer)
        for (const char *t = ls; t < p && decl; t++) decl = *t != '(' && *t != '{' && *t != '=' && *t != '/';
        if (!decl) continue;
        const char *semi = strchr(ls, ';');
        if (!semi || semi < p) continue;
        const char *q = p + vl;
        while (*q == ' ') q++;
        if (*q == '(') continue;  // (a function of that name)
        n++;
        if (*q == '=' && q[1] != '=') {
            q++;
            while (*q == ' ') q++;
            size_t k = 0;
            while (*q && *q != ',' && *q != ';' && *q != ' ' && k + 1 < cap) out[k++] = *q++;
            out[k] = 0;
        } else if (*q == ',' || *q == ';') snprintf(out, cap, "0");
    }
    return n;
}

// parse = the header with the live key's handler when it is not the declaring one (duelgod: declared in sora_net_duel.h)
static void key_on_in(const char *dir, const char *file, const char *key, const char *var, const char *want, const char *parse) {
    char *src = slurp(dir, file);
    CHECK(src != NULL);
    if (!src) { printf("  (cannot read %s/%s)\n", dir, file); return; }
    char init[64];
    int n = decl_init(src, var, init, sizeof init);
    bool ok = n == 1 && !strcmp(init, want);
    CHECK(ok);
    char quoted[64];
    snprintf(quoted, sizeof quoted, "\"%s\"", key);
    char *ps = parse ? slurp(dir, parse) : src;
    bool parsed = ps && strstr(ps, quoted) != NULL;  // (the live "set <key> v" handler: its strcmp / NO_KEYIS literal)
    if (ps && ps != src) free(ps);
    CHECK(parsed);
    printf("  %-12s %-20s %-6s %s\n", key, file, init, ok && parsed ? "default ON" : n != 1 ? "declaration not found once" : "NOT ON");
    free(src);
}

static void key_on(const char *dir, const char *file, const char *key, const char *var, const char *want) { key_on_in(dir, file, key, var, want, NULL); }

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "runtime";
    // one test per key (the user's list, s290)
    key_on_in(dir, "sora_net_duel.h", "duelgod", "g_du_god", "true", "sora_duel.h");
    key_on(dir, "sora_duel.h", "duelthrow", "g_du_throw", "1");
    key_on(dir, "sora_duel.h", "duelreflect", "g_du_reflect", "1");
    key_on(dir, "sora_duelmenu.h", "duelmenu", "g_dum", "1");
    key_on(dir, "sora_mgelem.h", "mgelemclaim", "g_mgelem_claim", "1");
    key_on(dir, "sora_duel.h", "duelmagnet", "g_du_magnet", "1");
    key_on(dir, "sora_net_noauto.h", "roomnoauto", "g_net_noauto", "true");
    key_on(dir, "sora_net_gear.h", "netgear", "g_net_gear", "1");
    key_on(dir, "sora_revive.h", "revive", "g_revive", "true");
    key_on(dir, "sora_net_loot.h", "netloot", "g_net_loot", "true");
    key_on(dir, "sora_net_shots.h", "netshots", "g_net_shots", "1");
    key_on(dir, "sora_net_shots.h", "netrunes", "g_net_runes", "1");
    key_on(dir, "sora_net_objs.h", "netobjs", "g_net_objs", "1");
    key_on(dir, "sora_net_objs.h", "nethorse", "g_net_horse", "1");  // (s291)
    key_on(dir, "sora_nopause.h", "netnopause", "g_np_key", "1");  // (s291)
    key_on(dir, "sora_slate.h", "slateeata", "g_sk_eat_a", "1");  // (s292)
    key_on(dir, "sora_slate.h", "slatekeep", "g_sk_keep", "1");  // (s292)
    key_on(dir, "sora_carry.h", "carryheld", "g_carry_held", "1");  // (s292)
    key_on(dir, "sora.c", "malnoflinch", "g_malnoflinch", "1");  // (s292 release audit)
    // the room line for the ones whose live "on" step checked the exe (sora_net_flip.h), hooked once in sora_net.h
    {
        char *net = slurp(dir, "sora_net.h");
        CHECK(net != NULL);
        if (net) {
            CHECK(strstr(net, "#include \"sora_net_flip.h\"") != NULL);
            const char *c = strstr(net, "    net_flip_frame();");
            CHECK(c != NULL && strstr(c + 1, "    net_flip_frame();") == NULL);
            free(net);
        }
    }
    // the parser itself (a declaration with no initializer = 0; a second variable on the line; a function of the same name)
    {
        char o[16];
        CHECK(decl_init("static int g_a, g_b = 1;\n", "g_a", o, sizeof o) == 1 && !strcmp(o, "0"));
        CHECK(decl_init("static int g_a, g_b = 1;\n", "g_b", o, sizeof o) == 1 && !strcmp(o, "1"));
        CHECK(decl_init("static int g_x = 1, g_x_lift = 0;\nstatic void g_x(void);\n", "g_x", o, sizeof o) == 1 && !strcmp(o, "1"));
        CHECK(decl_init("    g_x = 0;\nstatic bool g_x = true;  // g_x\n", "g_x", o, sizeof o) == 1 && !strcmp(o, "true"));
        CHECK(decl_init("static bool g_x;\nstatic bool f(void) { return h(g_x, 1); }\n", "g_x", o, sizeof o) == 1 && !strcmp(o, "0"));
    }
    printf("flip_test: %d / %d checks pass%s\n", runs - fails, runs, fails ? " - FAIL" : "");
    return fails != 0;
}
