// host test for runtime/sora_net_sock.h (multiplayer slice 1): sh D:/SoraBotW/recon/make_net_sock_test.sh
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include "sora_win.h"  // (winsock2 + windows.h, before the socket header, as in the DLL)
#include "sora_net_sock.h"
static void nap_ms(int ms) { Sleep(ms); }
#else
#include "sora_net_sock.h"
#include <time.h>
static void nap_ms(int ms) { struct timespec t = {0, ms * 1000000L}; nanosleep(&t, NULL); }
#endif

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static void test_loopback_udp(void) {
    NetSock a, b;
    CHECK(nsock_open(&a, 47790, false) && a.ok);
    CHECK(nsock_open(&b, 47791, false) && b.ok);
    uint8_t buf[64], got[64];
    uint32_t ip = 0; uint16_t port = 0;
    CHECK(nsock_recv(&b, got, sizeof got, &ip, &port) == 0);  // nothing waiting, nonblocking
    for (int i = 0; i < 32; i++) buf[i] = (uint8_t)(i * 7 + 1);
    uint32_t lo = 0;
    CHECK(nsock_parse_ip("127.0.0.1", &lo));
    CHECK(nsock_send(&a, lo, 47791, buf, 32) == 32);
    int n = 0;
    for (int t = 0; t < 200 && n == 0; t += 5) { n = nsock_recv(&b, got, sizeof got, &ip, &port); if (!n) nap_ms(5); }
    CHECK(n == 32);
    CHECK(memcmp(buf, got, 32) == 0);
    CHECK(port == 47790);
    CHECK(ip == lo);
    CHECK(nsock_recv(&b, got, sizeof got, &ip, &port) == 0);
    nsock_close(&a); nsock_close(&b);
    CHECK(!a.ok && !b.ok);
    nsock_close(&a);  // double close is harmless
}

static void test_broadcast_open(void) {
    NetSock s;
    CHECK(nsock_open(&s, 47792, true) && s.ok);
    nsock_close(&s);
}

// (mp2) Windows: the port is exclusive (SO_EXCLUSIVEADDRUSE), broadcast discovery still reaches it. Linux keeps SO_REUSEADDR.
static void test_exclusive_port_and_broadcast(void) {
    NetSock a, b, tx;
    CHECK(nsock_open(&a, 47793, true) && a.ok);
#ifdef _WIN32
    CHECK(!nsock_open(&b, 47793, true));  // a 2nd process (here: socket) cannot share the port
    CHECK(!b.ok);
#else
    (void)b;
#endif
    CHECK(nsock_open(&tx, 47794, true) && tx.ok);
    uint8_t m[8] = {1, 2, 3, 4, 5, 6, 7, 8}, got[16];
    uint32_t ip = 0; uint16_t port = 0;
    CHECK(nsock_send(&tx, 0xFFFFFFFFu, 47793, m, 8) == 8);  // broadcast
    int n = 0;
    for (int t = 0; t < 200 && n == 0; t += 5) { n = nsock_recv(&a, got, sizeof got, &ip, &port); if (!n) nap_ms(5); }
    CHECK(n == 8 && memcmp(m, got, 8) == 0 && port == 47794);
    nsock_close(&a);
    CHECK(nsock_open(&a, 47793, true) && a.ok);  // free again after close
    nsock_close(&a); nsock_close(&tx);
}

static void test_parse_ip(void) {
    uint32_t ip = 0;
    CHECK(nsock_parse_ip("192.168.1.50", &ip));
    const uint8_t *p = (const uint8_t *)&ip;  // network byte order in memory
    CHECK(p[0] == 192 && p[1] == 168 && p[2] == 50 && p[3] == 202);
    CHECK(!nsock_parse_ip("300.1.1.1", &ip));
    CHECK(!nsock_parse_ip("1.2.3", &ip));
    CHECK(!nsock_parse_ip("1.2.3.4.5", &ip));
    CHECK(!nsock_parse_ip("a.b.c.d", &ip));
    CHECK(!nsock_parse_ip("", &ip));
    CHECK(!nsock_parse_ip(NULL, &ip));
    CHECK(nsock_parse_ip(" 10.0.0.1\n", &ip));  // live_test.txt lines may carry spaces/newline
}

// (mp4) "ip[:port]" for net peer / SAVAGEWIND_SORA_NET_PEERS
static void test_parse_addr(void) {
    uint32_t ip = 0; uint16_t port = 99;
    CHECK(nsock_parse_addr("192.168.1.50", &ip, &port) && port == 0);
    uint8_t q[4]; memcpy(q, &ip, 4);
    CHECK(q[0] == 192 && q[1] == 168 && q[2] == 50 && q[3] == 202);
    CHECK(nsock_parse_addr("10.0.0.7:7791", &ip, &port) && port == 7791);
    CHECK(nsock_parse_addr("  127.0.0.1:65535 \r\n", &ip, &port) && port == 65535);
    CHECK(!nsock_parse_addr("10.0.0.7:0", &ip, &port));
    CHECK(!nsock_parse_addr("10.0.0.7:65536", &ip, &port));
    CHECK(!nsock_parse_addr("10.0.0.7:", &ip, &port));
    CHECK(!nsock_parse_addr("10.0.0.7:12x", &ip, &port));
    CHECK(!nsock_parse_addr("10.0.0.7:80:90", &ip, &port));
    CHECK(!nsock_parse_addr("host:80", &ip, &port));
    CHECK(!nsock_parse_addr("", &ip, &port));
    CHECK(!nsock_parse_addr(NULL, &ip, &port));
}

int main(void) {
    test_loopback_udp();
    test_broadcast_open();
    test_exclusive_port_and_broadcast();
    test_parse_ip();
    test_parse_addr();
    printf("net_sock_test: %d/%d passed, %d fails\n", runs - fails, runs, fails);
    return fails != 0;
}
