#ifndef DIE_H
#define DIE_H

#include <stdio.h>
#include <stdlib.h>

#define die(...)                                                               \
  do {                                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

#endif
