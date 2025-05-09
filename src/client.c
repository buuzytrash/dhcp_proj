#include <aio.h>
#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

struct dhcp_packet {
    uint8_t op;             // BOTTREQUEST (1) / BOOTREPLY (2)
    uint8_t htype;          // 1 (Ethernet)
    uint8_t hlen;           // 6 (MAC address)
    uint8_t hops;           // 0
    uint32_t xid;           // Uniq ID for transaction
    uint16_t secs;          // 0
    uint16_t flags;         // BROADCAST-flag
    uint32_t ciaddr;        // curr IP (0, if doesn't)
    uint32_t yiaddr;        // new IP (fils server)
    uint32_t siaddr;        // server IP
    uint32_t giaddr;        // retranslator IP, gateway IP
    uint8_t chaddr[16];     // client MAC-addr
    uint8_t sname[64];      // server name
    uint8_t file[128];      // download file
    uint32_t magic_cookie;  // 0x63825363 (DHCP signature)
    uint8_t options[308];   // DHCP options
};

// DHCP-options
#define DHCP_OPTION_SUBNET_MASK 1
#define DHCP_OPTION_ROUTER 3
#define DHCP_OPTION_DNS_SERVER 6
#define DHCP_OPTION_REQUESTED_IP 50
#define DHCP_OPTION_LEASE_TIME 51
#define DHCP_OPTION_DHCP_SERVER 54
#define DHCP_OPTION_MSG_TYPE 53
#define DHCP_OPTION_END 255

#define DHCPDISCOVER 1
#define DHCPOFFER 2
#define DHCPREQUEST 3
#define DHCPACK 5
#define DHCPNAK 6

void get_mac_addr(const char *ifname, uint8_t *mac) {
    int fd = 0;
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket() in get_mac_addr()");
        return;
    }
    struct ifreq ifr;
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ioctl(fd, SIOCGIFHWADDR, &ifr);
    memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
    close(fd);
}

void set_ip_addr(const char *ifname, uint32_t ip) {
    int fd = 0;
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket() in set_ip_addr()");
        return;
    }
    struct ifreq ifr;
    struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;

    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = ip;

    ioctl(fd, SIOCSIFADDR, &ifr);
    close(fd);
}

uint32_t get_dhcp_serv_id(uint8_t *options) {
    int i = 0;
    while (i < 308) {
        if (options[i] == DHCP_OPTION_END) {
            break;
        }
        if (options[i] == DHCP_OPTION_DHCP_SERVER && options[i + 1] == 4) {
            uint32_t server_id;
            memcpy(&server_id, &options[i + 2], 4);
            return server_id;
        }
        i += 2 + options[i + 1];
    }
    return 0;
}

