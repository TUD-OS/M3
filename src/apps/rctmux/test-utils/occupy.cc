/**
* Copyright (C) 2015, René Küttner <rene.kuettner@.tu-dresden.de>
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

#include <base/Common.h>
#include <base/stream/Serial.h>
#include <m3/VPE.h>
#include <m3/Syscalls.h>
#include <base/DTU.h>

using namespace m3;

#define ALLOC_SIZE 128

int main(int, char**)
{
    Serial::get() << "Occupy program started...\n";

    // this program tries to keep the kernel busy 

    while (1) {
        {
            Serial::get() << "Allocating " << ALLOC_SIZE << "bytes...\n";
            MemGate foo = MemGate::create_global(ALLOC_SIZE, MemGate::RWX);
        }
        // freed when out of scope
    }

    return 0;
}
