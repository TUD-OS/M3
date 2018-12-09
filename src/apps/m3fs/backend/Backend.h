/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include "../sess/Request.h"

class Backend {
public:
    virtual ~Backend() {
    }

    virtual void load_meta(void *dst, size_t dst_off, m3::blockno_t bno, event_t unlock) = 0;
    virtual void load_data(m3::MemGate &mem, m3::blockno_t bno, size_t blocks, bool init, event_t unlock) = 0;

    virtual void store_meta(const void *src, size_t src_off, m3::blockno_t bno, event_t unlock) = 0;
    virtual void store_data(m3::blockno_t bno, size_t blocks, event_t unlock) = 0;

    virtual void sync_meta(Request &r, m3::blockno_t bno) = 0;

    virtual size_t get_filedata(Request &r, m3::Extent *ext, size_t extoff, int perms, capsel_t sel,
                                bool dirty, bool load, size_t accessed) = 0;

    virtual void clear_extent(Request &r, m3::Extent *ext, size_t accessed) = 0;

    virtual void load_sb(m3::SuperBlock &sb) = 0;

    virtual void store_sb(m3::SuperBlock &sb) = 0;

    virtual void shutdown() = 0;
};
