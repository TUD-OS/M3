/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <m3/stream/Serial.h>
#include <m3/util/Sync.h>
#include <m3/DTU.h>
#include <cstring>

namespace m3 {

int Serial::do_write(const char *str, size_t len) {
    volatile uint *ack = reinterpret_cast<volatile uint*>(SERIAL_ACK);
    char *buffer = reinterpret_cast<char*>(SERIAL_BUF);
    assert(len <= SERIAL_BUFSIZE);
    memcpy(buffer, str, len);
    *ack = len;
    while(*ack != 0)
        ;
    return 0;
}

ssize_t Serial::do_read(char *, size_t) {
    return 0;
}

}
