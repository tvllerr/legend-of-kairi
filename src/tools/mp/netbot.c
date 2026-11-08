// netbot: a fake multiplayer peer for the Sora-in-BotW net protocol (host exe, pure C; no game includes).
//   netbot record <port> <file> [--name N] [--secs S]
//       listens on <port> (the game pins it: `net peer 127.0.0.1:<port>`), answers HELLOs, writes every SNAP of the first
//       player it hears (arrival ms + raw packet) to <file>.
//   netbot play <file> --port P --name N --peer ip[:port] [--peer ...] [--offset x,z] [--loop] [--secs S]
//       own random player id, own seq numbers and event seqs; replays the recorded SNAPs at their timing, everything that is
//       a WORLD place shifted by the offset (BotW units; 1 u = 1 m while the mod's g_k is 1); HELLO every second, BYE at the
//       end / Ctrl+C. Also answers HELLOs from anyone (learns them as targets, up to 8).
//   netbot dump <file>     prints the recorded SNAPs in text (the host test reads it).
// Packing / unpacking is only done with sora_net_proto.h (NET_VERSION / NET_MAX_EVENTS are whatever that header says).
// Build: tools/mp/make_netbot.sh -> build_mp4/netbot.exe
#include "../../runtime/sora_net_proto.h"
#include "../../runtime/sora_net_sock.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <unistd.h>
#endif

static volatile int g_stop;
static const char *g_stopfile;  // --stopfile F: the file appears = stop cleanly (BYE), for scripts that cannot send Ctrl+C
static void check_stopfile(void) {
    if (!g_stopfile) return;
    FILE *f = fopen(g_stopfile, "rb");
    if (f) fclose(f), g_stop = 1;
}
static void on_sig(int s) { (void)s; g_stop = 1; }

static uint64_t now_ms(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000 + (uint64_t)t.tv_nsec / 1000000;
#endif
}
static void nap(int ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)ms * 1000);
#endif
}

typedef struct { uint32_t ip; uint16_t port; bool fixed; } Target;
static Target g_tg[8];
static int g_ntg;
static NetSock g_sock;
static uint8_t g_me;
static uint16_t g_seq;
static char g_name[NET_NAME_MAX + 1] = "Bot";

static bool add_target(uint32_t ip, uint16_t port, bool fixed) {
    for (int i = 0; i < g_ntg; i++) if (g_tg[i].ip == ip && g_tg[i].port == port) return false;
    if (g_ntg == 8) return false;
    g_tg[g_ntg++] = (Target){ip, port, fixed};
    return true;
}
static bool parse_peer(const char *s, uint32_t *ip, uint16_t *port) {
    char b[64];
    strncpy(b, s, sizeof b - 1), b[sizeof b - 1] = 0;
    *port = NET_PORT;
    char *c = strchr(b, ':');
    if (c) *c = 0, *port = (uint16_t)atoi(c + 1);
    return nsock_parse_ip(b, ip) && *port;
}
static void send_all(const uint8_t *b, int n) {
    for (int i = 0; i < g_ntg; i++) nsock_send(&g_sock, g_tg[i].ip, g_tg[i].port, b, n);
}
static void send_to(uint32_t ip, uint16_t port, const uint8_t *b, int n) { nsock_send(&g_sock, ip, port, b, n); }

// THE ONE PLACE THAT BUILDS A HELLO. v5: header + name + an EMPTY peer list, on purpose: the bots never gossip (the game does
// that for them: it lists the bots it has met to its other peers), and an empty list is always valid.
static int bot_hello(uint8_t *out, int cap) { return net_pack_hello(g_me, ++g_seq, g_name, out, cap); }
static void send_hello_all(void) {
    uint8_t b[NET_PKT_MAX];
    int n = bot_hello(b, sizeof b);
    if (n > 0) send_all(b, n);
}
static void send_hello_to(uint32_t ip, uint16_t port) {
    uint8_t b[NET_PKT_MAX];
    int n = bot_hello(b, sizeof b);
    if (n > 0) send_to(ip, port, b, n);
}
static void send_bye_all(void) {
    uint8_t b[NET_PKT_MAX];
    int n = net_pack_bye(g_me, ++g_seq, b, sizeof b);
    if (n > 0) for (int k = 0; k < 3; k++) send_all(b, n), nap(10);
}

