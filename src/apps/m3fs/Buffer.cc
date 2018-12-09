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

#include "Buffer.h"

#include <fs/internal.h>

using namespace m3;

BufferHead::BufferHead(blockno_t bno, size_t size)
    : TreapNode(bno),
      DListItem(),
      _size(size),
      locked(true),
      dirty(false),
      unlock(ThreadManager::get().get_wait_event()) {
}

Buffer::Buffer(size_t blocksize, Backend *backend)
    : ht(),
      lru(),
      _blocksize(blocksize),
      _backend(backend) {
}

void Buffer::mark_dirty(blockno_t bno) {
    BufferHead *b = get(bno);
    if(b)
        b->dirty = true;
}
