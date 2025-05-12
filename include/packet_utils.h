#ifndef PACKET_UTILS_H
#define PACKET_UTILS_H

#include "dhcp.h"

void print_dhcp_packet(const struct dhcp_packet *packet, const char *type);
uint32_t get_dhcp_serv_id(uint8_t *options);

#endif