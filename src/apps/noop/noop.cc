/*
 * Copyright (C) 2015-2016, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <m3/stream/Standard.h>

int main() {
    /* on the host, we rely on Config to get linked in. this doesn't happen because the linker
     * thinks it's not used. since this does only occur if nothing of the M3 library is used
     * (which would prevent to do anything useful), we fix that here by printing something */
#if defined(__host__)
    m3::cout << "Hello World!\n";
#endif
    return 0;
}
