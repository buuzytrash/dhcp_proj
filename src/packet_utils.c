#include "packet_utils.h"

#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>

#include "logging.h"
#include "network_utils.h"

void create_dhcp_packet(dhcp_packet_t *packet, uint8_t *mac, uint32_t xid,
                        uint8_t msg_type) {
  memset(packet, 0, sizeof(dhcp_packet_t));

  packet->op = BOOTREQUEST;
  packet->htype = DHCP_HTYPE_ETHERNET;
  packet->hlen = DHCP_HLEN_ETHERNET;
  packet->xid = htonl(xid);
  packet->secs = htons(0);
  packet->flags = htons(0x8000);
  memcpy(packet->chaddr, mac, 6);
  packet->magic_cookie = htonl(DHCP_MAGIC_COOKIE);

  uint8_t *opt = packet->options;

  *opt++ = DHCP_OPTION_MSG_TYPE;
  *opt++ = 1;
  *opt++ = msg_type;

  *opt++ = 61;
  *opt++ = 7;
  *opt++ = 1;
  memcpy(opt, mac, 6);
  opt += 6;

  *opt++ = DHCP_OPTION_PARAMETER_REQUEST_LIST;
  *opt++ = 3;
  *opt++ = DHCP_OPTION_SUBNET_MASK;
  *opt++ = DHCP_OPTION_ROUTER;
  *opt++ = DHCP_OPTION_DNS_SERVER;

  *opt++ = DHCP_OPTION_END;
};

void create_header(uint8_t *buffer, uint8_t *src_mac, uint8_t *dst_mac,
                   uint32_t src_ip, uint32_t dst_ip, uint16_t src_port,
                   uint16_t dst_port, uint16_t udp_len) {
  eth_header_t *eth = (eth_header_t *)buffer;
  memcpy(eth->dst_mac, dst_mac, 6);
  memcpy(eth->src_mac, src_mac, 6);
  eth->eth_type = htons(ETH_P_IP);

  ip_header_t *ip = (ip_header_t *)(buffer + sizeof(eth_header_t));
  ip->version = 4;
  ip->ihl = 5;
  ip->tos = 0;
  ip->tot_len = htons(sizeof(ip_header_t) + sizeof(udp_header_t) + udp_len);
  ip->id = htons(rand() % 0xFFFF);
  ip->frag_off = 0;
  ip->ttl = 64;
  ip->protocol = IPPROTO_UDP;
  ip->saddr = src_ip;
  ip->daddr = dst_ip;
  ip->check = 0;
  ip->check = checksum((uint16_t *)ip, sizeof(ip_header_t));

  udp_header_t *udp =
      (udp_header_t *)(buffer + sizeof(eth_header_t) + sizeof(ip_header_t));
  udp->source = htons(src_port);
  udp->dest = htons(dst_port);
  udp->len = htons(sizeof(udp_header_t) + udp_len);
  udp->check = 0;
}

int send_dhcp_packet(int sock, uint8_t *src_mac, dhcp_packet_t *dhcp_packet,
                     const char *ifname) {
  uint8_t buffer[1500];
  uint8_t broadcast_mac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

  create_header(buffer, src_mac, broadcast_mac, INADDR_ANY, INADDR_BROADCAST,
                DHCP_PORT_CLIENT, DHCP_PORT_SERVER, sizeof(dhcp_packet_t));

  memcpy(buffer + sizeof(eth_header_t) + sizeof(ip_header_t) +
             sizeof(udp_header_t),
         dhcp_packet, sizeof(dhcp_packet_t));

  struct ifreq ifr;
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
  if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
    perror("[-] ioctl() SIOCGIFINDEX in send_dhcp_packet");
    return -1;
  }

  struct sockaddr_ll dest_addr;
  memset(&dest_addr, 0, sizeof(dest_addr));
  dest_addr.sll_family = AF_PACKET;
  dest_addr.sll_protocol = htons(ETH_P_IP);
  dest_addr.sll_ifindex = ifr.ifr_ifindex;
  dest_addr.sll_halen = ETH_ALEN;
  memcpy(dest_addr.sll_addr, broadcast_mac, ETH_ALEN);

  DEBUG_PRINT("[DEBUG] Interface index: %d\n", dest_addr.sll_ifindex);

  ssize_t sent = sendto(sock, buffer,
                        sizeof(eth_header_t) + sizeof(ip_header_t) +
                            sizeof(udp_header_t) + sizeof(dhcp_packet_t),
                        0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

  if (sent < 0) {
    perror("[-] sendto() in create_header");
    return -1;
  }

  DEBUG_PRINT("Sent %zd bytes successfully\n", sent);

  return 0;
}

