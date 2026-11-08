// Multiplayer slice 1: tiny nonblocking UDP wrapper (Winsock on Windows, BSD sockets on the Deck).
// Header-only, no game includes: host-tested by runtime/test/net_sock_test.c (recon/make_net_sock_test.sh).
// Addresses are IPv4 in network byte order (ip_be) as they sit in sockaddr_in; 0xFFFFFFFF = broadcast.
// The DLL that includes this on Windows links -lws2_32.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

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
