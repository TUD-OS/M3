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
#include <base/util/Time.h>

#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>
#include <m3/VPE.h>

#include <accel/hash/Hash.h>

using namespace m3;
using namespace accel;

static const struct {
    const char *name;
    Hash::Algorithm algo;
} algos[] = {
    {"1",   Hash::Algorithm::SHA1},
    {"224", Hash::Algorithm::SHA224},
    {"256", Hash::Algorithm::SHA256},
    {"384", Hash::Algorithm::SHA384},
    {"512", Hash::Algorithm::SHA512},
};

static void print(uint8_t *result, size_t len, const char *src) {
    for(size_t i = 0; i < len; ++i)
        cout << fmt(result[i], "0x", 2);
    cout << " " << src << "\n";
}

int main(int argc, char **argv) {
    Hash::Algorithm algo = Hash::Algorithm::SHA1;
    uint8_t res[64];
    Hash accel;

    int first = 1;
    if(argc > 2 && strcmp(argv[1], "-a") == 0) {
        for(size_t i = 0; i < ARRAY_SIZE(algos); ++i) {
            if(strcmp(argv[2], algos[i].name) == 0) {
                algo = algos[i].algo;
                break;
            }
        }
        first += 2;
    }

    size_t len;
    if(first == argc) {
        len = accel.get(algo, VPE::self().fds()->get(STDIN_FD), res, sizeof(res));
        print(res, len, "-");
    }
    else {
        for(int i = first; i < argc; ++i) {
            fd_t fd = VFS::open(argv[i], FILE_R);
            if(fd == FileTable::INVALID) {
                errmsg("Unable to open " << argv[i]);
                continue;
            }

            len = accel.get(algo, VPE::self().fds()->get(fd), res, sizeof(res));
            print(res, len, argv[i]);
            VFS::close(fd);
        }
    }
    return 0;
}
