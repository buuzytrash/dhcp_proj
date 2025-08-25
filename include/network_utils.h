#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#include <stdint.h>

#define DHCP_PORT_SERVER 67
#define DHCP_PORT_CLIENT 68

void get_mac_addr(const char *ifname, uint8_t *mac);
void set_ip_addr(const char *ifname, uint32_t ip);
int create_dhcp_socket(const char *ifname);
void bring_interface_up(const char *ifname);

#endif