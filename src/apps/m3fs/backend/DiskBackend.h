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

#include <m3/session/Disk.h>

#include "Backend.h"
#include "../sess/Request.h"
#include "../FSHandle.h"
#include "../MetaBuffer.h"

class DiskBackend : public Backend {
public:
    explicit DiskBackend(size_t dev)
        : _blocksize(),
          _disk(new m3::Disk("disk", dev)),
          _metabuf() {
    }

    void load_meta(void *dst, size_t dst_off, m3::blockno_t bno, event_t unlock) override {
        size_t off = dst_off * (_blocksize + MetaBuffer::PRDT_SIZE);
        _disk->read(0, bno, 1, _blocksize, off);
        _metabuf->read(dst, _blocksize, off);
        m3::ThreadManager::get().notify(unlock);
    }
    void load_data(m3::MemGate &mem, m3::blockno_t bno, size_t blocks, bool init, event_t unlock) override {
        delegate_mem(mem, bno, blocks);
        if(init)
            _disk->read(bno, bno, blocks, _blocksize);
        m3::ThreadManager::get().notify(unlock);
    }

    void store_meta(const void *src, size_t src_off, m3::blockno_t bno, event_t unlock) override {
        size_t off = src_off * (_blocksize + MetaBuffer::PRDT_SIZE);
        _metabuf->write(src, _blocksize, off);
        _disk->write(0, bno, 1, _blocksize, off);
        m3::ThreadManager::get().notify(unlock);
    }
    void store_data(m3::blockno_t bno, size_t blocks, event_t unlock) override {
        _disk->write(bno, bno, blocks, _blocksize);
        m3::ThreadManager::get().notify(unlock);
    }

    void sync_meta(Request &r, m3::blockno_t bno) override {
        // check if there is a filebuffer entry for it or create one
        capsel_t msel = m3::VPE::self().alloc_sel();
        size_t ret = r.hdl().filebuffer().get_extent(bno, 1, msel, m3::MemGate::RWX, 1, false);
        if(ret) {
            // okay, so write it from metabuffer to filebuffer
            m3::MemGate m = m3::MemGate::bind(msel);
            m.write(r.hdl().metabuffer().get_block(r, bno), r.hdl().sb().blocksize, 0);
            r.pop_meta();
        }
        // if the filebuffer entry didn't exist and couldn't be created, update block on disk
        else
            r.hdl().metabuffer().write_back(bno);
    }

    size_t get_filedata(Request &r, m3::Extent *ext, size_t extoff, int perms, capsel_t sel,
                        bool dirty, bool load, size_t accessed) override {
        size_t first_block = extoff / _blocksize;
        return r.hdl().filebuffer().get_extent(ext->start + first_block,
                                               ext->length - first_block,
                                               sel, perms, accessed, load, dirty);
    }

    void clear_extent(Request &r, m3::Extent *ext, size_t accessed) override {
        alignas(64) static char zeros[m3::MAX_BLOCK_SIZE];
        capsel_t sel = m3::VPE::self().alloc_sel();
        size_t i = 0;
        while(i < ext->length) {
            // since we override everything with zeros we don't have to load from the disk
            size_t bytes = r.hdl().filebuffer().get_extent(ext->start + i, ext->length - i,
                                                           sel, m3::MemGate::RW, accessed,
                                                           false, true);
            m3::MemGate mem = m3::MemGate::bind(sel);
            mem.write(zeros, bytes, 0);
            i += bytes / _blocksize;
        }
    }

    void load_sb(m3::SuperBlock &sb) override {
        m3::MemGate tmp = m3::MemGate::create_global(512 + Buffer::PRDT_SIZE, m3::MemGate::RW);
        delegate_mem(tmp, 0, 1);

        // read super block
        _disk->read(0, 0, 1, 512);
        tmp.read(&sb, sizeof(sb), 0);

        // use separate transfer buffer for each entry to allow parallel disk requests
        _blocksize = sb.blocksize;
        size_t size = (_blocksize + MetaBuffer::PRDT_SIZE) * MetaBuffer::META_BUFFER_SIZE;
        _metabuf = new m3::MemGate(m3::MemGate::create_global(size, m3::MemGate::RW));
        // store the MemCap as blockno 0, bc we won't load the superblock again
        delegate_mem(*_metabuf, 0, 1);
    }

    void store_sb(m3::SuperBlock &sb) override {
        m3::MemGate tmp = m3::MemGate::create_global(512 + Buffer::PRDT_SIZE, m3::MemGate::RW);
        delegate_mem(tmp, 0, 1);

        // write back super block
        sb.checksum = sb.get_checksum();
        tmp.write(&sb, sizeof(sb), 0);
        _disk->write(0, 0, 1, 512);
    }

    void shutdown() override {
        // close disk session
        delete _disk;
        _disk = nullptr;
    }

private:
    void delegate_mem(m3::MemGate &mem, m3::blockno_t bno, size_t len) {
        m3::KIF::CapRngDesc crd(m3::KIF::CapRngDesc::OBJ, mem.sel(), 1);
        m3::KIF::ExchangeArgs args;
        args.count   = 2;
        args.vals[0] = static_cast<xfer_t>(bno);
        args.vals[1] = static_cast<xfer_t>(len);
        _disk->delegate(crd, &args);
    }

    size_t _blocksize;
    m3::Disk *_disk;
    m3::MemGate *_metabuf;
};
