// host tests for runtime/sora_kb.h (pause menu slice 5 Equipment / Keyblades): sh D:/SoraBotW/recon/make_kb_test.sh
#include <stdio.h>
#include "sora_kb.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

int main(void) {
    int kb[KB_SLOTS], b[KB_SLOTS];
    kb_default(kb);  // default packs: main KK, Valor sub Star Seeker (today's look)
    CHECK(kb[KB_MAIN] == 0x29 && kb[KB_VALOR] == 0x1e0);
    CHECK(kb_pack(kb) == 0x01e00029u);
    CHECK(kb_load(0, 0xdeadbeef, b) == 0 && b[KB_MAIN] == 0x29 && b[KB_VALOR] == 0x1e0);
    // roundtrip
    kb[KB_MAIN] = 0x2b, kb[KB_VALOR] = 0x2a;
    CHECK(kb_unpack(kb_pack(kb), b) == 0 && b[KB_MAIN] == 0x2b && b[KB_VALOR] == 0x2a);
    // kb_bad_id: an id not in the table -> that slot back to its default
    CHECK(kb_unpack(0x01e00777u, b) == 1 && b[KB_MAIN] == 0x29 && b[KB_VALOR] == 0x1e0);
    CHECK(kb_unpack(0x0999002bu, b) == 1 && b[KB_MAIN] == 0x2b && b[KB_VALOR] == 0x1e0);
    CHECK(kb_unpack(0u, b) == 2 && b[KB_MAIN] == 0x29 && b[KB_VALOR] == 0x1e0);
    // kb_one_copy: main == Valor sub (hand edit) -> Valor gives way; kb_set refuses the other slot's Keyblade
    CHECK(kb_unpack(0x002a002au, b) == 1 && b[KB_MAIN] == 0x2a && b[KB_VALOR] == 0x1e0);
    CHECK(kb_unpack(0x01e001e0u, b) == 1 && b[KB_MAIN] == 0x1e0 && b[KB_VALOR] == 0x29);
    kb_default(kb);
    CHECK(kb_set(kb, KB_MAIN, 0x1e0) == -1 && kb[KB_MAIN] == 0x29);
    CHECK(kb_set(kb, KB_VALOR, 0x29) == -1 && kb[KB_VALOR] == 0x1e0);
    CHECK(!kb_offer(kb, KB_MAIN, 0x1e0) && kb_offer(kb, KB_MAIN, 0x2b) && !kb_offer(kb, KB_VALOR, 0x29));
    CHECK(kb_set(kb, KB_MAIN, 0x29) == 0);
    CHECK(kb_set(kb, KB_MAIN, 0x2b) == 1 && kb[KB_MAIN] == 0x2b);
    CHECK(kb_set(kb, KB_VALOR, 0x29) == 1 && kb[KB_VALOR] == 0x29);
    CHECK(kb_set(kb, KB_MAIN, 0x777) == -1 && kb_set(kb, 2, 0x29) == -1 && kb_set(kb, -1, 0x29) == -1);
    // stats per Keyblade (03system.bin), unknown id = Kingdom Key's
    CHECK(kb_str(0x29) == 3 && kb_mag(0x29) == 1 && kb_str(0x1e0) == 3 && kb_mag(0x1e0) == 1);
    CHECK(kb_str(0x2a) == 3 && kb_mag(0x2a) == 3 && kb_str(0x2b) == 6 && kb_mag(0x2b) == 2);
    CHECK(kb_str(0x777) == 3 && kb_mag(0x777) == 1);
    CHECK(kb_ability(0x1e0) == 163 && kb_ability(0x29) == 542 && kb_ability(0x777) == 0);
    // alt ids = the bake's (sorabake SORABAKE_KB_ID / _ALT: 1 KK, 2 Star Seeker, 3 Oathkeeper, 4 Oblivion)
    CHECK(kb_alt(0x29) == 1 && kb_alt(0x1e0) == 2 && kb_alt(0x2a) == 3 && kb_alt(0x2b) == 4 && kb_alt(0x777) == 0);
    // (s89 kb) every KH2 FM Keyblade: 24 rows, 03system.bin stats / abilities, alt ids in table order, text keys
    CHECK(KB_N == 24 && kb_alt(0x1e1) == 5 && kb_alt(0x220) == 24 && kb_index(0x1f3) == 20);
    CHECK(kb_str(0x1f3) == 7 && kb_mag(0x1f3) == 1 && kb_ability(0x1f3) == 394);   // Fenrir
    CHECK(kb_str(0x1f4) == 6 && kb_mag(0x1f4) == 4 && kb_ability(0x1f4) == 422);   // Ultima Weapon
    CHECK(kb_str(0x1ef) == 0 && kb_mag(0x1ef) == 4 && kb_ability(0x1ef) == 540);   // Sweet Memories
    CHECK(kb_str(0x220) == 5 && kb_mag(0x220) == 7 && kb_ability(0x220) == 404);   // Winner's Proof
    for (int i = 0; i < KB_N; i++)
        for (int j = i + 1; j < KB_N; j++) CHECK(KB_TAB[i].id != KB_TAB[j].id && kb_text_key(i) != kb_text_key(j));
    CHECK(kb_text_key(0) == 166 && kb_text_key(3) == 172 && kb_text_key(4) == 400 && kb_text_key(23) == 438);
    // owned set: KK + SS always, the equipped ones forced in, the list in table order
    kb[KB_MAIN] = 0x1f4, kb[KB_VALOR] = 0x2b;
    CHECK(kb_own_fix(0, kb) == (0x3u | 1u << 21 | 1u << 3));
    CHECK(kb_owned(kb_own_fix(0, kb), 0x1f4) && !kb_owned(kb_own_fix(0, kb), 0x1f3) && kb_own_bit(0x777) == 0);
    CHECK(kb_own_fix(0xffffffffu, kb) == 0xffffffu);
    int l[KB_N];
    CHECK(kb_own_list(kb_own_fix(1u << 10, kb), l) == 5 && l[0] == 0 && l[1] == 1 && l[2] == 3 && l[3] == 10 && l[4] == 21);
    // (s91 mfkb) Master / Final slots: 4 slots, one copy across all, sidecar "keyblade2", defaults, per-form slot
    CHECK(KB_SLOTS == 4 && kb_sub_slot(3) == KB_MASTER && kb_sub_slot(4) == KB_FINAL && kb_sub_slot(0) == KB_VALOR && kb_sub_slot(2) == KB_VALOR);
    kb_default(kb);
    CHECK(kb[KB_MASTER] == 0 && kb[KB_FINAL] == 0 && kb_pack(kb) == 0x01e00029u && kb_pack2(kb) == 0);
    CHECK(kb_sub_id(kb, 3) == 0x1e0 && kb_slot_id(kb, KB_FINAL) == 0x1e0);  // auto = Valor's
    CHECK(kb_set(kb, KB_MASTER, 0x2a) == 1 && kb_set(kb, KB_FINAL, 0x2a) == -1 && kb_set(kb, KB_FINAL, 0x1e0) == -1 && kb_set(kb, KB_MAIN, 0x2a) == -1);
    CHECK(kb_set(kb, KB_FINAL, 0x2b) == 1 && kb_sub_id(kb, 3) == 0x2a && kb_sub_id(kb, 4) == 0x2b && kb_sub_id(kb, 0) == 0x1e0);
    CHECK(!kb_offer(kb, KB_VALOR, 0x2b) && kb_offer(kb, KB_FINAL, 0x2b) && kb_offer(kb, KB_MASTER, 0x2b) == 0 && kb_offer(kb, KB_MASTER, 0x2a));
    g_mfkb = 0;
    CHECK(kb_sub_id(kb, 3) == 0x1e0 && kb_sub_id(kb, 4) == 0x1e0);  // set mfkb 0 = the old behaviour
    g_mfkb = 1;
    CHECK(kb_pack2(kb) == (0x2au | 0x2bu << 16) && kb_unpack2(kb_pack2(kb), b) == 0 && b[KB_MASTER] == 0x2a && b[KB_FINAL] == 0x2b);
    kb_default(b), b[KB_VALOR] = 0x2a;
    CHECK(kb_unpack2(0x2au | 0x777u << 16, b) == 2 || b[KB_FINAL] == 0);  // bad id -> auto
    kb_default(b), b[KB_MASTER] = 0x2b;
    CHECK(kb_unpack2(0x2bu | 0x2bu << 16, b) == 1 && b[KB_MASTER] == 0x2b && b[KB_FINAL] == 0);  // dup of an earlier slot
    // old save: unpack leaves Master / Final 0; defaults need OWNED + unequipped Keyblades
    CHECK(kb_unpack(0x01e00029u, b) == 0 && b[KB_MASTER] == 0 && b[KB_FINAL] == 0);
    CHECK(kb_form_defaults(b, 0x3u) == 0 && b[KB_MASTER] == 0);  // only KK + SS owned, both equipped: stays auto
    CHECK(kb_form_defaults(b, 0x3u | 1u << 2 | 1u << 3) == 2 && b[KB_MASTER] == 0x2a && b[KB_FINAL] == 0x2b);  // Oathkeeper / Oblivion
    kb_unpack(0x01e00029u, b);
    CHECK(kb_form_defaults(b, 0x3u | 1u << 3 | 1u << 10) == 2 && b[KB_MASTER] == 0x2b && b[KB_FINAL] == 0x1e9);  // Oblivion preferred, else first free
    CHECK(kb_form_defaults(b, 0xffffffu) == 0);  // never replaces a pick
    kb[KB_MAIN] = 0x29, kb[KB_VALOR] = 0x1e0, kb[KB_MASTER] = 0x2a, kb[KB_FINAL] = 0x2b;
    CHECK(kb_own_fix(0, kb) == (0x3u | 1u << 2 | 1u << 3));  // equipped form blades count as owned
    // (s118) the user's save: KK main, SS Valor, Guardian Soul (Master) + Hidden Dragon (Final) parked in LOCKED slots
    kb[KB_MAIN] = 0x29, kb[KB_VALOR] = 0x1e0, kb[KB_MASTER] = 0x1eb, kb[KB_FINAL] = 0x1e1;
    CHECK(kb_lock_forms(kb, 0) == 2 && kb[KB_MASTER] == 0 && kb[KB_FINAL] == 0 && kb_offer(kb, KB_MAIN, 0x1eb) && kb_offer(kb, KB_MAIN, 0x1e1));
    CHECK(kb_form_defaults_m(kb, 0x1013u, 0) == 0 && kb[KB_MASTER] == 0);  // locked forms are never filled
    CHECK(kb_form_defaults_m(kb, 0x1013u, 1) == 1 && kb[KB_MASTER] == 0x1e1 && kb[KB_FINAL] == 0);  // Master open: first free owned
    CHECK(kb_lock_forms(kb, 3) == 0 && kb[KB_MASTER] == 0x1e1);  // an open form keeps its pick
    // swap: main picks Valor's Star Seeker -> Valor gets the Kingdom Key
    CHECK(kb_holder(kb, KB_MAIN, 0x1e0) == KB_VALOR && kb_swap(kb, KB_MAIN, 0x1e0) == 1 && kb[KB_MAIN] == 0x1e0 && kb[KB_VALOR] == 0x29);
    CHECK(kb_swap(kb, KB_MAIN, 0x2b) == -1);  // nobody holds it: not a swap
    kb[KB_FINAL] = 0;
    CHECK(kb_swap(kb, KB_FINAL, 0x29) == -1 && kb[KB_VALOR] == 0x29);  // an auto (0) slot has nothing to give back
    g_kbswap = 0;
    CHECK(kb_swap(kb, KB_VALOR, 0x1e0) == -1 && kb[KB_VALOR] == 0x29);  // set kbswap 0 = the old error
    g_kbswap = 1;
    printf("kb_test: %d/%d passed\n", runs - fails, runs);
    return fails != 0;
}
