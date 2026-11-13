// host test for runtime/sora_area.h (s190, Melty spec 4.1): the pure part, on a made-up image of main's data.
//   clang -O1 -g -Wall -Wextra -DSORA_AREA_TEST -Iruntime runtime/test/area_test.c -o build_h/area_test.exe
#include <stdio.h>
#include "sora_area.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) fails++, printf("FAIL line %d: %s\n", __LINE__, #c); } while (0)

#define BASE 0x7001b7a000ull
static uint8_t img[0x4000];

// a FixedSafeString<cap> at off: vtable, top, cap, text
static void put(size_t off, uint64_t vt, int32_t cap, const char *text) {
    uint64_t top = BASE + off + 0x14;
    memcpy(img + off, &vt, 8), memcpy(img + off + 8, &top, 8), memcpy(img + off + 0x10, &cap, 4);
    memset(img + off + 0x14, 0, 32);
    memcpy(img + off + 0x14, text, strlen(text));
}

int main(void) {
    uint64_t c[4];
    char t[AREA_CAP];
    // nothing there
    CHECK(area_scan(img, BASE, sizeof img, c, 4) == 0);
    // the real shape (1.9.0 offsets inside the image do not matter): live pair first, the default pair later
    put(0x3a0, 0x7001bc4e70ull, 32, "MainField"), put(0x3d8, 0x7001bc4e70ull, 32, "F-6");
    put(0x1360, 0x7001bc4e70ull, 32, "MainField"), put(0x1398, 0x7001bc4e70ull, 32, "A-1");
    CHECK(area_scan(img, BASE, sizeof img, c, 4) == 2 && c[0] == BASE + 0x3a0 && c[1] == BASE + 0x1360);
    CHECK(area_scan(img, BASE, sizeof img, c, 1) == 2 && c[0] == BASE + 0x3a0);  // (counts past cap, writes cap)
    // not pairs: a lone map type, another class beside it, an empty name, a string of another size, an odd address
    put(0x2000, 0x7001bc4e70ull, 32, "MainField"), put(0x2038, 0x7001bc4e70ull, 32, "TitleMenu");  // (TitleMenu is a name here: a pair by shape)
    CHECK(area_scan(img, BASE, sizeof img, c, 4) == 3);
    put(0x2038, 0x7001bc46e0ull, 32, "TitleMenu");  // another vtable
    CHECK(area_scan(img, BASE, sizeof img, c, 4) == 2);
    put(0x2038, 0x7001bc4e70ull, 32, "");
    CHECK(area_scan(img, BASE, sizeof img, c, 4) == 2);
    put(0x2038, 0x7001bc4e70ull, 64, "F-6");
    CHECK(area_scan(img, BASE, sizeof img, c, 4) == 2);
    put(0x2000, 0x7001bc4e70ull, 32, "AppCamera"), put(0x2038, 0x7001bc4e70ull, 32, "F-6");  // not a map type
    CHECK(area_scan(img, BASE, sizeof img, c, 4) == 2);
    CHECK(!area_fss32(img, BASE, sizeof img, 0x3a4) && !area_fss32(img, BASE, sizeof img, sizeof img - 8));
    CHECK(area_fss32(img, BASE, sizeof img, 0x3a0) && !strcmp(area_fss32(img, BASE, sizeof img, 0x3d8), "F-6"));
    // a text with no end inside its 32 bytes, or with a control byte, is not one
    memset(img + 0x3d8 + 0x14, 'A', 32);
    CHECK(!area_fss32(img, BASE, sizeof img, 0x3d8));
    put(0x3d8, 0x7001bc4e70ull, 32, "F\x01" "6");
    CHECK(!area_fss32(img, BASE, sizeof img, 0x3d8));
    // the pair at the very end of the image fits; one byte less does not
    memset(img, 0, sizeof img);
    put(sizeof img - 0x70, 1, 32, "CDungeon"), put(sizeof img - 0x38, 1, 32, "Dungeon012");
    CHECK(area_scan(img, BASE, sizeof img, c, 4) == 1 && c[0] == BASE + sizeof img - 0x70);
    CHECK(area_scan(img, BASE, sizeof img - 8, c, 4) == 0);

    // the text: one name for the whole overworld, the map's own name elsewhere
    area_text("MainField", "F-6", t); CHECK(!strcmp(t, "MainField"));
    area_text("MainField", "A-1", t); CHECK(!strcmp(t, "MainField"));
    area_text("CDungeon", "Dungeon012", t); CHECK(!strcmp(t, "Dungeon012"));
    area_text("MainFieldDungeon", "RemainsWater", t); CHECK(!strcmp(t, "RemainsWater"));
    area_text("AocField", "", t); CHECK(!strcmp(t, "AocField"));
    area_text("", "F-6", t); CHECK(!strcmp(t, ""));
    area_text(NULL, NULL, t); CHECK(!strcmp(t, ""));
    area_text("CDungeon", "Dungeon 12", t); CHECK(!strcmp(t, ""));  // (a space: not a map name we pass on)
    area_text("CDungeon", "0123456789012345678901234567890", t); CHECK(strlen(t) == 31);
    area_text("CDungeon", "01234567890123456789012345678901", t); CHECK(!strcmp(t, ""));  // (32 letters: too long)
    printf("area_test: %d/%d\n", runs - fails, runs);
    return fails != 0;
}
