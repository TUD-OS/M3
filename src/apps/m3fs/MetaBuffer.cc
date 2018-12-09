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

#include "MetaBuffer.h"

#include <fs/internal.h>

using namespace m3;

MetaBufferHead::MetaBufferHead(blockno_t bno, size_t size, size_t off, char *data)
    : BufferHead(bno, size),
      _off(off),
      _data(data),
      _linkcount(0) {
}

MetaBuffer::MetaBuffer(size_t blocksize, Backend *backend)
    : Buffer(blocksize, backend),
      _blocks(new char[_blocksize * META_BUFFER_SIZE]) {
    for(size_t i = 0; i < META_BUFFER_SIZE; i++)
        lru.append(new MetaBufferHead(0, 1, i, _blocks + i * _blocksize));
}

void *MetaBuffer::get_block(Request &r, blockno_t bno) {
    MetaBufferHead *b;
    while(true) {
        b = get(bno);
        if(b) {
            if(b->locked)
                ThreadManager::get().wait_for(b->unlock);
            else {
                lru.moveToEnd(b);
                b->_linkcount++;
                SLOG(FS, "MetaBuffer: Found cached block <" << b->key() << ">, Links: "
                                                            << b->_linkcount);
                r.push_meta(b);
                return b->_data;
            }
        }
        else
            break;
    }

    // find first non-used block
    for(auto it = lru.begin(); it != lru.end(); ++it) {
        auto mb = static_cast<MetaBufferHead*>(&*it);
        if(mb->_linkcount == 0) {
            b = mb;
            break;
        }
    }

    // write-back, if necessary
    if(b->key()) {
        ht.remove(b);
        if(b->dirty)
            flush_chunk(b);
    }

    b->key(bno);
    ht.insert(b);

    _backend->load_meta(b->_data, b->_off, bno, b->unlock);

    b->_linkcount = 1;
    lru.moveToEnd(b);
    SLOG(FS, "MetaBuffer: Load new block <" << b->key() << ">, Links: " << b->_linkcount);
    b->locked = false;

    r.push_meta(b);
    return b->_data;
}

void MetaBuffer::quit(MetaBufferHead *b) {
    assert(b->_linkcount > 0);
    SLOG(FS, "MetaBuffer: Dereferencing block <" << b->key() << ">, Links: " << b->_linkcount);
    b->_linkcount--;
}

MetaBufferHead *MetaBuffer::get(blockno_t bno) {
    MetaBufferHead *b = static_cast<MetaBufferHead*>(ht.find(bno));
    if(b)
        return b;
    return nullptr;
}

void MetaBuffer::flush_chunk(BufferHead *b) {
    MetaBufferHead *mb = reinterpret_cast<MetaBufferHead*>(b);
    mb->locked         = true;

    // write_to_disk
    SLOG(FS, "MetaBuffer: Write back block <" << b->key() << ">");
    _backend->store_meta(mb->_data, mb->_off, b->key(), mb->unlock);

    b->dirty   = false;
    mb->locked = false;
}

void MetaBuffer::write_back(blockno_t bno) {
    MetaBufferHead *b = get(bno);
    if(b) {
        if(b->dirty)
            flush_chunk(b);
    }
}

void MetaBuffer::flush() {
    while(!ht.empty()) {
        MetaBufferHead *b = reinterpret_cast<MetaBufferHead*>(ht.remove_root());
        if(b->dirty)
            flush_chunk(b);
    }
}

bool MetaBuffer::dirty(blockno_t bno) {
    MetaBufferHead *b = get(bno);
    if(b)
        return b->dirty;
    return false;
}
