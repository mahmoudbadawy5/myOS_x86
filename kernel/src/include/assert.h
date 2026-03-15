#include <stdio.h>

#define ASSERT(cond, ...) \
    if (!(cond)) panic(__VA_ARGS__)