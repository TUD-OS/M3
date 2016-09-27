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

#include <base/Common.h>

#include <m3/stream/Standard.h>

#include <hash/Hash.h>

using namespace m3;

static const char *names[] = {
    "sha1", "sha224", "sha256", "sha384", "sha512"
};

static char buffer[512];

int main() {
    hash::Hash accel;

    for(size_t j = 0; j < sizeof(buffer); ++j)
        buffer[j] = j;

    for(int i = 0; i < hash::Hash::COUNT; ++i) {
        uint8_t result[64];
        size_t len = accel.get(static_cast<hash::Hash::Algorithm>(i), buffer, sizeof(buffer),
            result, sizeof(result));

        if(len == 0)
            cout << "Error\n";
        else {
            cout << names[i] << " hash: ";
            for(size_t i = 0; i < len; ++i)
                cout << fmt(result[i], "0x", 2);
            cout << "\n";
        }
    }
    return 0;
}
