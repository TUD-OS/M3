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

MetaBuffer::MetaBuffer(size_t blocksize, Disk *disk)
    : Buffer(blocksize, disk),
      _blocks(new char[_blocksize * META_BUFFER_SIZE]),
      gate(MemGate::create_global(_blocksize + PRDT_SIZE, MemGate::RW)) {
    for(size_t i = 0; i < META_BUFFER_SIZE; i++)
        lru.append(new MetaBufferHead(0, 1, i, _blocks + i * _blocksize));

    // store the MemCap as blockno 0, bc we won't load the superblock again
    KIF::CapRngDesc crd(KIF::CapRngDesc::OBJ, gate.sel(), 1);
    KIF::ExchangeArgs args;
    args.count   = 2;
    args.vals[0] = static_cast<xfer_t>(0);
    args.vals[1] = static_cast<xfer_t>(1);
    _disk->delegate(crd, &args);
}

void *MetaBuffer::get_block(Request &r, blockno_t bno) {
    MetaBufferHead *b;
    while(true) {
        b = get(bno);
        if(b) {
            if(b->locked)
                ThreadManager::get().wait_for(b->unlock);
            else {
                if(b->_linkcount == 0) {
                    lru.remove(b);
                    _size++;
                }
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

    if(_size >= META_BUFFER_SIZE) {
        // this should not happen
        PANIC("MetaBufferCache to small");
        return nullptr;
    }
    _size++;
    b = static_cast<MetaBufferHead*>(lru.removeFirst());
    if(b->key()) {
        ht.remove(b);
        if(b->dirty)
            flush_chunk(b);
    }
    b->key(bno);
    ht.insert(b);

    // disk load into b->_data;
    _disk->read(0, bno, 1, _blocksize, 0);
    gate.read(b->_data, _blocksize, 0);

    b->_linkcount = 1;
    SLOG(FS, "MetaBuffer: Load new block <" << b->key() << ">, Links: " << b->_linkcount);
    b->locked = false;
    ThreadManager::get().notify(b->unlock);

    r.push_meta(b);
    return b->_data;
}

void MetaBuffer::quit(MetaBufferHead *b) {
    assert(b->_linkcount > 0);
    SLOG(FS, "MetaBuffer: Dereferencing block <" << b->key() << ">, Links: " << b->_linkcount);
    b->_linkcount--;
    if(b->_linkcount == 0) {
        // append block to the free list(lru)
        // the block remains inside the ht until a new block needs to be loaded
        lru.append(b);
        _size--;
    }
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
    gate.write(mb->_data, _blocksize, 0);
    _disk->write(0, b->key(), 1, _blocksize, 0);

    b->dirty   = false;
    mb->locked = false;
    ThreadManager::get().notify(mb->unlock);
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
