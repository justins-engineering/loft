#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include <stdio.h>

#define PRINTDBG(f_, ...) fprintf(stderr, "%s" f_ "%s", "\033[38;5;87m", ##__VA_ARGS__, "\033[0m\n")

#define PRINTERR(f_, ...) fprintf(stderr, "%s" f_ "%s", "\033[38;5;9m", ##__VA_ARGS__, "\033[0m\n")

#define PRINTALRT(f_, ...) \
  fprintf(stderr, "%s" f_ "%s", "\033[38;5;226m", ##__VA_ARGS__, "\033[0m\n")

#define PRINTSUCCES(f_, ...) \
  fprintf(stderr, "%s" f_ "%s", "\033[38;5;82m", ##__VA_ARGS__, "\033[0m\n")

typedef struct CharBuff {
  char *response;
  size_t size;
} CharBuff;

#endif  // DEFINITIONS_H
