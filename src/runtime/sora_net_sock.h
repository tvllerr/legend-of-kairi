// Multiplayer slice 1: tiny nonblocking UDP wrapper (Winsock on Windows, BSD sockets on the Deck).
// Header-only, no game includes: host-tested by runtime/test/net_sock_test.c (recon/make_net_sock_test.sh).
// Addresses are IPv4 in network byte order (ip_be) as they sit in sockaddr_in; 0xFFFFFFFF = broadcast.
// The DLL that includes this on Windows links -lws2_32.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifndef SO_EXCLUSIVEADDRUSE
#define SO_EXCLUSIVEADDRUSE ((int)(~SO_REUSEADDR))
#endif
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
#else
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

typedef struct { intptr_t fd; bool ok; } NetSock;

static inline void nsock_closefd_(intptr_t fd) {
#ifdef _WIN32
    closesocket((SOCKET)fd);
#else
    close((int)fd);
#endif
}

static inline bool nsock_open(NetSock *s, uint16_t port, bool broadcast) {
    s->fd = -1, s->ok = false;
#ifdef _WIN32
    static bool wsa_up;
    if (!wsa_up) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa)) return false;
        wsa_up = true;
    }
    SOCKET fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd == INVALID_SOCKET) return false;
    u_long on = 1;
    if (ioctlsocket(fd, FIONBIO, &on)) { closesocket(fd); return false; }
    // a send to a closed port makes the NEXT recvfrom fail with WSAECONNRESET: switch that off
    BOOL no = FALSE; DWORD got = 0;
    WSAIoctl(fd, SIO_UDP_CONNRESET, &no, sizeof no, NULL, 0, &got, NULL, NULL);
#else
    int fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) return false;
#endif
    int one = 1;
#ifdef _WIN32
    // (mp2) Windows: SO_REUSEADDR would let a 2nd process share (and steal packets from) our UDP port; broadcasts still
    // reach an exclusive socket. Linux keeps SO_REUSEADDR (the Deck's native build is the only copy there).
    setsockopt(fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&one, sizeof one);
#else
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof one);
#endif
    if (broadcast && setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (const char *)&one, sizeof one)) {
        nsock_closefd_((intptr_t)fd);
        return false;
    }
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET, a.sin_port = htons(port), a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (struct sockaddr *)&a, sizeof a)) { nsock_closefd_((intptr_t)fd); return false; }
    s->fd = (intptr_t)fd, s->ok = true;
    return true;
}

// bytes sent, -1 = error (or socket not open)
static inline int nsock_send(NetSock *s, uint32_t ip_be, uint16_t port, const void *buf, int len) {
    if (!s->ok) return -1;
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET, a.sin_port = htons(port), a.sin_addr.s_addr = ip_be;
#ifdef _WIN32
    int n = sendto((SOCKET)s->fd, (const char *)buf, len, 0, (struct sockaddr *)&a, sizeof a);
    return n == SOCKET_ERROR ? -1 : n;
#else
    ssize_t n = sendto((int)s->fd, buf, (size_t)len, MSG_DONTWAIT, (struct sockaddr *)&a, sizeof a);
    return n < 0 ? -1 : (int)n;
#endif
}

// bytes received, 0 = nothing waiting (or a transient error: retry next frame), -1 = error / not open.
// A datagram bigger than cap is dropped on Windows (WSAEMSGSIZE -> 0) and truncated to cap on Linux:
// the protocol's own length checks reject it either way.
static inline int nsock_recv(NetSock *s, void *buf, int cap, uint32_t *from_ip_be, uint16_t *from_port) {
    if (!s->ok) return -1;
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
#ifdef _WIN32
    int al = sizeof a;
    int n = recvfrom((SOCKET)s->fd, (char *)buf, cap, 0, (struct sockaddr *)&a, &al);
    if (n == SOCKET_ERROR) {
        int e = WSAGetLastError();
        return (e == WSAEWOULDBLOCK || e == WSAECONNRESET || e == WSAEMSGSIZE || e == WSAEINTR) ? 0 : -1;
    }
#else
    socklen_t al = sizeof a;
    ssize_t n = recvfrom((int)s->fd, buf, (size_t)cap, MSG_DONTWAIT, (struct sockaddr *)&a, &al);
    if (n < 0) return (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR || errno == ECONNREFUSED) ? 0 : -1;
#endif
    if (from_ip_be) *from_ip_be = a.sin_addr.s_addr;
    if (from_port) *from_port = ntohs(a.sin_port);
    return (int)n;
}

