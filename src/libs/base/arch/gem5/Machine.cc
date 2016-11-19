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

#include <base/Machine.h>
#include <base/DTU.h>

#include <stdlib.h>
#include <unistd.h>

EXTERN_C void gem5_shutdown(uint64_t delay);
EXTERN_C void gem5_writefile(const char *str, uint64_t len, uint64_t offset, uint64_t file);
EXTERN_C ssize_t gem5_readfile(char *dst, uint64_t max, uint64_t offset);

namespace m3 {

void Machine::shutdown() {
    gem5_shutdown(0);
    UNREACHED;
}

int Machine::write(const char *str, size_t len) {
    DTU::get().print(str, len);

    static const char *fileAddr = "stdout";
    gem5_writefile(str, len, 0, reinterpret_cast<uint64_t>(fileAddr));
    return 0;
}

ssize_t Machine::read(char *dst, size_t max) {
    return gem5_readfile(dst, max, 0);
}

}
