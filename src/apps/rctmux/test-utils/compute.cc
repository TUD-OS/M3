/**
* Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
* Economic rights: Technische Universität Dresden (Germany)
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

#include <base/stream/IStringStream.h>
#include <base/CPU.h>

using namespace m3;

int main(int argc, char **argv) {
    cycles_t cycles = 1000000;
    if(argc > 1)
        cycles = IStringStream::read_from<cycles_t>(argv[1]);

    CPU::compute(cycles);
    return 0;
}
