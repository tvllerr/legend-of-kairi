// armor_test.c (s120): sora_armor.h armor_rows (pure). Build: clang -I.. armor_test.c -o armor_test.exe
#define SORA_ARMOR_PURE
#include "../sora_armor.h"
static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)
int main(void) {
    for (int i = 1; i < SORA_ARMOR_N; i++) CHECK(strcmp(SORA_ARMOR[i - 1].actor, SORA_ARMOR[i].actor) < 0);  // bsearch order
    char nm[3][24]; int df[3];
    const char w1[4][40] = {"Armor_001_Lower", "Armor_Default_Extra_00", "Armor_002_Head", "Armor_060_Upper"};
    int n = armor_rows(w1, 4, nm, df);
    CHECK(n == 3);
    CHECK(!strcmp(nm[0], "Hylian Hood") && df[0] == 5);
    CHECK(!strcmp(nm[1], "Tunic of the Wild") && df[1] == 18);
    CHECK(!strcmp(nm[2], "Hylian Trousers") && df[2] == 3);
    const char w2[1][40] = {"Armor_001_Head_B"};
    CHECK(armor_rows(w2, 1, nm, df) == 1 && !strcmp(nm[0], "Hylian Hood") && df[0] == 3);
    CHECK(armor_rows(w2, 0, nm, df) == 0);
    printf("armor_test %d/%d\n", runs - fails, runs);
    return fails != 0;
}
