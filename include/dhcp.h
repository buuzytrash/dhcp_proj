#ifndef DHCP_H
#define DHCP_H

#include <netinet/in.h>
#include <stdint.h>

// DHCP-options
#define DHCP_OPTION_SUBNET_MASK 1
#define DHCP_OPTION_ROUTER 3
#define DHCP_OPTION_DNS_SERVER 6
#define DHCP_OPTION_REQUESTED_IP 50
#define DHCP_OPTION_LEASE_TIME 51
#define DHCP_OPTION_MSG_TYPE 53
#define DHCP_OPTION_DHCP_SERVER 54
#define DHCP_OPTION_PARAMETER_REQUEST_LIST 55
#define DHCP_OPTION_END 255

#define DHCP_MAGIC_COOKIE 0x63825363

#define BOOTREQUEST 1
#define BOOTREPLY 2

#define DHCP_HTYPE_ETHERNET 1
#define DHCP_HLEN_ETHERNET 6

#define DHCPDISCOVER 1
#define DHCPOFFER 2
#define DHCPREQUEST 3
#define DHCPDECLINE
#define DHCPACK 5
#define DHCPNAK 6
#define DHCPRELEASE 7
#define DHCPINFORM

#define DHCP_OPT_LEN 312

struct dhcp_packet {
  uint8_t op;                     // BOOTREQUEST (1) / BOOTREPLY (2)
  uint8_t htype;                  // 1 (Ethernet)
  uint8_t hlen;                   // 6 (MAC address)
  uint8_t hops;                   // 0
  uint32_t xid;                   // Uniq ID for transaction
  uint16_t secs;                  // 0
  uint16_t flags;                 // BROADCAST-flag
  uint32_t ciaddr;                // curr IP (0, if doesn't)
  uint32_t yiaddr;                // new IP (fils server)
  uint32_t siaddr;                // server IP
  uint32_t giaddr;                // retranslator IP, gateway IP
  uint8_t chaddr[16];             // client MAC-addr
  uint8_t sname[64];              // server name
  uint8_t file[128];              // download file
  uint32_t magic_cookie;          // 0x63825363 (DHCP signature)
  uint8_t options[DHCP_OPT_LEN];  // DHCP options
} __attribute__((packed));

void dhcp_client_run(const char *ifname);

#endif