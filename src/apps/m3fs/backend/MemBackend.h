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

#include <fs/internal.h>

#include "Backend.h"
#include "../sess/Request.h"
#include "../FSHandle.h"
#include "../MetaBuffer.h"

class MemBackend : public Backend {
public:
    explicit MemBackend(size_t fsoff, size_t fssize)
        : _blocksize(),
          _mem(m3::MemGate::create_global_for(fsoff, fssize, m3::MemGate::RWX)) {
    }

    void load_meta(void *dst, size_t, m3::blockno_t bno, event_t) override {
        _mem.read(dst, _blocksize, bno * _blocksize);
    }
    void load_data(m3::MemGate &, m3::blockno_t, size_t, bool, event_t) override {
        // unused
    }

    void store_meta(const void *src, size_t, m3::blockno_t bno, event_t) override {
        _mem.write(src, _blocksize, bno * _blocksize);
    }
    void store_data(m3::blockno_t, size_t, event_t) override {
        // unused
    }

    size_t get_filedata(Request &, m3::Extent *ext, size_t extoff, int perms, capsel_t sel,
                        bool, bool, size_t) override {
        size_t first_block = extoff / _blocksize;
        size_t bytes = (ext->length - first_block) * _blocksize;
        if(m3::Syscalls::get().derivemem(sel, _mem.sel(), (ext->start + first_block) * _blocksize,
                                         bytes, perms) != m3::Errors::NONE) {
            return 0;
        }
        return bytes;
    }

    void clear_extent(Request &, m3::Extent *ext, size_t) override {
        alignas(64) static char zeros[m3::MAX_BLOCK_SIZE];
        for(uint32_t i = 0; i < ext->length; ++i)
            _mem.write(zeros, _blocksize, (ext->start + i) * _blocksize);
    }

    void load_sb(m3::SuperBlock &sb) override {
        _mem.read(&sb, sizeof(sb), 0);
        _blocksize = sb.blocksize;
    }

    void store_sb(m3::SuperBlock &sb) override {
        sb.checksum = sb.get_checksum();
        _mem.write(&sb, sizeof(sb), 0);
    }

    void shutdown() override {
    }

private:
    size_t _blocksize;
    m3::MemGate _mem;
};