static int open_sock(uint16_t port) {
    if (!nsock_open(&g_sock, port, false)) { fprintf(stderr, "netbot: cannot bind UDP port %u\n", port); return 0; }
    return 1;
}
static void rand_id(void) {
    uint32_t s = (uint32_t)now_ms() * 2654435761u;
#ifdef _WIN32
    s ^= (uint32_t)GetCurrentProcessId() * 40503u;
#else
    s ^= (uint32_t)getpid() * 40503u;
#endif
    g_me = (uint8_t)(1 + (s >> 8) % 254);
}

// ---- record file: "NBR1", then { u32 arrival ms, u16 len, bytes } ----
static void put32(FILE *f, uint32_t v) { for (int i = 0; i < 4; i++) fputc((int)(v >> (8 * i) & 255), f); }
static void put16(FILE *f, uint16_t v) { fputc(v & 255, f), fputc(v >> 8, f); }
static int get32(FILE *f, uint32_t *v) {
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) return 0;
    *v = (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
    return 1;
}
static int get16(FILE *f, uint16_t *v) {
    uint8_t b[2];
    if (fread(b, 1, 2, f) != 2) return 0;
    *v = (uint16_t)(b[0] | b[1] << 8);
    return 1;
}

typedef struct { uint32_t at; uint16_t len; uint8_t d[NET_PKT_MAX]; } Rec;
static Rec *load_rec(const char *path, int *n) {
    FILE *f = fopen(path, "rb");
    *n = 0;
    if (!f) { fprintf(stderr, "netbot: cannot open %s\n", path); return NULL; }
    char m[4];
    if (fread(m, 1, 4, f) != 4 || memcmp(m, "NBR1", 4)) { fprintf(stderr, "netbot: %s is not a netbot record\n", path); fclose(f); return NULL; }
    int cap = 256;
    Rec *r = malloc(sizeof *r * (size_t)cap);
    uint32_t at; uint16_t len;
    while (get32(f, &at) && get16(f, &len)) {
        if (len > NET_PKT_MAX) break;
        if (*n == cap) cap *= 2, r = realloc(r, sizeof *r * (size_t)cap);
        r[*n].at = at, r[*n].len = len;
        if (fread(r[*n].d, 1, len, f) != len) break;
        (*n)++;
    }
    fclose(f);
    return r;
}

// ---- record ----
static int cmd_record(uint16_t port, const char *file, int secs) {
    if (!open_sock(port)) return 1;
    FILE *f = fopen(file, "wb");
    if (!f) { fprintf(stderr, "netbot: cannot write %s\n", file); return 1; }
    fwrite("NBR1", 1, 4, f);
    rand_id();
    uint64_t t0 = now_ms(), last_print = t0;
    int snaps = 0, hellos = 0, bad = 0, first_player = -1, other = 0, evs = 0;
    uint8_t buf[NET_PKT_MAX + 64];
    printf("netbot record: port %u, id %u -> %s\n", port, g_me, file);
    while (!g_stop && (!secs || now_ms() - t0 < (uint64_t)secs * 1000)) {
        check_stopfile();
        uint32_t ip; uint16_t pt;
        int n = nsock_recv(&g_sock, buf, sizeof buf, &ip, &pt);
        if (n < 0) break;
        if (n == 0) { nap(1); continue; }
        int type; uint8_t pl; uint16_t seq; char nm[NET_NAME_MAX + 1] = {0};
        NetSnap s;
        if (!net_unpack(buf, n, &type, &pl, &seq, &s, nm)) {
            bad++;
            int ov = net_other_version(buf, n);
            if (ov >= 0 && bad == 1) fprintf(stderr, "netbot: packet with protocol version %d, ours is %d\n", ov, NET_VERSION);
            continue;
        }
        if (type == NET_HELLO) {
            hellos++;
            add_target(ip, pt, false);
            send_hello_to(ip, pt);  // the game treats us as a peer once it hears a HELLO back
        } else if (type == NET_SNAP) {
            if (first_player < 0) first_player = pl;
            if (pl != first_player) { other++; continue; }
            put32(f, (uint32_t)(now_ms() - t0)), put16(f, (uint16_t)n);
            fwrite(buf, 1, (size_t)n, f);
            snaps++, evs += s.nev;
            if (add_target(ip, pt, false)) send_hello_to(ip, pt);
        }
        if (now_ms() - last_print > 2000) printf("  %d snaps, %d hellos\n", snaps, hellos), last_print = now_ms();
        fflush(f);
    }
    fclose(f);
    printf("netbot record: done: %d snaps (%d events) from player %d, %d hellos, %d bad, %d other-player snaps\n", snaps, evs, first_player, hellos, bad, other);
    return 0;
}

