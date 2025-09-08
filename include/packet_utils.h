#ifndef PACKET_UTILS_H
#define PACKET_UTILS_H

#include "dhcp.h"
#include <stdint.h>

void create_dhcp_packet(dhcp_packet_t *packet, uint8_t *mac, uint32_t xid, uint8_t msg_type);
void create_header(uint8_t *buffer, uint8_t *src_mac, uint8_t *dst_mac, uint32_t src_ip, uint32_t dst_ip, uint16_t src_port, uint16_t dst_port, uint16_t udp_len);
int send_dhcp_packet(int sock, uint8_t *src_mac, dhcp_packet_t *dhcp_packet, uint32_t xid, uint8_t msg_type, const char *ifname);
int receive_dhcp_packet(int sock, dhcp_packet_t *dhcp_packet, uint32_t expected_xid);

void print_dhcp_packet(const dhcp_packet_t *packet, const char *type);

#endif