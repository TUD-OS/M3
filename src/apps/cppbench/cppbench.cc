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

#include <m3/stream/Standard.h>

#include "cppbench.h"

int main() {
    RUN_SUITE(bdlist);
    RUN_SUITE(bslist);
    RUN_SUITE(btreap);
    RUN_SUITE(bregfile);
    RUN_SUITE(bmemgate);
    RUN_SUITE(bsyscall);
    RUN_SUITE(bpipe);
    RUN_SUITE(bfsmeta);

    m3::cout << "\033[1;32mAll tests successful!\033[0;m\n";
    return 0;
}