static inline void nsock_close(NetSock *s) {
    if (s->ok) nsock_closefd_(s->fd);
    s->fd = -1, s->ok = false;
}

// dotted quad "a.b.c.d" (leading/trailing spaces, tabs, CR/LF allowed) -> network byte order
static inline bool nsock_parse_ip(const char *txt, uint32_t *ip_be) {
    if (!txt) return false;
    while (*txt == ' ' || *txt == '\t') txt++;
    uint8_t q[4];
    for (int i = 0; i < 4; i++) {
        if (i && *txt++ != '.') return false;
        if (*txt < '0' || *txt > '9') return false;
        int v = 0, d = 0;
        while (*txt >= '0' && *txt <= '9') { v = v * 10 + (*txt++ - '0'); if (++d > 3 || v > 255) return false; }
        q[i] = (uint8_t)v;
    }
    while (*txt == ' ' || *txt == '\t' || *txt == '\r' || *txt == '\n') txt++;
    if (*txt) return false;
    memcpy(ip_be, q, 4);  // q[0] first in memory = network order
    return true;
}

// (mp4) "a.b.c.d" or "a.b.c.d:port" (port 1..65535; none = *port 0) -> ip in network byte order. Trailing blanks allowed.
static inline bool nsock_parse_addr(const char *txt, uint32_t *ip_be, uint16_t *port) {
    if (!txt) return false;
    char buf[48];
    size_t n = 0;
    while (*txt == ' ' || *txt == '\t') txt++;
    while (txt[n] && txt[n] != ':' && txt[n] != ' ' && txt[n] != '\t' && txt[n] != '\r' && txt[n] != '\n' && n < sizeof buf - 1) buf[n] = txt[n], n++;
    buf[n] = 0;
    if (!nsock_parse_ip(buf, ip_be)) return false;
    txt += n;
    *port = 0;
    if (*txt == ':') {
        txt++;
        if (*txt < '0' || *txt > '9') return false;
        int v = 0;
        while (*txt >= '0' && *txt <= '9') { v = v * 10 + (*txt++ - '0'); if (v > 65535) return false; }
        if (v < 1) return false;
        *port = (uint16_t)v;
    }
    while (*txt == ' ' || *txt == '\t' || *txt == '\r' || *txt == '\n') txt++;
    return *txt == 0;
}

