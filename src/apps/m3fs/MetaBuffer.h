/*
 * Copyright (C) 2018, Sebastian Reimers <sebastian.reimers@mailbox.tu-dresden.de>
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

#include <base/col/Treap.h>

#include <m3/com/MemGate.h>
#include <m3/session/Disk.h>

#include <fs/internal.h>

#include "sess/FileSession.h"
#include "Buffer.h"

class MetaBufferHead : public BufferHead {
    friend class MetaBuffer;

public:
    explicit MetaBufferHead(m3::blockno_t bno, size_t size, size_t off, char *data);

private:
    size_t _off;
    void *_data;
    size_t _linkcount;
};

/*
 * stores single blocks
 * lru is the list of free blocks
 * blocks will be freed if no session uses them
 */
class MetaBuffer : public Buffer {
public:
    static constexpr size_t META_BUFFER_SIZE    = 512;

    explicit MetaBuffer(size_t blocksize, Backend *backend);

    void *get_block(Request &r, m3::blockno_t bno, bool dirty = false);
    void quit(MetaBufferHead *b);
    void write_back(m3::blockno_t bno);
    void flush() override;
    bool dirty(m3::blockno_t);

private:
    MetaBufferHead *get(m3::blockno_t bno) override;
    void flush_chunk(BufferHead *b) override;

    char *_blocks;
};
