// (s289 b9fix) botwpic's one-picture state (sora_hud_bi.h): the DLL's asks, the render thread's take / done / fail.
// clang -O1 -g -Wall -Wno-unused-function -Iruntime runtime/test/hud_bi_test.c -o hud_bi.exe
#include <stdio.h>
#include "sora_hud_bi.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static uint8_t *pic_(void) { return (uint8_t *)calloc(16, 1); }
// one full ask -> decode (as sora_hud_botw_icon) -> answer
static int ask_(BiState *b, const char *a, int has_pic) {
    int r = bi_ask_(b, a);
    if (r != 2) return r;
    uint8_t *px = has_pic ? pic_() : NULL;
    bi_decoded_(b, a, px, 2, 2);
    return px ? 0 : -1;
}
// the render thread's whole upload of the waiting picture (ok = the image was made)
static void upload_(BiState *b, int ok) {
    char n[72];
    int w, h;
    uint8_t *px = bi_take_(b, n, &w, &h);
    if (!px) return;
    free(px);
    if (ok) bi_done_(b, n); else bi_fail_(b, n);
}

static void test_basic(void) {
    BiState b = {0};
    CHECK(ask_(&b, "Weapon_Sword_001", 1) == 0);
    CHECK(ask_(&b, "Weapon_Sword_001", 1) == 0);  // decoded, not uploaded
    upload_(&b, 1);
    CHECK(ask_(&b, "Weapon_Sword_001", 1) == 1 && !strcmp(b.have, "Weapon_Sword_001"));
    CHECK(ask_(&b, "Armor_001_Head", 0) == -1 && ask_(&b, "Armor_001_Head", 0) == -1);  // no picture: -1, no decode again
    CHECK(!b.pend && b.px == NULL);
    char n[72]; int w, h;
    CHECK(bi_take_(&b, n, &w, &h) == NULL && !strcmp(b.have, "Weapon_Sword_001"));  // nothing waits: have kept
}

// (a) during the upload of B, a request for the old picture A answers "not uploaded" (0), never 1
static void test_take_clears_have(void) {
    BiState b = {0};
    ask_(&b, "A", 1), upload_(&b, 1);
    CHECK(bi_ask_(&b, "A") == 1);
    CHECK(ask_(&b, "B", 1) == 0);
    char n[72]; int w, h;
    uint8_t *px = bi_take_(&b, n, &w, &h);  // the render thread holds B, texture 15 about to be overwritten
    CHECK(px && !strcmp(n, "B") && b.have[0] == 0 && !b.pend);
    CHECK(ask_(&b, "A", 1) == 0);  // was 1 before the fix: A's quad over B's pixels
    CHECK(ask_(&b, "B", 1) == 0);  // B asked again mid-upload: decoded again (want was A), not 1
    free(px);
    bi_done_(&b, "B");
    CHECK(bi_ask_(&b, "B") == 1);  // B is in texture 15 now
}

// (b) the image could not be made: no picture, the name answers -1 (not 1 or 0 forever)
static void test_fail(void) {
    BiState b = {0};
    ask_(&b, "A", 1), upload_(&b, 1);
    CHECK(ask_(&b, "B", 1) == 0);
    upload_(&b, 0);
    CHECK(b.have[0] == 0 && b.failed == 1);
    CHECK(bi_ask_(&b, "B") == -1 && bi_ask_(&b, "B") == -1);
    CHECK(ask_(&b, "A", 1) == 0);  // A is gone from texture 15 too: decoded again
    upload_(&b, 1);
    CHECK(bi_ask_(&b, "A") == 1);
}

// (b) a newer picture already waiting when the older one fails keeps its own state (0, then uploaded)
static void test_fail_keeps_newer(void) {
    BiState b = {0};
    CHECK(ask_(&b, "A", 1) == 0);
    char n[72]; int w, h;
    uint8_t *px = bi_take_(&b, n, &w, &h);
    CHECK(ask_(&b, "C", 1) == 0);  // C decoded while A uploads
    free(px);
    bi_fail_(&b, n);
    CHECK(!b.failed && b.pend && bi_ask_(&b, "C") == 0);
    upload_(&b, 1);
    CHECK(bi_ask_(&b, "C") == 1);
}

// (s288 port) back to the uploaded picture while another waits: the waiting one is dropped
static void test_drop_pending(void) {
    BiState b = {0};
    ask_(&b, "A", 1), upload_(&b, 1);
    CHECK(ask_(&b, "B", 1) == 0 && b.pend);
    CHECK(bi_ask_(&b, "A") == 1 && !b.pend && b.px == NULL && !strcmp(b.want, "A"));
    upload_(&b, 1);  // nothing waits
    CHECK(bi_ask_(&b, "A") == 1);
}

int main(void) {
    test_basic();
    test_take_clears_have();
    test_fail();
    test_fail_keeps_newer();
    test_drop_pending();
    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
