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

#include <fs/internal.h>

#include "Buffer.h"

#define FILE_BUFFER_SIZE    16384 // at least 128
#define LOAD_LIMIT          128

using namespace m3;

struct InodeExt : public DListItem {
    blockno_t _start;
    size_t _size;

    explicit InodeExt(blockno_t start, size_t size)
        : DListItem(),
          _start(start),
          _size(size) {
    }
};

class FileBufferHead : public BufferHead {
    friend class FileBuffer;

public:
    explicit FileBufferHead(blockno_t bno, size_t size, size_t blocksize);

private:
    MemGate _data;
    DList<InodeExt> _extents;
};

class FileBuffer : public Buffer {
public:
    explicit FileBuffer(size_t blocksize, DiskSession *disk, size_t max_load);

    size_t get_extent(blockno_t bno, size_t size, capsel_t sel, int perms, size_t accessed,
                      bool load = true, bool check = false);
    void flush() override;

private:
    FileBufferHead *get(blockno_t bno) override;
    void flush_chunk(BufferHead *b) override;

    size_t _max_load;
};