void print_dhcp_packet(const struct dhcp_packet *packet, const char *type) {
    printf("\n=== DHCP %s PACKET DETAILS ===\n", type);
    printf("Operation:               %s\n",
           packet->op == 1 ? "REQUEST" : "REPLY");
    printf("Hardware Type:           %d\n", packet->htype);
    printf("Hardware Address Length: %d\n", packet->hlen);
    printf("Transaction ID (xid):    0x%08X\n", htonl(packet->xid));
    printf("Flags:                   0x%04X\n", htons(packet->flags));
    printf("Client IP (ciaddr):      %s\n",
           inet_ntoa(*(struct in_addr *)&packet->ciaddr));
    printf("Your IP (yiaddr):        %s\n",
           inet_ntoa(*(struct in_addr *)&packet->yiaddr));
    printf("Server IP (siaddr):      %s\n",
           inet_ntoa(*(struct in_addr *)&packet->siaddr));
    printf("Gateway IP (giaddr):     %s\n",
           inet_ntoa(*(struct in_addr *)&packet->giaddr));

    printf("Client MAC:              %02X:%02X:%02X:%02X:%02X:%02X\n",
           packet->chaddr[0], packet->chaddr[1], packet->chaddr[2],
           packet->chaddr[3], packet->chaddr[4], packet->chaddr[5]);
    printf("Server Name:             %.64s\n", packet->sname);
    printf("Boot file:               %.128s\n", packet->file);
    printf("Magic Cookie:            0x%08X\n", htonl(packet->magic_cookie));

    printf("\nOptions:\n");
    const uint8_t *opt = packet->options;
    while (*opt != DHCP_OPTION_END &&
           (size_t)(opt - packet->options) < sizeof(packet->options)) {
        if (*opt == 0) {
            opt++;
            continue;
        }
        uint8_t code = *opt++;
        uint8_t len = *opt++;
        printf("Option %3d (len %3d): ", code, len);

        switch (code) {
            case DHCP_OPTION_MSG_TYPE:
                printf("Message type: ");
                switch (*opt) {
                    case DHCPDISCOVER:
                        printf("DHCPDISCOVER");
                        break;
                    case DHCPOFFER:
                        printf("DHCPOFFER");
                        break;
                    case DHCPREQUEST:
                        printf("DHCPREQUEST");
                        break;
                    case DHCPACK:
                        printf("DHCPACK");
                        break;
                    case DHCPNAK:
                        printf("DHCPNAK");
                        break;
                    default:
                        printf("Unknown (%d)", *opt);
                        break;
                }
                break;
            case DHCP_OPTION_SUBNET_MASK:
                printf("Subnet Mask: %s", inet_ntoa(*(struct in_addr *)opt));
                break;
            case DHCP_OPTION_ROUTER:
                printf("Router: %s", inet_ntoa(*(struct in_addr *)opt));
                break;
            case DHCP_OPTION_DNS_SERVER:
                printf("DNS Server: %s", inet_ntoa(*(struct in_addr *)opt));
                break;
            case DHCP_OPTION_REQUESTED_IP:
                printf("Requested IP: %s", inet_ntoa(*(struct in_addr *)opt));
                break;
            case DHCP_OPTION_LEASE_TIME:
                printf("Lease Time: %u seconds", ntohl(*(uint32_t *)opt));
                break;
            case DHCP_OPTION_DHCP_SERVER:
                printf("DHCP Server: %s", inet_ntoa(*(struct in_addr *)opt));
                break;
            default:
                for (int i = 0; i < len; i++) {
                    printf("%02X ", opt[i]);
                }
                break;
        }
        printf("\n");
        opt += len;
    }
    printf("=================================\n\n");
}