// ---- play ----
static float g_off[2];
static void shift(float p[3]) { p[0] += g_off[0], p[2] += g_off[1]; }

// event seq remap: an old seq keeps its new seq while it keeps appearing (it rides 3 SNAPs); after 1500 ms of recording time
// without it, a reappearing old seq (8-bit wrap) gets a fresh new seq
typedef struct { bool on; uint8_t to; uint32_t at; } SeqMap;
static SeqMap g_map[256];
static uint8_t g_evctr;
static uint8_t map_seq(uint8_t old, uint32_t rec_ms) {
    SeqMap *m = &g_map[old];
    if (!m->on || rec_ms - m->at > 1500) m->on = true, m->to = g_evctr++;
    m->at = rec_ms;
    return m->to;
}

static void transform(NetSnap *s, uint32_t rec_ms) {
    s->player = g_me, s->seq = ++g_seq;
    shift(s->feet);
    for (int i = 0; i < s->nev; i++) {
        NetEvent *e = &s->ev[i];
        e->seq = map_seq(e->seq, rec_ms);
        if (e->anchor == NANC_WORLD) shift(e->pos);  // anchored offsets stay as they are
    }
    for (int i = 0; i < s->nmv; i++) {
        NetFxMove *m = &s->mv[i];
        m->ev = map_seq(m->ev, rec_ms);
        if (m->anchor == NANC_WORLD) shift(m->pos);
    }
    for (int i = 0; i < s->npm; i++) {
        NetPartyMember *m = &s->pm[i];
        shift(m->feet);
        if (m->flags & NPM_SHIELD) shift(m->shield);
    }
}

static int cmd_play(const char *file, uint16_t port, int secs, bool loop) {
    int n;
    Rec *r = load_rec(file, &n);
    if (!r) return 1;
    // decode the first player's SNAPs once (a file of another protocol version has none = clear message)
    NetSnap *sn = malloc(sizeof *sn * (size_t)(n ? n : 1));
    uint32_t *at = malloc(sizeof *at * (size_t)(n ? n : 1));
    int ns = 0;
    for (int i = 0; i < n; i++) {
        int type;
        if (net_unpack(r[i].d, r[i].len, &type, NULL, NULL, &sn[ns], NULL) && type == NET_SNAP) at[ns] = r[i].at, ns++;
    }
    if (!ns) { fprintf(stderr, "netbot: no usable SNAPs in %s (%d records; made by another protocol version?)\n", file, n); return 1; }
    if (!g_ntg) { fprintf(stderr, "netbot: need at least one --peer ip[:port]\n"); return 1; }
    if (!open_sock(port)) return 1;
    rand_id();
    printf("netbot play: %s id %u port %u, %d snaps over %u ms, offset %.2f,%.2f u, %d peer(s)%s\n", g_name, g_me, port, ns, at[ns - 1], g_off[0], g_off[1], g_ntg, loop ? ", loop" : "");
    uint64_t t0 = now_ms(), start = t0, next_hello = t0;
    int i = 0, sent = 0, sent_ev = 0, got_hello = 0;
    uint8_t buf[NET_PKT_MAX + 64];
    while (!g_stop && (!secs || now_ms() - start < (uint64_t)secs * 1000)) {
        uint64_t now = now_ms();
        check_stopfile();
        if (now >= next_hello) send_hello_all(), next_hello = now + 1000;
        for (int k = 0; k < 32; k++) {  // answer HELLOs, learn senders
            uint32_t ip; uint16_t pt;
            int len = nsock_recv(&g_sock, buf, sizeof buf, &ip, &pt);
            if (len <= 0) break;
            int type; uint8_t pl;
            if (!net_unpack(buf, len, &type, &pl, NULL, NULL, NULL) || pl == g_me) continue;
            if (type == NET_HELLO) {
                got_hello++;
                if (add_target(ip, pt, false)) send_hello_to(ip, pt);
            }
        }
        while (i < ns && at[i] <= now - t0) {
            NetSnap s = sn[i];
            transform(&s, at[i]);
            uint8_t b[NET_PKT_MAX];
            int len = net_pack_snap(&s, b, sizeof b);
            if (len > 0) send_all(b, len), sent++, sent_ev += s.nev;
            i++;
        }
        if (i >= ns) {
            if (!loop) break;
            nap(500);
            i = 0, t0 = now_ms();
            memset(g_map, 0, sizeof g_map);
        }
        nap(1);
    }
    send_bye_all();
    printf("netbot play: done: %d snaps sent (%d events), %d hellos heard\n", sent, sent_ev, got_hello);
    nsock_close(&g_sock);
    return 0;
}

