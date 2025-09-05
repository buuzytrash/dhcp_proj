#include "dhcp.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <net/if.h>

#include "network_utils.h"
#include "packet_utils.h"

void dhcp_client_run(const char *ifname)
{
    printf("Starting DHCP client on interface: %s\n", ifname);

    struct ifreq ifr;
    int test_sock = socket(AF_INET, SOCK_DGRAM, 0);
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    if (ioctl(test_sock, SIOCGIFINDEX, &ifr) < 0)
    {
        perror("Interface does not exist");
        close(test_sock);
        return;
    }
    close(test_sock);
    printf("Interface exists: %s\n", ifname);

    srand(time(NULL));
    uint32_t xid = rand() % 0xffffffff;

    bring_interface_up(ifname);

    int sock = 0;
    uint8_t mac[6];

    get_mac_addr(ifname, mac);
    printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2],
           mac[3], mac[4], mac[5]);

    if ((sock = create_raw_socket(ifname)) < 0)
    {
        fprintf(stderr, "[-] Failed to create socket\n");
        return;
    }

    struct dhcp_packet discover_packet;
    create_dhcp_packet(&discover_packet, mac, xid, DHCPDISCOVER);

    int attempt = 0;
    while (attempt < 3)
    {
        printf("[*] Sending DHCPDISCOVER, xid: 0x%08X\n", xid);

        if (send_dhcp_packet(sock, mac, &discover_packet, xid, DHCPDISCOVER, ifname) < 0)
        {
            fprintf(stderr, "[-] Failed to send DHCPDISCOVER\n");
            close(sock);
            return;
        }

        printf("[*] Waiting for DHCPOFFER...\n");

        struct dhcp_packet offer_packet;
        if (receive_dhcp_packet(sock, &offer_packet, xid) == 0)
        {
            uint8_t *options = offer_packet.options;
            while (*options != DHCP_OPTION_END)
            {
                if (*options == 0)
                {
                    options++;
                    continue;
                }

                uint8_t code = *options++;
                uint8_t len = *options++;

                if (code == DHCP_OPTION_MSG_TYPE && len == 1 && *options == DHCPOFFER)
                {
                    printf("[+] Received DHCPOFFER!\n");
                    printf("    Offered IP: %s\n", inet_ntoa(*(struct in_addr *)&offer_packet.yiaddr));
                    print_dhcp_packet(&offer_packet, "OFFER");
                    close(sock);
                    return;
                }

                options += len;
            }
        }

        printf("[-] Timeout waiting for DHCPOFFER, attempt %d/3\n", attempt + 1);
        attempt++;
        sleep(2);
    }
    printf("[-] No DHCPOFFER received after 3 attempts\n");

    close(sock);
}