int main() {
    srand(time(NULL));
    int sock = 0;
    struct sockaddr_in client_addr = {0}, server_addr = {0};
    char ifname[] = "wlp2s0";
    uint8_t mac[6];

    set_ip_addr(ifname, 0);

    get_mac_addr(ifname, mac);
    printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2],
           mac[3], mac[4], mac[5]);

    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket()");
        exit(EXIT_FAILURE);
    }

    int broadcast = 1;
    if ((setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast,
                    sizeof(broadcast))) < 0) {
        perror("setsockopt(SO_BROADCAST)");
        close(sock);
        exit(EXIT_FAILURE);
    }

    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt(SO_REUSEADDR)");
        close(sock);
        exit(EXIT_FAILURE);
    }

    struct timeval tv;
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    if ((setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))) < 0) {
        perror("setsockopt(SO_RCVTIMEO)");
        close(sock);
        exit(EXIT_FAILURE);
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);

    if ((setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr))) <
        0) {
        perror("setsockopt(SO_BINDTODEVICE)");
        close(sock);
        exit(EXIT_FAILURE);
    }

    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(68);
    client_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("bind()");
        close(sock);
        exit(EXIT_FAILURE);
    }

    struct dhcp_packet discover_packet = {0};
    discover_packet.op = 1;
    discover_packet.htype = 1;
    discover_packet.hlen = 6;
    discover_packet.xid = htonl(rand());
    discover_packet.flags = htons(0x8000);
    memcpy(discover_packet.chaddr, mac, 6);
    discover_packet.magic_cookie = htonl(0x63825363);
    uint8_t discover_options[] = {
        DHCP_OPTION_MSG_TYPE, 1,
        DHCPDISCOVER,    // DHCP DISCOVER
        DHCP_OPTION_END  // END
    };
    memcpy(discover_packet.options, discover_options, sizeof(discover_options));
    // discover_packet.ciaddr = 0;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(67);
    server_addr.sin_addr.s_addr = INADDR_BROADCAST;

    printf("CIADDR before send: %s\n",
           inet_ntoa(*(struct in_addr *)&discover_packet.ciaddr));

    ssize_t discover_size = sizeof(struct dhcp_packet) -
                            sizeof(discover_packet.options) +
                            sizeof(discover_options);

    print_dhcp_packet(&discover_packet, "DHCPDISCOVER");
    if ((sendto(sock, &discover_packet, discover_size, 0,
                (struct sockaddr *)&server_addr, sizeof(server_addr))) < 0) {
        perror("sendto(DHCPDISCOVER)");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Sent DHCPDISCOVER\n");

    uint8_t buffer[1024];
    struct dhcp_packet *offer_packet = (struct dhcp_packet *)buffer;

    if ((recvfrom(sock, buffer, sizeof(buffer), 0, NULL, NULL)) < 0) {
        perror("recvfrom(DHCPOFFER)");
        close(sock);
        exit(EXIT_FAILURE);
    }

    if (offer_packet->xid != discover_packet.xid ||
        offer_packet->options[0] != DHCP_OPTION_MSG_TYPE ||
        offer_packet->options[2] != DHCPOFFER) {
        printf("Not a DHCPOFFER!\n");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("Received DHCPOFFER, offered IP: %s\n",
           inet_ntoa(*(struct in_addr *)&offer_packet->yiaddr));
    print_dhcp_packet(offer_packet, "OFFER");

    struct dhcp_packet request_packet = {0};
    request_packet.op = 1;
    request_packet.htype = 1;
    request_packet.hlen = 6;
    request_packet.xid = discover_packet.xid;
    request_packet.flags = htons(0x8000);
    memcpy(request_packet.chaddr, mac, 6);
    request_packet.magic_cookie = htonl(0x63825363);
    request_packet.yiaddr = 0;

    uint8_t request_options[] = {DHCP_OPTION_MSG_TYPE,
                                 1,
                                 DHCPREQUEST,
                                 DHCP_OPTION_REQUESTED_IP,
                                 4,
                                 0,
                                 0,
                                 0,
                                 0,
                                 DHCP_OPTION_DHCP_SERVER,
                                 4,
                                 0,
                                 0,
                                 0,
                                 0,
                                 DHCP_OPTION_END};

    memcpy(&request_options[5], &offer_packet->yiaddr, 4);
    // memcpy(&request_options[10], &offer_packet->siaddr, 4);
    uint32_t server_id = get_dhcp_serv_id(offer_packet->options);
    memcpy(&request_options[10], &server_id, 4);
    memcpy(&request_packet.options, request_options, sizeof(request_options));

    print_dhcp_packet(&request_packet, "DHCPREQUEST");
    if ((sendto(sock, &request_packet, sizeof(request_packet), 0,
                (struct sockaddr *)&server_addr, sizeof(server_addr))) < 0) {
        perror("sendto(DHCPREQUEST)");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("Sent DHCPREQUEST\n");

    if ((recvfrom(sock, buffer, sizeof(buffer), 0, NULL, NULL)) < 0) {
        perror("recvfrom(DHCPACK)");
        close(sock);
        exit(EXIT_FAILURE);
    }
    struct dhcp_packet *ack_packet = (struct dhcp_packet *)buffer;

    if (ack_packet->options[0] == 53 && ack_packet->options[2] == DHCPNAK) {
        printf("Received DHCPNAK! Server declined the request.\n");
        close(sock);
        exit(EXIT_FAILURE);
    }

    if (ack_packet->options[0] != DHCP_OPTION_MSG_TYPE ||
        ack_packet->options[2] != DHCPACK) {
        printf("Not a DHCPACK!\n");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("Recived DHCPACK, IP: %s\n",
           inet_ntoa(*(struct in_addr *)&ack_packet->yiaddr));
    print_dhcp_packet(ack_packet, "ACK");

    set_ip_addr(ifname, ack_packet->yiaddr);
    printf("IP configured successfully!\n");

    close(sock);
    return 0;
}