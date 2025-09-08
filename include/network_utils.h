#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#include <stdint.h>
#include <netinet/in.h>

#define DHCP_PORT_SERVER 67
#define DHCP_PORT_CLIENT 68

typedef struct eth_header
{
    uint8_t dst_mac[6];
    uint8_t src_mac[6];
    uint16_t eth_type;
} __attribute__((packed)) eth_header_t;

typedef struct ip_header
{
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

typedef struct udp_header
{
    uint16_t source;
    uint16_t dest;
    uint16_t len;
    uint16_t check;
} __attribute__((packed)) udp_header_t;

void get_mac_addr(const char *ifname, uint8_t *mac);
int create_raw_socket(const char *ifname);
void bring_interface_up(const char *ifname);
uint16_t checksum(uint16_t *addr, int len);

#endif