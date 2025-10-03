#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "dhcp.h"

typedef struct {
  char *interface;
  int verbose;
  int timeout;
  int retries;
  char *log_file;
} client_config_t;

void print_usage(const char *program_name) {
  printf("Usage: %s [OPTIONS] <interface>\n", program_name);
  printf("DHCP Client Implementation\n\n");
  printf("Options:\n");
  printf("  -i, --interface IFACE   Network interface (e.g., eth0)\n");
  printf("  -v, --verbose           Enable verbose output\n");
  printf("  -t, --timeout           Set timeout in seconds (default: 5)\n");
  printf("  -r, --retries           Set number of retries (default: 3)\n");
  printf("  -h, --help              Show this help message\n");
}

int parse_args(int argc, char **argv, client_config_t *config) {
  config->interface = NULL;
  config->verbose = 0;
  config->timeout = 5;
  config->retries = 3;
  config->log_file = NULL;

  struct option long_options[] = {{"help", no_argument, 0, 'h'},
                                  {"interface", required_argument, 0, 'i'},
                                  {"verbose", no_argument, 0, 'v'},
                                  {"timeout", required_argument, 0, 't'},
                                  {"retries", required_argument, 0, 'r'},
                                  {NULL, 0, NULL, 0}};
  int opt;
  int options_index = 0;

  while ((opt = getopt_long(argc, argv, "i:vt:r:h", long_options,
                            &options_index)) != -1) {
    switch (opt) {
      case 'i':
        config->interface = optarg;
        break;
      case 'v':
        config->verbose = 1;
        break;
      case 't':
        config->timeout = atoi(optarg);
        if (config->timeout <= 0) {
          fprintf(stderr, "Error: Timeout must be positive\n");
          return -1;
        }
        break;
      case 'r':
        config->retries = atoi(optarg);
        if (config->retries <= 0) {
          fprintf(stderr, "Error: Retries must be positive\n");
          return -1;
        }
        break;
      case 'h':
        print_usage(argv[0]);
        exit(EXIT_SUCCESS);
        break;
      case '?':
        return -1;
      default:
        fprintf(stderr, "Unexpected error in argument parsing\n");
        return -1;
    }
  }

  if (config->interface == NULL) {
    if (optind < argc) {
      config->interface = argv[optind];
    } else {
      fprintf(stderr, "Error: Interface name is required\n");
      print_usage(argv[0]);
      return -1;
    }
  }

  if (optind < argc - 1) {
    fprintf(stderr, "Error: Too many arguments\n");
    print_usage(argv[0]);
    return -1;
  }

  return 0;
}

int main(int argc, char *argv[]) {
  client_config_t config;

  if (parse_args(argc, argv, &config) != 0) {
    exit(EXIT_FAILURE);
  }

  if (config.verbose) {
    printf("DHCP Client Configuration:\n");
    printf("  Interface: %s\n", config.interface);
    printf("  Verbose: %s\n", config.verbose ? "enabled" : "disabled");
    printf("  Timeout: %d seconds\n", config.timeout);
    printf("  Retries: %d\n", config.retries);
    printf("\n");
  }

  dhcp_client_run(config.interface);

  return 0;
}