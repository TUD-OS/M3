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
#include <base/util/Profile.h>

#include <m3/stream/Standard.h>

#include <hash/Hash.h>

using namespace m3;

static const char *names[] = {
    "sha1", "sha224", "sha256", "sha384", "sha512"
};

static const int WARMUP    = 10;
static const int REPEATS   = 50;

static char buffer[4096];

static void print(size_t algo, uint8_t *result, size_t len) {
    if(len == 0)
        cout << "Error\n";
    else {
        cout << names[algo] << " hash: ";
        for(size_t i = 0; i < len; ++i)
            cout << fmt(result[i], "0x", 2);
        cout << "\n";
    }
}

int main(int argc, char **argv) {
    const char *service = nullptr;
    if(argc > 1)
        service = argv[1];

    for(size_t j = 0; j < sizeof(buffer); ++j)
        buffer[j] = j;

    uint8_t result[64];
    hash::Hash *accel = service ? new hash::Hash(service) : new hash::Hash();

    size_t len = accel->get(hash::Hash::Algorithm::SHA256, buffer, sizeof(buffer), result, sizeof(result));
    print(hash::Hash::Algorithm::SHA256, result, len);

    for(int j = 0; j < WARMUP; ++j)
        accel->get(hash::Hash::Algorithm::SHA256, buffer, sizeof(buffer), result, sizeof(result));

    for(int j = 0; j < REPEATS; ++j) {
        Profile::start(0x1234);
        accel->get(hash::Hash::Algorithm::SHA256, buffer, sizeof(buffer), result, sizeof(result));
        Profile::stop(0x1234);
    }
    delete accel;
    return 0;
}
