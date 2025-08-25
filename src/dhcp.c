#include "dhcp.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "network_utils.h"
#include "packet_utils.h"

void dhcp_client_run(const char *ifname) {
    srand(time(NULL));

    bring_interface_up(ifname);
    set_ip_addr(ifname, INADDR_ANY);

    int sock = 0;
    uint8_t mac[6];

    get_mac_addr(ifname, mac);
    printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2],
           mac[3], mac[4], mac[5]);

    if ((sock = create_dhcp_socket(ifname)) < 0) {
        fprintf(stderr, "[-] Failed to create socket\n");
        return;
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DHCP_PORT_SERVER);
    server_addr.sin_addr.s_addr = INADDR_BROADCAST;

    struct dhcp_packet discover_packet = {0};
    discover_packet.op = BOOTREQUEST;
    discover_packet.htype = 1;
    discover_packet.hlen = 6;
    discover_packet.xid = htonl(rand() % 0xffffffff);
    discover_packet.secs = htons(1);
    discover_packet.flags = htons(0x8000);
    memcpy(discover_packet.chaddr, mac, 6);

    discover_packet.magic_cookie = htonl(0x63825363);

    // uint8_t discover_options[] = {
    //     DHCP_OPTION_MSG_TYPE,
    //     1,
    //     DHCPDISCOVER,
    //     DHCP_OPTION_PARAMETER_REQUEST_LIST,
    //     3,
    //     1,
    //     3,
    //     6,
    //     DHCP_OPTION_END  // END
    // };
    unsigned char *discover_options = discover_packet.options;
    *discover_options++ = DHCP_OPTION_MSG_TYPE;
    *discover_options++ = 1;
    *discover_options++ = DHCPDISCOVER;

    *discover_options++ = 61;
    *discover_options++ = 7;
    *discover_options++ = 1;
    memcpy(discover_options, mac, 6);
    discover_options += 6;

    *discover_options++ = 12;
    *discover_options++ = strlen("own_dhcp");
    memcpy(discover_options, "own_dhcp", strlen("own_dhcp"));
    discover_options += strlen("own_dhcp");

    *discover_options++ = DHCP_OPTION_END;

    // memcpy(discover_packet.options, discover_options,
    // sizeof(discover_options));

    uint8_t buffer[1024];

    while (1) {
        size_t packet_len =
            offsetof(struct dhcp_packet, options) + sizeof(discover_options);
        if ((sendto(sock, &discover_packet, packet_len, 0,
                    (struct sockaddr *)&server_addr, sizeof(server_addr))) <
            0) {
            perror("[-] sendto(DHCPDISCOVER)");
            close(sock);
            exit(EXIT_FAILURE);
        }
        printf("[*] Sent DHCPDISCOVER\nWaiting for DHCPOFFER...\n");

        ssize_t n_bytes = recvfrom(sock, buffer, sizeof(buffer), 0, NULL, NULL);
        if (n_bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("[-] Timeout waiting for DHCPOFFER, retrying...\n");
                continue;
            }
            perror("[-] recvfrom(DHCPOFFER)");
            close(sock);
            exit(EXIT_FAILURE);
        }

        struct dhcp_packet *recv_packet = (struct dhcp_packet *)buffer;
        if (recv_packet->xid == discover_packet.xid) {
            uint8_t *options = recv_packet->options;
            while (*options != DHCP_OPTION_END &&
                   (size_t)(options - recv_packet->options) <
                       sizeof(recv_packet->options)) {
                if (*options == 0) {
                    options++;
                    continue;
                }

                uint8_t code = *options++;
                uint8_t len = *options++;

                if (code == DHCP_OPTION_MSG_TYPE && len == 1) {
                    if (*options == DHCPOFFER) {
                        printf(
                            "[+] Received DHCPOFFER, offered IP: %s\n",
                            inet_ntoa(*(struct in_addr *)&recv_packet->yiaddr));
                        print_dhcp_packet(recv_packet, "OFFER");
                        close(sock);
                        return;
                    }
                }
                options += len;
            }
        }
    }

    close(sock);
}