int receive_dhcp_packet(int sock, dhcp_packet_t *dhcp_packet,
                        uint32_t expected_xid) {
  uint8_t buffer[1500];

  while (1) {
    ssize_t n_bytes = recv(sock, buffer, sizeof(buffer), 0);
    if (n_bytes < 0) {
      perror("[-] recv in receive_dhcp_packet");
      return -1;
    }

    DEBUG_PRINT("Received %zd bytes\n", n_bytes);

    if (n_bytes < (ssize_t)(sizeof(eth_header_t) + sizeof(ip_header_t) +
                            sizeof(udp_header_t))) {
      DEBUG_PRINT("Packet too small, skipping\n");

      continue;
    }

    eth_header_t *eth = (eth_header_t *)buffer;
    if (htons(eth->eth_type) != ETH_P_IP) {
      DEBUG_PRINT("Not IP packet, skipping\n");
      continue;
    }

    ip_header_t *ip = (ip_header_t *)(buffer + sizeof(eth_header_t));
    if (ip->protocol != IPPROTO_UDP) {
      DEBUG_PRINT("Not UDP packet, skipping\n");

      continue;
    }

    udp_header_t *udp =
        (udp_header_t *)(buffer + sizeof(eth_header_t) + sizeof(ip_header_t));
    DEBUG_PRINT("UDP dest port: %d (expected: %d)\n", ntohs(udp->dest),
                DHCP_PORT_CLIENT);
    if (ntohs(udp->dest) != DHCP_PORT_CLIENT) {
      DEBUG_PRINT("Not DHCP client port, skipping\n");

      continue;
    }

    size_t headers_size =
        sizeof(eth_header_t) + sizeof(ip_header_t) + sizeof(udp_header_t);

    if (n_bytes < (ssize_t)(headers_size + 240)) {
      DEBUG_PRINT(
          "Packet too small for DHCP, headers: %zu, total received: "
          "%zd\n",
          headers_size, n_bytes);
      continue;
    }

    dhcp_packet_t *recv_packet =
        (dhcp_packet_t *)(buffer + sizeof(eth_header_t) + sizeof(ip_header_t) +
                          sizeof(udp_header_t));

    if (recv_packet->magic_cookie != htonl(DHCP_MAGIC_COOKIE)) {
      continue;
    }

    if (recv_packet->xid != htonl(expected_xid)) {
      continue;
    }

    DEBUG_PRINT("Ethernet type: 0x%04X\n", htons(eth->eth_type));
    DEBUG_PRINT("IP protocol: %d\n", ip->protocol);
    DEBUG_PRINT("UDP dest port: %d\n", ntohs(udp->dest));
    DEBUG_PRINT("DHCP magic cookie: 0x%08X\n", recv_packet->magic_cookie);
    DEBUG_PRINT("Expected XID: 0x%08X, Received XID: 0x%08X\n",
                htonl(expected_xid), recv_packet->xid);

    memcpy(dhcp_packet, recv_packet, sizeof(dhcp_packet_t));
    return 0;
  }
}

