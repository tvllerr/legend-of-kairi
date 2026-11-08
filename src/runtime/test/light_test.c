// host test for runtime/sora_light.h pure math: clang -Iruntime runtime/test/light_test.c -o build/light_test.exe
#define LIGHT_TEST_ONLY
#include <stdio.h>
#include <string.h>
#include "sora_light.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR(a, b) (fabsf((a) - (b)) < 0.02f)

int main(void) {
    float d[3], c[3], c2[3], p[16];
    // noon: 60 deg up, toward the south (+z), unit length
    light_sun_dir(12, 0, d);
    CHECK(NEAR(d[1], sinf(60 * 3.14159265f / 180)) && d[2] > 0.4f && fabsf(d[0]) < 0.05f);
    CHECK(NEAR(d[0] * d[0] + d[1] * d[1] + d[2] * d[2], 1));
    // 6:00 east side (+x), 18:00 west side (-x), both still above the horizon
    light_sun_dir(6, 0, d);
    CHECK(d[0] > 0.3f && d[1] > 0);
    light_sun_dir(18, 0, d);
    CHECK(d[0] < -0.3f && d[1] > 0);
    // night: moon from the west, high
    light_sun_dir(0, 0, d);
    CHECK(d[0] < -0.4f && d[1] > 0.6f);
    // wrap: 24.0 == 0.0, 25 == 1, -1 == 23
    light_sun_dir(24, 0, d), light_sun_dir(0, 0, c);
    CHECK(NEAR(d[0], c[0]) && NEAR(d[1], c[1]) && NEAR(d[2], c[2]));
    light_sun_rgb(25, c), light_sun_rgb(1, c2);
    CHECK(NEAR(c[0], c2[0]) && NEAR(c[2], c2[2]));
    light_sun_rgb(-1, c), light_sun_rgb(23, c2);
    CHECK(NEAR(c[0], c2[0]) && NEAR(c[1], c2[1]));
    // continuity across midnight and keyframes: no jump between 23.99 and 0.01
    light_sun_rgb(23.99f, c), light_sun_rgb(0.01f, c2);
    CHECK(NEAR(c[0], c2[0]) && NEAR(c[1], c2[1]) && NEAR(c[2], c2[2]));
    // key rows hit exactly; midpoints between
    light_sun_rgb(12, c);
    CHECK(NEAR(c[0], 1.05f) && NEAR(c[2], 0.92f));
    light_sun_rgb(5.5f, c);
    CHECK(NEAR(c[0], 1.15f) && NEAR(c[1], 0.70f));
    light_sun_rgb(4.75f, c);  // between night (0.22) and dawn (1.15): strictly between
    CHECK(c[0] > 0.3f && c[0] < 1.1f);
    // night is dim blue, day brighter and warm
    light_sun_rgb(2, c), light_sun_rgb(12, c2);
    CHECK(c[2] > c[0] && c[0] < c2[0] * 0.5f);
    // yaw 90 deg rotates the noon sun from +z to a horizontal axis
    light_sun_dir(12, 90, d);
    CHECK(fabsf(d[2]) < 0.05f && fabsf(d[0]) > 0.4f);
    // ambient: bottom darker than top
    light_sun_rgb(12, c);
    light_amb(c, d, p);
    CHECK(p[0] < d[0] && p[1] < d[1] && p[2] < d[2]);
    // weather mapping + rain dims the sun
    float rn, sn;
    light_weather(0, &rn, &sn);
    CHECK(rn == 0 && sn == 0);
    light_weather(3, &rn, &sn);
    CHECK(rn == 1 && sn == 0);
    light_weather(5, &rn, &sn);
    CHECK(rn == 0 && sn == 1);
    float a[16], b[16], in[16];
    light_compose(12, 0, 0, 0, 0, 1, 0.7f, 0.65f, a);
    light_compose(12, 0, 1, 0, 0, 1, 0.7f, 0.65f, b);
    CHECK(b[4] < a[4] * 0.6f && a[3] == 1 && NEAR(a[7], 0.7f) && NEAR(a[11], 0.65f) && a[15] == 0);
    // indoor ignores the hour: night vs noon identical at indoor = 1
    light_compose(0, 0, 0, 0, 1, 2, 0.7f, 0.65f, in);
    light_compose(12, 0, 0, 0, 1, 2, 0.7f, 0.65f, b);
    int same = 1;
    for (int i = 0; i < 16; i++) same &= NEAR(in[i], b[i]);
    CHECK(same && in[3] == 2);
    // (s123 a2 review) BotW words: dawn block -> unit dir toward the sun, style/amount/floor kept; bad words -> p untouched
    light_weather(7, &rn, &sn);
    CHECK(rn == 1 && sn == 0);
    float c4[144] = {0}, q[16];
    c4[92] = -0.801f, c4[93] = -0.459f, c4[94] = 0.384f, c4[19] = 4.79f;
    c4[20] = 4.79f, c4[21] = 4.33f, c4[22] = 3.83f, c4[40] = 0.554f, c4[41] = 0.557f, c4[42] = 0.341f;
    c4[130] = INFINITY;  // an unknown word: must not reject the block
    light_compose(6, 0, 0, 0, 0, 1, 0.7f, 0.65f, q);
    CHECK(light_from_botw(c4, 0.18f, 1.4f, 0.65f, q) && NEAR(q[0], 0.801f) && NEAR(q[1], 0.459f) && NEAR(q[4], 4.79f * 0.18f) &&
          NEAR(q[8], 0.554f * 1.4f) && NEAR(q[12], q[8] * 0.65f) && q[3] == 1 && NEAR(q[7], 0.7f) && NEAR(q[11], 0.65f));
    memcpy(b, q, sizeof b);
    c4[41] = NAN;
    CHECK(!light_from_botw(c4, 0.18f, 1.4f, 0.65f, q) && !memcmp(b, q, sizeof b));
    c4[41] = 0.557f, c4[93] = 0.459f;  // sun below the horizon
    CHECK(!light_from_botw(c4, 0.18f, 1.4f, 0.65f, q));
    float z4[144] = {0};  // zero-padded short block
    CHECK(!light_from_botw(z4, 0.18f, 1.4f, 0.65f, q));
    printf("light_test: %d checks, %d failed\n", runs, fails);
    return fails != 0;
}
