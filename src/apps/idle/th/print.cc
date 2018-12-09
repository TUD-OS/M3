/*
 * Copyright (C) 2016, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/Common.h>
#include <base/Config.h>
#include <base/DTU.h>

#include "print.h"

static void memcpy(void *dest, const void *src, size_t len) {
    char *d = reinterpret_cast<char*>(dest);
    const char *s = reinterpret_cast<const char*>(src);
    while(len-- > 0)
        *d++ = *s++;
}

static void memset(void *dest, int val, size_t len) {
    char *d = reinterpret_cast<char*>(dest);
    while(len-- > 0)
        *d++ = val;
}

char *fmtu(char *buffer, ulong n, uint base) {
    if(n >= base)
        buffer = fmtu(buffer, n / base, base);
    *buffer = "0123456789ABCDEF"[n % base];
    return buffer + 1;
}

int print(const char *str, size_t len) {
#if defined(__t2__)
    volatile uint *ack = reinterpret_cast<volatile uint*>(SERIAL_ACK);
    char *buffer = reinterpret_cast<char*>(SERIAL_BUF);
    assert((len & (DTU_PKG_SIZE - 1)) == 0);
    assert(len <= SERIAL_BUFSIZE);
    memcpy(buffer, str, len);
    *ack = len;
    while(*ack != 0)
        ;
    return 0;
#else
    register int a2 __asm__ ("a2") = 4;
    register int a3 __asm__ ("a3") = 1;
    register const char *a4 __asm__ ("a4") = str;
    register size_t a5 __asm__ ("a5") = len;
    register int ret_val __asm__ ("a2");
    register int ret_err __asm__ ("a3");
    asm volatile ("simcall"
            : "=a" (ret_val), "=a" (ret_err)
            : "a" (a2), "a" (a3), "a" (a4), "a" (a5));
    return ret_err;
#endif
}

void notify(uint32_t arg) {
    char buffer[16];
    memset(buffer, ' ', sizeof(buffer));
    fmtu(buffer, arg, 16);
    buffer[sizeof(buffer) - 1] = '\n';
    print(buffer, sizeof(buffer));
}