// ---- (s187) Melty Net: a room instead of the UDP mesh (recon/melty/handoff-2026-11-10, runtime/melty/) ----
// The mod's packets stay as they are; only where they go changes. The glue asks sora_melty_start() ONLY when Melty launched
// this copy (sora_net.h net_melty_wanted: MELTY_NET_DIR set, or SAVAGEWIND_SORA_MELTY=1), and it says 1 only when the
// contract folder has melty.properties: outside Melty everything below is never reached and the UDP path above is all
// there is. sora_melty_state() -1 (refused / closed for good) or no seat in 60 s: the glue stops it and goes back to UDP.
// Three ways to build:
//   SORA_MELTY       the real connector (runtime/melty/sora_melty.cpp + melty_net.h; make_sora_dll41.sh sets it when
//                    runtime/melty/ is there)
//   SORA_MELTY_FAKE  a host test brings its own sora_melty_* (runtime/test/net_melty_test.c)
//   neither          no connector in this build (the public source has no runtime/melty/): never in a room
#if defined(SORA_MELTY)
#include "melty/sora_melty.h"
#else
#include "sora_melty_types.h"  // (s209 step 2: the lab calls' plain types)
#define SORA_MELTY_EVERYONE (-1)
typedef struct sora_melty_peer_event { uint16_t peer; uint8_t joined; char name[64]; } sora_melty_peer_event;
#ifdef SORA_MELTY_FAKE
static int sora_melty_start(const char *folder, const char *game, const char *name);
static void sora_melty_frame(void);
static void sora_melty_where(float x, float y, float z, float yaw, const char *area);
static int sora_melty_send(int peer, const void *packet, int len);
static int sora_melty_recv(uint16_t *from, void *buffer, int capacity);
static int sora_melty_peer(sora_melty_peer_event *out);
static int sora_melty_self(void);
static int sora_melty_connected(void);
static int sora_melty_state(void);
static int sora_melty_log_next(char *line, int capacity);
static void sora_melty_stop(void);
// (s191) the room clock and the room's story (spec 3 / 4.4; the test's fake story: net_melty_test.c)
static double sora_melty_now_ms(void);
static int sora_melty_story_ready(void);
static int sora_melty_story_leader(void);
static int sora_melty_story_set(const char *key, const void *value, int len);
static int sora_melty_story_clear(const char *prefix);
static int sora_melty_story_get(const char *key, void *buffer, int capacity);
static int sora_melty_story_trigger(const char *key, const void *args, int len);
static int sora_melty_story_next(char *key, int key_capacity, void *value, int value_capacity, int *value_len, int *deleted);
static int sora_melty_trigger_next(uint16_t *from, char *key, int key_capacity, void *args, int args_capacity, int *args_len);
// (s209) the lab notes (melty_addon.h's Lab in the connector; the test's fake records them: net_melty_test.c)
static void sora_melty_addon_setup(const char *addon, const char *version, const char *payload_format, float metres_per_unit);
static void sora_melty_addon_frame(float dt, int flags, int on_screen, int drawn, int animated, int room_drawn);
static void sora_melty_addon_puppet(int peer, float x, float y, float z, int moving, float behind_ms, float delay_ms);
static void sora_melty_addon_forget(int peer);
// (s209 step 2) the notes as they happen, autopilot (the test's fake records them / plays the lab)
static void sora_melty_addon_act(const char *action, int phase, const char *detail);
static void sora_melty_addon_played(int peer, const char *action, int result, const char *what);
static void sora_melty_addon_hurt(int from, float amount, const char *how);
static void sora_melty_addon_beat(const char *what, int value, const char *detail);
static int sora_melty_addon_pilot_on(void);
static int sora_melty_addon_take_pilot(sora_melty_pilot *out);
static void sora_melty_addon_report_pilot(int number, int state, float distance);
static void sora_melty_addon_steer_target(float x, float y, float z, float arrive_radius);
static void sora_melty_addon_steer_stop(void);
static void sora_melty_addon_steer_update(float dt, float x, float y, float z, sora_melty_steer *out);
#else
static inline int sora_melty_start(const char *folder, const char *game, const char *name) { (void)folder, (void)game, (void)name; return 0; }
static inline void sora_melty_frame(void) {}
static inline void sora_melty_where(float x, float y, float z, float yaw, const char *area) { (void)x, (void)y, (void)z, (void)yaw, (void)area; }
static inline int sora_melty_send(int peer, const void *packet, int len) { (void)peer, (void)packet, (void)len; return 0; }
static inline int sora_melty_recv(uint16_t *from, void *buffer, int capacity) { (void)from, (void)buffer, (void)capacity; return 0; }
static inline int sora_melty_peer(sora_melty_peer_event *out) { (void)out; return 0; }
static inline int sora_melty_self(void) { return 0; }
static inline int sora_melty_connected(void) { return 0; }
static inline int sora_melty_state(void) { return 0; }
static inline int sora_melty_log_next(char *line, int capacity) { (void)line, (void)capacity; return 0; }
static inline void sora_melty_stop(void) {}
// (s191) no room: never ready, never the leader, the clock 0, every story call refused / empty
static inline double sora_melty_now_ms(void) { return 0; }
static inline int sora_melty_story_ready(void) { return 0; }
static inline int sora_melty_story_leader(void) { return 0; }
static inline int sora_melty_story_set(const char *key, const void *value, int len) { (void)key, (void)value, (void)len; return 0; }
static inline int sora_melty_story_clear(const char *prefix) { (void)prefix; return 0; }
static inline int sora_melty_story_get(const char *key, void *buffer, int capacity) { (void)key, (void)buffer, (void)capacity; return -1; }
static inline int sora_melty_story_trigger(const char *key, const void *args, int len) { (void)key, (void)args, (void)len; return 0; }
static inline int sora_melty_story_next(char *key, int key_capacity, void *value, int value_capacity, int *value_len, int *deleted) {
    (void)key, (void)key_capacity, (void)value, (void)value_capacity, (void)value_len, (void)deleted;
    return 0;
}
static inline int sora_melty_trigger_next(uint16_t *from, char *key, int key_capacity, void *args, int args_capacity, int *args_len) {
    (void)from, (void)key, (void)key_capacity, (void)args, (void)args_capacity, (void)args_len;
    return 0;
}
// (s209) no room: no lab notes
static inline void sora_melty_addon_setup(const char *addon, const char *version, const char *payload_format, float metres_per_unit) {
    (void)addon, (void)version, (void)payload_format, (void)metres_per_unit;
}
static inline void sora_melty_addon_frame(float dt, int flags, int on_screen, int drawn, int animated, int room_drawn) {
    (void)dt, (void)flags, (void)on_screen, (void)drawn, (void)animated, (void)room_drawn;
}
static inline void sora_melty_addon_puppet(int peer, float x, float y, float z, int moving, float behind_ms, float delay_ms) {
    (void)peer, (void)x, (void)y, (void)z, (void)moving, (void)behind_ms, (void)delay_ms;
}
static inline void sora_melty_addon_forget(int peer) { (void)peer; }
// (s209 step 2) no room: no notes, never autopilot
static inline void sora_melty_addon_act(const char *action, int phase, const char *detail) { (void)action, (void)phase, (void)detail; }
static inline void sora_melty_addon_played(int peer, const char *action, int result, const char *what) { (void)peer, (void)action, (void)result, (void)what; }
static inline void sora_melty_addon_hurt(int from, float amount, const char *how) { (void)from, (void)amount, (void)how; }
static inline void sora_melty_addon_beat(const char *what, int value, const char *detail) { (void)what, (void)value, (void)detail; }
static inline int sora_melty_addon_pilot_on(void) { return 0; }
static inline int sora_melty_addon_take_pilot(sora_melty_pilot *out) { (void)out; return 0; }
static inline void sora_melty_addon_report_pilot(int number, int state, float distance) { (void)number, (void)state, (void)distance; }
static inline void sora_melty_addon_steer_target(float x, float y, float z, float arrive_radius) { (void)x, (void)y, (void)z, (void)arrive_radius; }
static inline void sora_melty_addon_steer_stop(void) {}
static inline void sora_melty_addon_steer_update(float dt, float x, float y, float z, sora_melty_steer *out) {
    (void)dt, (void)x, (void)y, (void)z;
    if (out) memset(out, 0, sizeof *out);
}
#endif
// (s209) FRAME's flags, as runtime/melty/sora_melty.h names them (protocol.md Notes)
#define SORA_MELTY_FRAME_MENU 1
#define SORA_MELTY_FRAME_LOADING 2
#define SORA_MELTY_FRAME_PAUSED 4
#define SORA_MELTY_FRAME_DEAD 8
#define SORA_MELTY_FRAME_CUTSCENE 16
#define SORA_MELTY_FRAME_AIR 64
#define SORA_MELTY_FRAME_COMBAT 128
#define SORA_MELTY_FRAME_FOCUSED 2048
#endif

