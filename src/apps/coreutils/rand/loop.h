#pragma once

#include <base/Common.h>

#define BUFFER_SIZE     8192
#define EL_COUNT        (BUFFER_SIZE / sizeof(rand_type))

typedef unsigned char rand_type;

EXTERN_C NOINLINE void generate(rand_type *buffer, unsigned long amount);
