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

#include <base/Errors.h>
#include <base/col/DList.h>
#include <base/col/Treap.h>

#include <m3/session/Disk.h>
#include <m3/session/M3FS.h>
#include <m3/stream/Standard.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

#include <fs/internal.h>
#include <thread/ThreadManager.h>

#include "backend/Backend.h"

class BufferHead : public m3::TreapNode<BufferHead, m3::blockno_t>, public m3::DListItem {
    friend class Buffer;
    friend class FileBuffer;
    friend class MetaBuffer;

public:
    BufferHead(m3::blockno_t bno, size_t size);

    bool matches(m3::blockno_t bno) {
        return (key() <= bno) && (bno < key() + _size);
    }

protected:
    // number of blocks
    size_t _size;
    bool locked;
    bool dirty;
    event_t unlock;
};

class Buffer {
public:
    // the PRDT is currently placed behind the data buffer when using DMA
    static constexpr size_t PRDT_SIZE   = 8;

    Buffer(size_t blocksize, Backend *backend);
    virtual ~Buffer(){};
    void mark_dirty(m3::blockno_t bno);
    virtual void flush() = 0;

protected:
    virtual BufferHead *get(m3::blockno_t bno)  = 0;
    virtual void flush_chunk(BufferHead *b) = 0;

    m3::Treap<BufferHead> ht;
    m3::DList<BufferHead> lru;

    size_t _blocksize;
    Backend *_backend;
};
