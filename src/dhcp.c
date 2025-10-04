#include "dhcp.h"

#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "network_utils.h"
#include "packet_utils.h"

dhcp_client_t *dhcp_client_init(const char *ifname) {
  srand(time(NULL));

  dhcp_client_t *client = malloc(sizeof(dhcp_client_t));
  if (!client) {
    perror("malloc");
    return NULL;
  }

  memset(client, 0, sizeof(dhcp_client_t));
  strncpy(client->ifname, ifname, IFNAMSIZ - 1);
  client->ifname[IFNAMSIZ - 1] = '\0';

  bring_interface_up(client->ifname);
  get_mac_addr(client->ifname, client->mac);

  client->xid = rand() % 0xffffffff;

  if ((client->sock = create_raw_socket(ifname)) < 0) {
    fprintf(stderr, "[-] Failed to create socket\n");
    free(client);
    return NULL;
  }

  return client;
}

void dhcp_client_cleanup(dhcp_client_t *client) {
  if (client) {
    if (client->sock > 0) {
      close(client->sock);
    }
    free(client);
  }
}

int dhcp_send_discover(dhcp_client_t *client) {
  dhcp_packet_t discover_packet;
  create_dhcp_packet(&discover_packet, client->mac, client->xid, DHCPDISCOVER);

  printf("[*] Sending DHCPDISCOVER, xid: 0x%08X\n", client->xid);

  if (send_dhcp_packet(client->sock, client->mac, &discover_packet, client->xid,
                       DHCPDISCOVER, client->ifname) < 0) {
    fprintf(stderr, "[-] Failed to send DHCPDISCOVER\n");
    return -1;
  }
  return 0;
}

int dhcp_receive_offer(dhcp_client_t *client) {
  dhcp_packet_t offer_packet;

  if (receive_dhcp_packet(client->sock, &offer_packet, client->xid) == 0) {
    if (parse_options(&offer_packet, client) == DHCPOFFER) {
      return 0;
    }
  }
  return -1;
}

int dhcp_send_request(dhcp_client_t *client) {
  dhcp_packet_t request_packet;
  create_dhcp_packet(&request_packet, client->mac, client->xid, DHCPREQUEST);

  uint8_t *opt = request_packet.options;
  while (*opt != DHCP_OPTION_END) {
    opt++;
  }

  *opt++ = DHCP_OPTION_REQUESTED_IP;
  *opt++ = 4;
  memcpy(opt, &client->offered_ip.s_addr, 4);
  opt += 4;

  *opt++ = DHCP_OPTION_DHCP_SERVER;
  *opt++ = 4;
  memcpy(opt, &client->server_ip.s_addr, 4);
  opt += 4;

  *opt = DHCP_OPTION_END;

  printf("[*] Sending DHCPREQUEST, xid: 0x%08X\n", client->xid);
  printf("    Requesting IP: %s\n", inet_ntoa(client->offered_ip));
  printf("    To server: %s\n", inet_ntoa(client->server_ip));

  if (send_dhcp_packet(client->sock, client->mac, &request_packet, client->xid,
                       DHCPREQUEST, client->ifname) < 0) {
    fprintf(stderr, "[-] Failed to send DHCPREQUEST\n");
    return -1;
  }
  return 0;
}

int dhcp_receive_ack(dhcp_client_t *client) {
  dhcp_packet_t ack_packet;

  if (receive_dhcp_packet(client->sock, &ack_packet, client->xid) == 0) {
    int msg_type = parse_options(&ack_packet, client);

    if (msg_type == DHCPACK) {
      set_ip_addr(client->ifname, client->offered_ip, client->subnet_mask);

      if (client->router.s_addr != 0) {
        add_default_router(client->ifname, client->router);
      }

      return 0;
    } else if (msg_type == DHCPNAK) {
      printf("[-] Received DHCPNAK - request denied\n");
      return -1;
    }
  }
  return -1;
}

void dhcp_client_run(const char *ifname) {
  printf("Starting DHCP client on interface: %s\n", ifname);

  dhcp_client_t *client = dhcp_client_init(ifname);
  if (!client) {
    return;
  }

  int attempt = 0;
  while (attempt < 3) {
    if (dhcp_send_discover(client) < 0) {
      break;
    }

    if (dhcp_receive_offer(client) == 0) {
      printf("[+] Successfully received DHCPOFFER\n");
      if (dhcp_send_request(client) < 0) {
        break;
      }

      if (dhcp_receive_ack(client) == 0) {
        printf("[+] DHCP process completed successfully!\n");
        printf("[+] IP: %s, Mask: %s, Router: %s, DNS: %s\n",
               inet_ntoa(client->offered_ip), inet_ntoa(client->subnet_mask),
               inet_ntoa(client->router), inet_ntoa(client->dns));
        break;
      } else {
        printf("[-] Failed to receive ACK/NAK\n");
      }

      printf("[-] Timeout waiting for DHCPOFFER, attempt %d/3\n", attempt + 1);
      attempt++;
      sleep(2);
    }

    if (attempt >= 3) {
      printf("[-] No DHCPOFFER received after 3 attempts\n");
    }

    dhcp_client_cleanup(client);
  }
}