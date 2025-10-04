#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#include <netinet/in.h>
#include <stdint.h>

#define DHCP_PORT_SERVER 67
#define DHCP_PORT_CLIENT 68

typedef struct {
  uint8_t dst_mac[6];
  uint8_t src_mac[6];
  uint16_t eth_type;
} __attribute__((packed)) eth_header_t;

typedef struct {
  uint8_t ihl : 4;
  uint8_t version : 4;
  uint8_t tos;
  uint16_t tot_len;
  uint16_t id;
  uint16_t frag_off;
  uint8_t ttl;
  uint8_t protocol;
  uint16_t check;
  uint32_t saddr;
  uint32_t daddr;
} __attribute__((packed)) ip_header_t;

typedef struct {
  uint16_t source;
  uint16_t dest;
  uint16_t len;
  uint16_t check;
} __attribute__((packed)) udp_header_t;

void get_mac_addr(const char *ifname, uint8_t *mac);
int create_raw_socket(const char *ifname);
void bring_interface_up(const char *ifname);
uint16_t checksum(uint16_t *addr, int len);

void set_ip_addr(const char *ifname, struct in_addr ip, struct in_addr mask);
void add_default_router(const char *ifname, struct in_addr gateway);

#endif