#include "old/sora_ragmap.h"
#include "old/sora_ragmap_roxas.h"
#include "old/sora_ragmap_mickey.h"
#include <stdio.h>
#include <string.h>
static void dump(const char *p, int nb, const int16_t *par, const float (*pos)[3], const SoraRagBone *m, int nm) {
    FILE *f = fopen(p, "wb"); uint32_t h[4] = {0x47415253u, 1, nb, nm}; fwrite(h, 16, 1, f);
    fwrite(par, 2, nb, f); int pad = (4 - (nb * 2) % 4) % 4; fwrite("\0\0\0", 1, pad, f);
    fwrite(pos, 12, nb, f);
    for (int i = 0; i < nm; i++) { fwrite(m[i].bind_dir, 12, 1, f); fwrite(&m[i].bind_len, 4, 1, f); }
    fclose(f);
}
int main(void) {
    dump("old_sora.bin", SORA_RAG_BONES, sora_rag_parent, sora_rag_bind_pos, sora_ragmap, SORA_RAGMAP_N);
    dump("old_roxas.bin", ROXAS_RAG_BONES, rx_rag_parent, rx_rag_bind_pos, sora_ragmap_roxas, ROXAS_RAGMAP_N);
    dump("old_mickey.bin", MICKEY_RAG_BONES, mk_rag_parent, mk_rag_bind_pos, sora_ragmap_mickey, MICKEY_RAGMAP_N);
    return 0;
}
