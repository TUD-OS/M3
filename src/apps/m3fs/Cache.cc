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

#include <base/log/Services.h>

#include <limits>

#include "Cache.h"

Cache::Cache(m3::MemGate &mem, size_t blocksize)
    : _mem(mem), _blocksize(blocksize), _data(new char[BLOCK_COUNT * _blocksize]),
      _timestamp(), _blocks() {
}

void *Cache::get_block(m3::blockno_t bno, bool write) {
    _timestamp++;
    BlockInfo *b = get(bno);
    if(b) {
        b->timestamp = _timestamp;
        b->dirty |= write;
        return _data + static_cast<size_t>(b - _blocks) * _blocksize;
    }

    // find the least recently used block
    unsigned mints = std::numeric_limits<unsigned>::max();
    size_t lru = 0;
    for(size_t i = 0; i < BLOCK_COUNT; ++i) {
        if(_blocks[i].bno == 0) {
            lru = i;
            break;
        }
        else if(_blocks[i].timestamp < mints) {
            lru = i;
            mints = _blocks[i].timestamp;
        }
    }

    // if its dirty, write it back to global memory
    if(_blocks[lru].bno != 0 && _blocks[lru].dirty) {
        _mem.write(_data + lru * _blocksize, _blocksize, _blocks[lru].bno * _blocksize);
        _blocks[lru].dirty = false;
    }

    // read desired block
    _mem.read(_data + lru * _blocksize, _blocksize, bno * _blocksize);
    _blocks[lru].timestamp = _timestamp;
    _blocks[lru].bno = bno;
    _blocks[lru].dirty |= write;
    return _data + lru * _blocksize;
}

void Cache::mark_dirty(m3::blockno_t bno) {
    BlockInfo *b = get(bno);
    if(b)
        b->dirty = true;
}

void Cache::write_back(m3::blockno_t bno) {
    BlockInfo *b = get(bno);
    if(b && b->dirty)
        flush_block(static_cast<size_t>(b - _blocks));
}

void Cache::flush() {
    for(size_t i = 0; i < BLOCK_COUNT; ++i) {
        if(_blocks[i].dirty)
            flush_block(i);
    }
}

Cache::BlockInfo *Cache::get(m3::blockno_t bno) {
    for(size_t i = 0; i < BLOCK_COUNT; ++i) {
        if(_blocks[i].bno == bno)
            return _blocks + i;
    }
    return nullptr;
}

void Cache::flush_block(size_t i) {
    SLOG(FS, "Writing block " << _blocks[i].bno << " to DRAM");
    _mem.write(_data + i * _blocksize, _blocksize, _blocks[i].bno * _blocksize);
    _blocks[i].dirty = false;
}