int parse_options(dhcp_packet_t *packet, dhcp_client_t *client) {
  uint8_t *options = packet->options;
  uint8_t msg_type = 0;

  while (*options != DHCP_OPTION_END) {
    if (*options == 0) {
      options++;
      continue;
    }

    uint8_t code = *options++;
    uint8_t len = *options++;

    if (code == DHCP_OPTION_MSG_TYPE && len == 1) {
      msg_type = *options;
      break;
    }
    options += len;
  }

  options = packet->options;

  while (*options != DHCP_OPTION_END) {
    if (*options == 0) {
      options++;
      continue;
    }

    uint8_t code = *options++;
    uint8_t len = *options++;

    switch (code) {
      case DHCP_OPTION_MSG_TYPE:
        switch (*options) {
          case DHCPOFFER:
            printf("[+] Received DHCPOFFER!\n");
            printf("    Offered IP: %s\n",
                   inet_ntoa(*(struct in_addr *)&packet->yiaddr));
            client->offered_ip = *(struct in_addr *)&packet->yiaddr;
            client->server_ip = *(struct in_addr *)&packet->siaddr;

            if (verbose_flag) {
              print_dhcp_packet(packet, "OFFER");
            }
            break;

          case DHCPACK:
            printf("[+] Received DHCPACK!\n");
            printf("    Assigned IP: %s\n",
                   inet_ntoa(*(struct in_addr *)&packet->yiaddr));
            client->offered_ip = *(struct in_addr *)&packet->yiaddr;
            client->server_ip = *(struct in_addr *)&packet->siaddr;

            if (verbose_flag) {
              print_dhcp_packet(packet, "ACK");
            }
            break;

          case DHCPNAK:
            printf("[!] Received DHCPNAK!\n");
            if (verbose_flag) {
              print_dhcp_packet(packet, "NAK");
            }
            break;

          default:
            printf("[*] Received DHCP message type: %d\n", *options);
        }
        break;

      case DHCP_OPTION_SUBNET_MASK:
        if (len == 4) {
          client->subnet_mask = *(struct in_addr *)options;
          DEBUG_PRINT("    Subnet Mask: %s\n", inet_ntoa(client->subnet_mask));
        }
        break;

      case DHCP_OPTION_ROUTER:
        if (len >= 4) {
          client->router = *(struct in_addr *)options;
          DEBUG_PRINT("    Router: %s\n", inet_ntoa(client->router));
        }
        break;

      case DHCP_OPTION_DNS_SERVER:
        if (len >= 4) {
          client->dns = *(struct in_addr *)options;
          DEBUG_PRINT("    DNS: %s\n", inet_ntoa(client->dns));
        }
        break;

      case DHCP_OPTION_LEASE_TIME:
        if (len == 4) {
          client->lease_time = ntohl(*(uint32_t *)options);
          DEBUG_PRINT("    Lease Time: %u seconds\n", client->lease_time);
        }
        break;

      case DHCP_OPTION_DHCP_SERVER:
        if (len == 4) {
          client->server_ip = *(struct in_addr *)options;
          DEBUG_PRINT("    Server IP: %s\n", inet_ntoa(client->server_ip));
        }
        break;

      case DHCP_OPTION_REQUESTED_IP:
        if (len == 4) {
          DEBUG_PRINT("    Requested IP: %s\n",
                      inet_ntoa(*(struct in_addr *)options));
        }
        break;
    }

    options += len;
  }

  return msg_type;
}

void print_dhcp_packet(const dhcp_packet_t *packet, const char *type) {
  printf("\n=== DHCP %s PACKET DETAILS ===\n", type);
  printf("Operation:               %s\n",
         packet->op == 1 ? "REQUEST" : "REPLY");
  printf("Hardware Type:           %d\n", packet->htype);
  printf("Hardware Address Length: %d\n", packet->hlen);
  printf("Transaction ID (xid):    0x%08X\n", ntohl(packet->xid));
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
