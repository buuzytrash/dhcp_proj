#include "dhcp.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "network_utils.h"
#include "packet_utils.h"

void dhcp_client_run(const char *ifname) {
    int sock = 0;
    uint8_t mac[6];

    // set_ip_addr(ifname, 0);

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
    discover_packet.op = 1;
    discover_packet.htype = 1;
    discover_packet.hlen = 6;
    discover_packet.xid = htonl(rand() % 0xffffffff);
    discover_packet.flags = htons(0x8000);
    memcpy(discover_packet.chaddr, mac, 6);
    discover_packet.magic_cookie = htonl(0x63825363);
    uint8_t discover_options[] = {
        DHCP_OPTION_MSG_TYPE,
        1,
        DHCPDISCOVER,  // DHCP DISCOVER
        55,
        3,
        1,
        3,
        6,
        DHCP_OPTION_END  // END
    };
    memcpy(discover_packet.options, discover_options, sizeof(discover_options));

    print_dhcp_packet(&discover_packet, "DHCPDISCOVER");

    uint8_t buffer[1024];

    while (1) {
        if ((sendto(sock, &discover_packet, sizeof(discover_packet), 0,
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

                if (*options == DHCP_OPTION_MSG_TYPE && options[1] == 1) {
                    if (options[2] == DHCPOFFER) {
                        printf(
                            "[+] Received DHCPOFFER, offered IP: %s\n",
                            inet_ntoa(*(struct in_addr *)&recv_packet->yiaddr));
                        print_dhcp_packet(recv_packet, "OFFER");
                        return;
                    }
                }
                options += 2 + options[1];
            }
        }
    }

    // struct dhcp_packet request_packet = {0};
    // request_packet.op = 1;
    // request_packet.htype = 1;
    // request_packet.hlen = 6;
    // request_packet.xid = discover_packet.xid;
    // request_packet.flags = htons(0x8000);
    // memcpy(request_packet.chaddr, mac, 6);
    // request_packet.magic_cookie = htonl(0x63825363);
    // request_packet.yiaddr = 0;

    // uint8_t request_options[] = {DHCP_OPTION_MSG_TYPE,
    //                              1,
    //                              DHCPREQUEST,
    //                              DHCP_OPTION_REQUESTED_IP,
    //                              4,
    //                              0,
    //                              0,
    //                              0,
    //                              0,
    //                              DHCP_OPTION_DHCP_SERVER,
    //                              4,
    //                              0,
    //                              0,
    //                              0,
    //                              0,
    //                              DHCP_OPTION_END};

    // memcpy(&request_options[5], &offer_packet->yiaddr, 4);
    // // memcpy(&request_options[10], &offer_packet->siaddr, 4);
    // uint32_t server_id = get_dhcp_serv_id(offer_packet->options);
    // memcpy(&request_options[10], &server_id, 4);
    // memcpy(&request_packet.options, request_options,
    // sizeof(request_options));

    // print_dhcp_packet(&request_packet, "DHCPREQUEST");
    // if ((sendto(sock, &request_packet, sizeof(request_packet), 0,
    //             (struct sockaddr *)&server_addr, sizeof(server_addr))) < 0) {
    //     perror("sendto(DHCPREQUEST)");
    //     close(sock);
    //     exit(EXIT_FAILURE);
    // }
    // printf("Sent DHCPREQUEST\n");

    // if ((recvfrom(sock, buffer, sizeof(buffer), 0, NULL, NULL)) < 0) {
    //     perror("recvfrom(DHCPACK)");
    //     close(sock);
    //     exit(EXIT_FAILURE);
    // }
    // struct dhcp_packet *ack_packet = (struct dhcp_packet *)buffer;

    // if (ack_packet->options[0] == 53 && ack_packet->options[2] == DHCPNAK) {
    //     printf("Received DHCPNAK! Server declined the request.\n");
    //     close(sock);
    //     exit(EXIT_FAILURE);
    // }

    // if (ack_packet->options[0] != DHCP_OPTION_MSG_TYPE ||
    //     ack_packet->options[2] != DHCPACK) {
    //     printf("Not a DHCPACK!\n");
    //     close(sock);
    //     exit(EXIT_FAILURE);
    // }
    // printf("Recived DHCPACK, IP: %s\n",
    //        inet_ntoa(*(struct in_addr *)&ack_packet->yiaddr));
    // print_dhcp_packet(ack_packet, "ACK");

    // set_ip_addr(ifname, ack_packet->yiaddr);
    // printf("IP configured successfully!\n");

    close(sock);
}
