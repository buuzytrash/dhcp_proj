#include "packet_utils.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

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
