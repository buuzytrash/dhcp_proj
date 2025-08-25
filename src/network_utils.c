#include "network_utils.h"

#include <aio.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

void get_mac_addr(const char *ifname, uint8_t *mac) {
    int fd = 0;
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket() in get_mac_addr()");
        return;
    }
    struct ifreq ifr;
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ioctl(fd, SIOCGIFHWADDR, &ifr);
    memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
    close(fd);
}

void set_ip_addr(const char *ifname, uint32_t ip) {
    int fd = 0;
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket() in set_ip_addr()");
        return;
    }
    struct ifreq ifr;
    struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;

    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = ip;

    ioctl(fd, SIOCSIFADDR, &ifr);
    close(fd);
}

int create_dhcp_socket(const char *ifname) {
    int sock = 0;
    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("[-] socket()");
        return -1;
    }

    int broadcast = 1;
    if ((setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast,
                    sizeof(broadcast))) < 0) {
        perror("[-] setsockopt(SO_BROADCAST)");
        close(sock);
        return -1;
    }

    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("[-] setsockopt(SO_REUSEADDR)");
        close(sock);
        return -1;
    }

    struct timeval tv = {5, 0};
    if ((setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))) < 0) {
        perror("[-] setsockopt(SO_RCVTIMEO)");
        close(sock);
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);

    if ((setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr))) <
        0) {
        perror("[-] setsockopt(SO_BINDTODEVICE)");
        close(sock);
        return -1;
    }

    struct sockaddr_in client_addr = {0};
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(DHCP_PORT_CLIENT);
    // client_addr.sin_addr.s_addr = INADDR_ANY;
    client_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    if (bind(sock, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("[-] bind()");
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