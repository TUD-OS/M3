/*
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/stream/Serial.h>
#include <base/util/Math.h>
#include <base/util/Sync.h>
#include <base/DTU.h>
#include <base/Machine.h>
#include <cstring>

namespace m3 {

void Machine::shutdown() {
    while(1)
        asm volatile ("waiti 0");
}

int Machine::write(const char *str, size_t len) {
    volatile uint *ack = reinterpret_cast<volatile uint*>(SERIAL_ACK);
    char *buffer = reinterpret_cast<char*>(SERIAL_BUF);
    assert(len <= SERIAL_BUFSIZE);
    memcpy(buffer, str, len);
    *ack = len;
    while(*ack != 0)
        ;
    return 0;
}

ssize_t Machine::read(char *dst, UNUSED size_t max) {
    volatile uint *wait = reinterpret_cast<volatile uint*>(SERIAL_INWAIT);
    char *buffer = reinterpret_cast<char*>(SERIAL_BUF);
    assert(max <= SERIAL_BUFSIZE);
    *wait = 1;
    while(*wait)
        ;
    size_t len = strlen(buffer);
    memcpy(dst, buffer, Math::min(max, len));
    return len;
}

}
