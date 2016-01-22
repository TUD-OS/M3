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

#include <m3/stream/Serial.h>
#include <m3/Config.h>
#include <m3/Machine.h>

#include <sys/file.h>
#include <unistd.h>

namespace m3 {

void Machine::shutdown() {
    exit(EXIT_FAILURE);
}

int Machine::write(const char *str, size_t len) {
    return ::write(Config::get().log_fd(), str, len);
}

ssize_t Machine::read(char *buf, size_t len) {
    return ::read(STDIN_FILENO, buf, len);
}

}
