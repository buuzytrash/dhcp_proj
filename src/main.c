#include <stdio.h>
#include <stdlib.h>

#include "dhcp.h"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage %s <interface>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  char *ifname = argv[1];
  dhcp_client_run(ifname);

  return 0;
}