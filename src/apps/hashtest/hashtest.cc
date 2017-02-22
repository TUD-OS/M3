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
#include <m3/vfs/VFS.h>

#include <hash/Hash.h>

using namespace m3;
using namespace hash;

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

static const int WARMUP    = 1;
static const int REPEATS   = 2;

static void print(const char *algo, uint8_t *result, size_t len) {
    if(len == 0)
        cout << "Error\n";
    else {
        cout << "sha" << algo << " hash: ";
        for(size_t i = 0; i < len; ++i)
            cout << fmt(result[i], "0x", 2);
        cout << "\n";
    }
}

static size_t gethash(Hash &hash, bool autonomous, Hash::Algorithm algo,
        const char *path, uint8_t *res, size_t max) {
    fd_t fd = VFS::open(path, FILE_R);
    if(fd == FileTable::INVALID)
        exitmsg("Unable to open " << path);

    size_t len;
    if(autonomous)
        len = hash.get_auto(algo, VPE::self().fds()->get(fd), res, max);
    else
        len = hash.get(algo, VPE::self().fds()->get(fd), res, max);
    VFS::close(fd);
    return len;
}

template<bool AUTO, uint NAME>
static void bench(Hash &accel, const char *path) {
    uint8_t res[64];

    for(size_t i = 0; i < ARRAY_SIZE(algos); ++i) {
        size_t len = gethash(accel, AUTO, algos[i].algo, path, res, sizeof(res));
        print(algos[i].name, res, len);

        for(int j = 0; j < WARMUP; ++j)
            gethash(accel, AUTO, algos[i].algo, path, res, sizeof(res));

        for(int j = 0; j < REPEATS; ++j) {
            Profile::start(NAME);
            gethash(accel, AUTO, algos[i].algo, path, res, sizeof(res));
            Profile::stop(NAME);
        }
    }
}

int main(int argc, char **argv) {
    if(argc < 2)
        exitmsg("Usage: " << argv[0] << " <path>");

    if(VFS::mount("/", new M3FS("m3fs")) != Errors::NONE) {
        if(Errors::last != Errors::EXISTS)
            exitmsg("Unable to mount filesystem\n");
    }

    Hash accel;
    bench<false, 0x1234>(accel, argv[1]);
    bench<true,  0x1235>(accel, argv[1]);
    return 0;
}
