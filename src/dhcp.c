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

dhcp_client_state_t *dhcp_client_init(const char *ifname) {
  srand(time(NULL));

  dhcp_client_state_t *client = malloc(sizeof(dhcp_client_state_t));
  if (!client) {
    perror("malloc");
    return NULL;
  }

  memset(client, 0, sizeof(dhcp_client_state_t));
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

void dhcp_client_cleanup(dhcp_client_state_t *client) {
  if (client) {
    if (client->sock > 0) {
      close(client->sock);
    }
    free(client);
  }
}

int dhcp_send_discover(dhcp_client_state_t *client) {
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

int dhcp_receive_offer(dhcp_client_state_t *client) {
  dhcp_packet_t offer_packet;

  if (receive_dhcp_packet(client->sock, &offer_packet, client->xid) == 0) {
    uint8_t *options = offer_packet.options;
    while (*options != DHCP_OPTION_END) {
      if (*options == 0) {
        options++;
        continue;
      }

      uint8_t code = *options++;
      uint8_t len = *options++;

      if (code == DHCP_OPTION_MSG_TYPE && len == 1 && *options == DHCPOFFER) {
        printf("[+] Received DHCPOFFER!\n");
        printf("    Offered IP: %s\n",
               inet_ntoa(*(struct in_addr *)&offer_packet.yiaddr));
        client->offered_ip = *(struct in_addr *)&offer_packet.yiaddr;
        client->server_ip = *(struct in_addr *)&offer_packet.siaddr;

        print_dhcp_packet(&offer_packet, "OFFER");
        return 0;
      }

      options += len;
    }
  }
  return -1;
}

void dhcp_client_run(const char *ifname) {
  printf("Starting DHCP client on interface: %s\n", ifname);

  dhcp_client_state_t *client = dhcp_client_init(ifname);
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
      break;
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