// A room peer id as an address, so the peer table (slots by ip:port) works unchanged: 240.0.<hi>.<lo>, port 1.
// 240.x.x.x is reserved (class E): no real UDP packet comes from it, and nothing is ever sent to it by socket.
#define NMELTY_PORT 1
static inline uint32_t nmelty_ip(uint16_t peer) {
    uint8_t q[4] = {240, 0, (uint8_t)(peer >> 8), (uint8_t)peer};
    uint32_t ip;
    memcpy(&ip, q, 4);
    return ip;
}
static inline bool nmelty_is(uint32_t ip_be, uint16_t port) {
    const uint8_t *q = (const uint8_t *)&ip_be;
    return q[0] == 240 && q[1] == 0 && port == NMELTY_PORT;
}
static inline uint16_t nmelty_peer(uint32_t ip_be) {
    const uint8_t *q = (const uint8_t *)&ip_be;
    return (uint16_t)(q[2] << 8 | q[3]);
}
// a Melty name (UTF-8, up to 63 bytes) as a plate name: printable ASCII kept, each other character one '?', cut to cap - 1
static inline void nmelty_name(char *out, int cap, const char *in, uint16_t peer) {
    int n = 0;
    for (const uint8_t *p = (const uint8_t *)(in ? in : ""); *p && n < cap - 1; p++) {
        if (*p >= 0x20 && *p < 0x7f) out[n++] = (char)*p;
        else if (*p >= 0xc0 || *p < 0x80) out[n++] = '?';  // (a lead byte or a control char: one '?'; continuation bytes add nothing)
    }
    while (n && out[n - 1] == ' ') n--;
    out[n] = 0;
    if (!n) snprintf(out, (size_t)cap, "Player %u", (unsigned)peer);
}
