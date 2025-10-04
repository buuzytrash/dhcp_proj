#include "network_utils.h"

#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/ether.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

void get_mac_addr(const char *ifname, uint8_t *mac) {
  int fd = 0;

  if ((fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0) {
    perror("socket() in get_mac_addr()");
    return;
  }

  struct ifreq ifr;
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);

  if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
    perror("ioctl");
    close(fd);
    return;
  }

  memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
  printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3],
         mac[4], mac[5]);

  close(fd);
}

int create_raw_socket(const char *ifname) {
  int sock = 0;
  if ((sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0) {
    perror("[-] socket()");
    return -1;
  }

  struct ifreq ifr;
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);

  if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
    perror("ioctl SIOCGIFINDEX");
    close(sock);
    return -1;
  }

  struct sockaddr_ll sll;
  memset(&sll, 0, sizeof(sll));
  sll.sll_family = AF_PACKET;
  sll.sll_ifindex = ifr.ifr_ifindex;
  sll.sll_protocol = htons(ETH_P_ALL);

  if (bind(sock, (struct sockaddr *)&sll, sizeof(sll)) < 0) {
    perror("bind");
    close(sock);
    return -1;
  }

  struct packet_mreq mr;
  memset(&mr, 0, sizeof(mr));
  mr.mr_ifindex = sll.sll_ifindex;
  mr.mr_type = PACKET_MR_PROMISC;

  if (setsockopt(sock, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr)) <
      0) {
    perror("setsockopt promiscuous");
    close(sock);
    return -1;
  }

  return sock;
}

void bring_interface_up(const char *ifname) {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  struct ifreq ifr = {0};
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
  ioctl(fd, SIOCGIFFLAGS, &ifr);
  ifr.ifr_flags |= IFF_UP;
  ioctl(fd, SIOCSIFFLAGS, &ifr);
  close(fd);
}

void set_ip_addr(const char *ifname, struct in_addr ip, struct in_addr mask) {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  struct ifreq ifr = {0};
  struct sockaddr_in *addr;

  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);

  addr = (struct sockaddr_in *)&ifr.ifr_addr;
  addr->sin_addr = ip;
  addr->sin_family = AF_INET;

  if (ioctl(fd, SIOCSIFADDR, &ifr) < 0) {
    perror("ioctl SIOCSIFADDR");
  }

  addr->sin_addr = mask;
  if (ioctl(fd, SIOCSIFNETMASK, &ifr) < 0) {
    perror("ioctl SIOCSIFNETMASK");
  }

  close(fd);
}

void add_default_router(const char *ifname, struct in_addr gateway) {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);

  struct rtentry route;
  memset(&route, 0, sizeof(route));

  struct sockaddr_in *addr;

  addr = (struct sockaddr_in *)&route.rt_dst;
  addr->sin_family = AF_INET;
  addr->sin_addr.s_addr = INADDR_ANY;

  addr = (struct sockaddr_in *)&route.rt_genmask;
  addr->sin_family = AF_INET;
  addr->sin_addr.s_addr = INADDR_ANY;

  addr = (struct sockaddr_in *)&route.rt_gateway;
  addr->sin_family = AF_INET;
  addr->sin_addr = gateway;

  route.rt_flags = RTF_UP | RTF_GATEWAY;
  route.rt_dev = (char *)ifname;

  if (ioctl(fd, SIOCADDRT, &route) < 0) {
    perror("ioctl SIOCADDRT");
    close(fd);
    return;
  }

  close(fd);
}

uint16_t checksum(uint16_t *addr, int len) {
  int nleft = len;
  uint32_t sum = 0;
  uint16_t *w = addr;
  uint16_t answer = 0;

  while (nleft > 1) {
    sum += *w++;
    nleft -= 2;
  }

  if (nleft == 1) {
    *(uint8_t *)(&answer) = *(uint8_t *)w;
    sum += answer;
  }

  sum = (sum >> 16) + (sum & 0xffff);
  sum += (sum >> 16);
  answer = ~sum;

  return answer;
}