#include "sora_ragmap.h"
#include "sora_ragmap_roxas.h"
#include "sora_ragmap_mickey.h"
static void dump(const char *p, int nb, const int16_t *par, const float (*pos)[3], const SoraRagBone *m, int nm) {
    FILE *f = fopen(p, "wb"); uint32_t h[4] = {0x47415253u, 1, nb, nm}; fwrite(h, 16, 1, f);
    fwrite(par, 2, nb, f); int pad = (4 - (nb * 2) % 4) % 4; fwrite("\0\0\0", 1, pad, f);
    fwrite(pos, 12, nb, f);
    for (int i = 0; i < nm; i++) { fwrite(m[i].bind_dir, 12, 1, f); fwrite(&m[i].bind_len, 4, 1, f); }
    fclose(f);
}
#define LD(P, N, PAR, POS, MAP, NM) do { const char *w; int ok = sora_ragmap_load(P, N, PAR, POS, MAP, NM, &w); printf("%s: ok %d (%s)\n", P, ok, ok ? "" : w); if (!ok) bad = 1; } while (0)
int main(int argc, char **argv) {
    int bad = 0; const char *d = argv[1]; char p[512];
    snprintf(p, sizeof p, "%s/sora.bin", d); LD(p, SORA_RAG_BONES, sora_rag_parent, sora_rag_bind_pos, sora_ragmap, SORA_RAGMAP_N);
    snprintf(p, sizeof p, "%s/roxas.bin", d); LD(p, ROXAS_RAG_BONES, rx_rag_parent, rx_rag_bind_pos, sora_ragmap_roxas, ROXAS_RAGMAP_N);
    snprintf(p, sizeof p, "%s/mickey.bin", d); LD(p, MICKEY_RAG_BONES, mk_rag_parent, mk_rag_bind_pos, sora_ragmap_mickey, MICKEY_RAGMAP_N);
    dump("new_sora.bin", SORA_RAG_BONES, sora_rag_parent, sora_rag_bind_pos, sora_ragmap, SORA_RAGMAP_N);
    dump("new_roxas.bin", ROXAS_RAG_BONES, rx_rag_parent, rx_rag_bind_pos, sora_ragmap_roxas, ROXAS_RAGMAP_N);
    dump("new_mickey.bin", MICKEY_RAG_BONES, mk_rag_parent, mk_rag_bind_pos, sora_ragmap_mickey, MICKEY_RAGMAP_N);
    /* failure modes */
    const char *w; snprintf(p, sizeof p, "%s/nope.bin", d);
    printf("missing: %d (%s)\n", sora_ragmap_load(p, SORA_RAG_BONES, sora_rag_parent, sora_rag_bind_pos, sora_ragmap, SORA_RAGMAP_N, &w), w);
    snprintf(p, sizeof p, "%s/roxas.bin", d);
    printf("wrong rig: %d (%s)\n", sora_ragmap_load(p, SORA_RAG_BONES, sora_rag_parent, sora_rag_bind_pos, sora_ragmap, SORA_RAGMAP_N, &w), w);
    return bad;
}