// ---- dump ----
static int cmd_dump(const char *file) {
    int n;
    Rec *r = load_rec(file, &n);
    if (!r) return 1;
    int ns = 0;
    for (int i = 0; i < n; i++) {
        int type; NetSnap s;
        if (!net_unpack(r[i].d, r[i].len, &type, NULL, NULL, &s, NULL) || type != NET_SNAP) { printf("rec %d: unusable\n", i); continue; }
        ns++;
        printf("snap %d player %u seq %u feet %.2f %.2f %.2f nev %u nmv %u npm %u\n", i, s.player, s.seq, s.feet[0], s.feet[1], s.feet[2], s.nev, s.nmv, s.npm);
        for (int e = 0; e < s.nev; e++)
            printf("  ev seq %u kind %u anc %u pos %.2f %.2f %.2f\n", s.ev[e].seq, s.ev[e].kind, s.ev[e].anchor, s.ev[e].pos[0], s.ev[e].pos[1], s.ev[e].pos[2]);
        for (int e = 0; e < s.nmv; e++)
            printf("  mv ev %u flags %u anc %u pos %.2f %.2f %.2f\n", s.mv[e].ev, s.mv[e].flags, s.mv[e].anchor, s.mv[e].pos[0], s.mv[e].pos[1], s.mv[e].pos[2]);
        for (int e = 0; e < s.npm; e++)
            printf("  pm who %u flags %u feet %.2f %.2f %.2f shield %.2f %.2f %.2f\n", s.pm[e].who, s.pm[e].flags, s.pm[e].feet[0], s.pm[e].feet[1],
                   s.pm[e].feet[2], s.pm[e].shield[0], s.pm[e].shield[1], s.pm[e].shield[2]);
    }
    printf("total %d snaps\n", ns);
    return 0;
}

static void usage(void) {
    fprintf(stderr,
            "usage: netbot record <port> <file> [--name N] [--secs S]\n"
            "       netbot play <file> --port P --name N --peer ip[:port] [--peer ...] [--offset x,z] [--loop] [--secs S]\n"
            "       netbot dump <file>\n");
}

int main(int argc, char **argv) {
    if (argc < 3) return usage(), 2;
    signal(SIGINT, on_sig), signal(SIGTERM, on_sig);
    const char *mode = argv[1];
    int secs = 0;
    bool loop = false;
    uint16_t port = 0;
    const char *pos[2] = {NULL, NULL};
    int npos = 0;
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--name") && i + 1 < argc) {
            const char *v = argv[++i];
            size_t k = 0;
            while (v[k] && k < NET_NAME_MAX) g_name[k] = v[k], k++;
            g_name[k] = 0;
        } else if (!strcmp(a, "--secs") && i + 1 < argc) secs = atoi(argv[++i]);
        else if (!strcmp(a, "--port") && i + 1 < argc) port = (uint16_t)atoi(argv[++i]);
        else if (!strcmp(a, "--stopfile") && i + 1 < argc) g_stopfile = argv[++i];
        else if (!strcmp(a, "--loop")) loop = true;
        else if (!strcmp(a, "--offset") && i + 1 < argc) {
            if (sscanf(argv[++i], "%f,%f", &g_off[0], &g_off[1]) != 2) return fprintf(stderr, "netbot: --offset x,z\n"), 2;
        } else if (!strcmp(a, "--peer") && i + 1 < argc) {
            uint32_t ip; uint16_t pt;
            if (!parse_peer(argv[++i], &ip, &pt)) return fprintf(stderr, "netbot: bad --peer %s\n", argv[i]), 2;
            add_target(ip, pt, true);
        } else if (a[0] != '-' && npos < 2) pos[npos++] = a;
        else return usage(), 2;
    }
    if (!strcmp(mode, "record") && npos == 2) return cmd_record((uint16_t)atoi(pos[0]), pos[1], secs);
    if (!strcmp(mode, "play") && npos == 1 && port) return cmd_play(pos[0], port, secs, loop);
    if (!strcmp(mode, "dump") && npos == 1) return cmd_dump(pos[0]);
    return usage(), 2;
}
