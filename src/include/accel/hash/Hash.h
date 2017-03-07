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

#pragma once

#include <m3/com/RecvGate.h>
#include <m3/com/SendGate.h>
#include <m3/session/Session.h>
#include <m3/vfs/File.h>

#include <accel/hash/HashAccel.h>

namespace accel {

/**
 * The hash accelerator allows to generate SHA1, SHA224, SHA256, SHA384 and SHA512 hashes for
 * arbitrary large amounts of data. Hashes are generated in a start-update-finish fashion.
 */
class Hash {
public:
    typedef HashAccel::Algorithm Algorithm;

    /**
     * Instantiates the hash accelerator
     */
    explicit Hash();
    ~Hash();

    /**
     * Starts a hash generation. In autonomous mode, the accelerator expects a memory EP and loads
     * the data directly from there. In that case, the update method that takes a memory capability
     * needs to be used. Otherwise, the accelerator expects the data to be already in its local
     * memory, which is done by the update method taking a pointer.
     *
     * @param autonomous whether the accelerator should work autonomous
     * @param algo the hash algorithm to use
     * @return true if successful
     */
    bool start(bool autonomous, Algorithm algo) {
        _lastmem = m3::ObjCap::INVALID;
        return sendRequest(HashAccel::Command::INIT, autonomous, algo) == 1;
    }

    /**
     * Updates the hash for the data denoted by the given memory capability.
     *
     * @param mem the memory capability
     * @param offset the offset within that memcap
     * @param len the number of bytes to read from the memcap
     * @return true if successful
     */
    bool update(capsel_t mem, size_t offset, size_t len);

    /**
     * Updates the hash for the given data.
     *
     * @param data the data to hash
     * @param len the number of bytes
     * @return true if successful
     */
    bool update(const void *data, size_t len);

    /**
     * Finishes the current hash generation.
     *
     * @param res the array to store the hash to
     * @param max the size of the array
     * @return the number of bytes of the hash (or 0 on error)
     */
    size_t finish(void *res, size_t max);

    /**
     * Convenience method that calls start, update and finish for the given file. The method get()
     * works autonomously and get_slow() works non-autonomously.
     *
     * @param algo the hash algorithm to use
     * @param file the file to generate the hash for
     * @param res the array to store the hash to
     * @param max the size of the array
     * @return the number of bytes of the hash (or 0 on error)
     */
    size_t get(Algorithm algo, m3::File *file, void *res, size_t max);
    size_t get_slow(Algorithm algo, m3::File *file, void *res, size_t max);

    /**
     * Convenience method that calls start, update and finish for the given data. It works
     * non-autonomously.
     *
     * @param algo the hash algorithm to use
     * @param data the data to hash
     * @param len the number of bytes
     * @param res the array to store the hash to
     * @param max the size of the array
     * @return the number of bytes of the hash (or 0 on error)
     */
    size_t get(Algorithm algo, const void *data, size_t len, void *res, size_t max);

private:
    uint64_t sendRequest(HashAccel::Command cmd, uint64_t arg1, uint64_t arg2);

    HashAccel *_accel;
    capsel_t _lastmem;
    m3::RecvGate _rgate;
    m3::RecvGate _srgate;
    m3::SendGate _sgate;
};

}
