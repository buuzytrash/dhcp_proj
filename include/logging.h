#ifndef LOGGING_H
#define LOGGING_H

extern int verbose_flag;

#define DEBUG_PRINT(format, ...)                         \
  do {                                                   \
    if (verbose_flag) {                                  \
      fprintf(stderr, "[DEBUG] " format, ##__VA_ARGS__); \
    }                                                    \
  } while (0)

#endif