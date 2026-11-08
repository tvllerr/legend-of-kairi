// smoke test of runtime/sora_esa_samp.h (Windows): sample a busy thread, report, stop. clang -O1 -g -Iruntime runtime/test/esf_test.c -o build_h/esf_test.exe
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#define LOG printf
#include "gamelib.h"
const GameLibTables *gamelib_tables(void) { return NULL; }
#include "sora_esa_samp.h"
static volatile double sink;
static void busy(void) { for (int i = 0; i < 3000000; i++) sink += i * 0.5; }
int main(void) {
    int fails = 0;
    for (int k = 0; k < 3; k++) {
        EsfSlot *s = esf_begin(true);
        if (!s) { printf("FAIL no slot\n"); return 1; }
        ULONGLONG t0 = GetTickCount64();
        while (GetTickCount64() - t0 < 120) busy();
        esf_end(s, 120);
    }
    printf("samples %u slow %u\n", g_esf.samples, g_esf.slow_calls);
    if (g_esf.samples < 10 || g_esf.slow_calls != 3) fails++;
    esf_report();
    esf_stop();
    if (g_esf.th) fails++;
    EsfSlot *s = esf_begin(true);
    if (s) fails++;  // dead after stop
    printf("%s\n", fails ? "FAILED" : "ok");
    return fails;
}
