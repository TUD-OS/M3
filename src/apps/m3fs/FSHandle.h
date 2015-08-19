/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
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

#pragma once

#include <fs/internal.h>
#include "Allocator.h"
#include "Cache.h"

class FSHandle {
public:
    explicit FSHandle(capsel_t mem);

    const m3::MemGate &mem() const {
        return _mem;
    }
    m3::SuperBlock &sb() {
        return _sb;
    }
    Cache &cache() {
        return _cache;
    }
    Allocator &inodes() {
        return _inodes;
    }
    Allocator &blocks() {
        return _blocks;
    }

    void read_from_block(void *buffer, size_t len, m3::blockno_t bno, size_t off) {
        _mem.read_sync(buffer, len, bno * _sb.blocksize + off);
    }
    void write_to_block(const void *buffer, size_t len, m3::blockno_t bno, size_t off) {
        _mem.write_sync(buffer, len, bno * _sb.blocksize + off);
    }

    void flush_cache() {
        _cache.flush();
        _sb.checksum = _sb.get_checksum();
        _mem.write_sync(&_sb, sizeof(_sb), 0);
    }

private:
    static bool load_superblock(m3::MemGate &mem, m3::SuperBlock *sb);

    m3::MemGate _mem;
    bool _dummy;
    m3::SuperBlock _sb;
    Cache _cache;
    Allocator _blocks;
    Allocator _inodes;
};
