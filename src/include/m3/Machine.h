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

#pragma once

#include <m3/Common.h>

namespace m3 {

/**
 * Low-level machine-specific functions.
 */
class Machine {
    Machine() = delete;

public:
    /**
     * Shuts down the machine
     */
    static NORETURN void shutdown();

    /**
     * Writes <len> bytes from <str> to the serial device.
     *
     * @param str the string to write
     * @param len the length of the string
     * @return 0 on success
     */
    static int write(const char *str, size_t len);

    /**
     * Reads at most <len> bytes into <buf> from the serial device.
     *
     * @param buf the buffer to write to
     * @param len the length of the buffer
     * @return the number of read bytes on success
     */
    static ssize_t read(char *buf, size_t len);
};

}
