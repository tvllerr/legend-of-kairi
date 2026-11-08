// fakesend: test sender for netbot_test.sh. fakesend <port> <count>: HELLO + <count> SNAPs (20 ms apart) to 127.0.0.1:<port>
// from player 42, from its own port <port>+100. Snap i has feet (10+i, 5, 20); every 5th snap carries a WORLD event
// (seq i, pos 1,2,3), an anchored event (seq i+100, anchor SORA, pos 1,2,3), a WORLD move, an anchored move, one party member
// with feet (7,8,9) and a shield at (4,5,6).
#include "../../../runtime/sora_net_proto.h"
#include "../../../runtime/sora_net_sock.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#define NAP(ms) Sleep(ms)
#else
#include <unistd.h>
#define NAP(ms) usleep((ms) * 1000)
#endif

int main(int argc, char **argv) {
    if (argc < 3) return 2;
    uint16_t port = (uint16_t)atoi(argv[1]);
    int count = atoi(argv[2]);
    NetSock s;
    if (!nsock_open(&s, (uint16_t)(port + 100), false)) return 1;
    uint32_t ip;
    nsock_parse_ip("127.0.0.1", &ip);
    uint8_t b[NET_PKT_MAX];
    int n = net_pack_hello(42, 1, "Fake", b, sizeof b);
    nsock_send(&s, ip, port, b, n);
    NAP(100);
    for (int i = 0; i < count; i++) {
        NetSnap sn;
        memset(&sn, 0, sizeof sn);
        sn.player = 42, sn.seq = (uint16_t)(i + 2);
        sn.feet[0] = 10.0f + (float)i, sn.feet[1] = 5, sn.feet[2] = 20;
        if (i % 5 == 0) {
            sn.nev = 2;
            sn.ev[0] = (NetEvent){(uint8_t)i, 0, 1, {1, 2, 3}, 0, 1, NANC_WORLD};
            sn.ev[1] = (NetEvent){(uint8_t)(i + 100), 0, 2, {1, 2, 3}, 0, 1, NANC_SORA};
            sn.nmv = 2;
            sn.mv[0] = (NetFxMove){(uint8_t)i, 0, {1, 2, 3}, 0, 0, NANC_WORLD};
            sn.mv[1] = (NetFxMove){(uint8_t)(i + 100), 0, {1, 2, 3}, 0, 0, NANC_SORA};
            sn.npm = 1;
            sn.pm[0].who = 0, sn.pm[0].flags = NPM_SHIELD;
            sn.pm[0].feet[0] = 7, sn.pm[0].feet[1] = 8, sn.pm[0].feet[2] = 9;
            sn.pm[0].shield[0] = 4, sn.pm[0].shield[1] = 5, sn.pm[0].shield[2] = 6;
        }
        n = net_pack_snap(&sn, b, sizeof b);
        if (n <= 0) return 1;
        nsock_send(&s, ip, port, b, n);
        NAP(20);
    }
    n = net_pack_bye(42, 9999, b, sizeof b);
    nsock_send(&s, ip, port, b, n);
    return 0;
}
