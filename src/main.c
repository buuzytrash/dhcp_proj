#include <stdio.h>
#include <stdlib.h>

#include "dhcp.h"

int main(int argc, char *argv[]) {
    srand(time(NULL));

    if (argc != 2) {
        printf("Usage %s <interface>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char *ifname = argv[1];
    dhcp_client_run(ifname);

    return 0;
}

// #include <arpa/inet.h>
// #include <net/if.h>
// #include <netinet/in.h>
// #include <stdint.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <sys/ioctl.h>
// #include <sys/socket.h>
// #include <time.h>
// #include <unistd.h>

// #define DHCP_PORT_SERVER 67
// #define DHCP_PORT_CLIENT 68
// #define DHCP_MAGIC_COOKIE 0x63825363

// struct dhcp_packet {
//     uint8_t op;            // 1: BOOTREQUEST, 2: BOOTREPLY
//     uint8_t htype;         // 1 - Ethernet
//     uint8_t hlen;          // 6 - MAC length
//     uint8_t hops;          // 0
//     uint32_t xid;          // Transaction ID (network byte order)
//     uint16_t secs;         // 0
//     uint16_t flags;        // 0x8000 - Broadcast
//     uint32_t ciaddr;       // 0
//     uint32_t yiaddr;       // Your IP (from server)
//     uint32_t siaddr;       // Next server IP
//     uint32_t giaddr;       // Relay agent IP
//     uint8_t chaddr[16];    // Client MAC + padding
//     uint8_t sname[64];     // Optional server name
//     uint8_t file[128];     // Boot file name
//     uint32_t magic;        // Magic cookie
//     uint8_t options[308];  // DHCP options
// } __attribute__((packed));

// int create_dhcp_socket(const char *ifname) {
//     int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
//     if (sock < 0) {
//         perror("[-] socket()");
//         return -1;
//     }

//     // Critical options
//     int broadcast = 1;
//     if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast,
//                    sizeof(broadcast))) {
//         perror("[-] SO_BROADCAST");
//         close(sock);
//         return -1;
//     }
//     int reuse = 1;
//     if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
//     0) {
//         perror("[-] setsockopt(SO_REUSEADDR)");
//         close(sock);
//         return -1;
//     }

//     struct timeval timeout = {5, 0};  // 5 seconds
//     setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

//     // Bind to interface
//     struct ifreq ifr = {0};
//     strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
//     if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr))) {
//         perror("[-] SO_BINDTODEVICE");
//         close(sock);
//         return -1;
//     }

//     // Bind to client port
//     struct sockaddr_in addr = {.sin_family = AF_INET,
//                                .sin_port = htons(DHCP_PORT_CLIENT),
//                                .sin_addr.s_addr = INADDR_ANY};
//     if (bind(sock, (struct sockaddr *)&addr, sizeof(addr))) {
//         perror("[-] bind()");
//         close(sock);
//         return -1;
//     }

//     return sock;
// }

// void dhcp_discover(int sock, const uint8_t *mac, const char *ifname) {
//     struct dhcp_packet packet = {0};

//     // Header
//     packet.op = 1;  // BOOTREQUEST
//     packet.htype = 1;
//     packet.hlen = 6;
//     packet.xid = htonl(rand() % 0xFFFFFFFF);
//     packet.flags = htons(0x8000);  // Broadcast
//     memcpy(packet.chaddr, mac, 6);
//     packet.magic = htonl(DHCP_MAGIC_COOKIE);

//     // DHCP Options
//     uint8_t options[] = {
//         53, 1, 1,        // DHCP Discover
//         55, 3, 1, 3, 6,  // Request Subnet, Router, DNS
//         255              // End
//     };
//     memcpy(packet.options, options, sizeof(options));

//     // Send to DHCP server
//     struct sockaddr_in dest = {.sin_family = AF_INET,
//                                .sin_port = htons(DHCP_PORT_SERVER),
//                                .sin_addr.s_addr = INADDR_BROADCAST};

//     for (int i = 0; i < 3; i++) {  // Retry 3 times
//         printf("[*] Sending DHCPDISCOVER (xid=0x%08X)\n", ntohl(packet.xid));
//         if (sendto(sock, &packet, sizeof(packet), 0, (struct sockaddr
//         *)&dest,
//                    sizeof(dest)) < 0) {
//             perror("[-] sendto()");
//             continue;
//         }

//         // Receive response
//         uint8_t buf[1024];
//         ssize_t len = recvfrom(sock, buf, sizeof(buf), 0, NULL, NULL);
//         if (len < 0) {
//             perror("[-] recvfrom()");
//             sleep(2);
//             continue;
//         }

//         // Validate packet
//         struct dhcp_packet *reply = (struct dhcp_packet *)buf;
//         if (reply->xid != packet.xid) {
//             printf("[!] Invalid XID\n");
//             continue;
//         }

//         // Parse options
//         uint8_t *opt = reply->options;
//         while (*opt != 255 && (opt - reply->options) < 308) {
//             if (*opt == 0) {
//                 opt++;
//                 continue;
//             }  // Padding
//             uint8_t type = *opt++;
//             uint8_t len = *opt++;

//             if (type == 53 && len == 1) {  // Message Type
//                 if (*opt == 2) {           // DHCPOFFER
//                     printf("[+] Received DHCPOFFER!\n");
//                     printf("    Offered IP: %s\n",
//                            inet_ntoa(*(struct in_addr *)&reply->yiaddr));
//                     return;
//                 }
//             }
//             opt += len;
//         }
//     }

//     printf("[-] No DHCPOFFER received\n");
// }

// int main(int argc, char *argv[]) {
//     if (argc != 2) {
//         printf("Usage: %s <interface>\n", argv[0]);
//         return 1;
//     }

//     srand(time(NULL));

//     // Get MAC address
//     uint8_t mac[6];
//     struct ifreq ifr;
//     int fd = socket(AF_INET, SOCK_DGRAM, 0);
//     strncpy(ifr.ifr_name, argv[1], IFNAMSIZ);
//     ioctl(fd, SIOCGIFHWADDR, &ifr);
//     memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
//     close(fd);

//     // Create socket
//     int sock = create_dhcp_socket(argv[1]);
//     if (sock < 0) return 1;

//     dhcp_discover(sock, mac, argv[1]);
//     close(sock);
//     return 0;
// }