/*
 * Copyright (C) 2017, Georg Kotheimer <georg.kotheimer@mailbox.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
 *
 * M3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * M3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

int printf_adapter(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#define LWIP_PLATFORM_DIAG(x) do {printf_adapter x;} while(0)

#define LWIP_PLATFORM_ASSERT(x) do {printf_adapter("Assertion \"%s\" failed at line %d in %s\n", \
                                     x, __LINE__, __FILE__); exit(1);} while(0)

#endif /* LWIP_ARCH_CC_H */
