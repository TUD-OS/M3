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

#include <m3/session/Disk.h>

#include "FileBuffer.h"
#include "MetaBuffer.h"
#include "data/Allocator.h"
#include "sess/OpenFiles.h"

class FSHandle {
public:
    explicit FSHandle(size_t extend, bool clear, bool revoke_first, size_t max_load);

    m3::SuperBlock &sb() {
        return _sb;
    }
    FileBuffer &filebuffer() {
        return _filebuffer;
    }
    MetaBuffer &metabuffer() {
        return _metabuffer;
    }
    Allocator &inodes() {
        return _inodes;
    }
    Allocator &blocks() {
        return _blocks;
    }
    OpenFiles &files() {
        return _files;
    }
    bool revoke_first() const {
        return _revoke_first;
    }
    bool clear_blocks() const {
        return _clear;
    }
    size_t extend() const {
        return _extend;
    }

    void flush_buffer() {
        _metabuffer.flush();
        _filebuffer.flush();

        // write back super block
        _sb.checksum = _sb.get_checksum();
        size_t len = sizeof(_sb);
        m3::MemGate tmp = m3::MemGate::create_global(512 + Buffer::PRDT_SIZE, m3::MemGate::RW);
        m3::KIF::CapRngDesc crd(m3::KIF::CapRngDesc::OBJ, tmp.sel(), 1);
        m3::KIF::ExchangeArgs args;
        args.count = 2;
        args.vals[0] = static_cast<xfer_t>(0);
        args.vals[1] = static_cast<xfer_t>(1);
        _disk->delegate(crd, &args);
        tmp.write(&_sb, len, 0);
        _disk->write(0, 0, 1, 512);
    }

    void shutdown() {
        // close disk session
        delete _disk;
        _disk = nullptr;
    }

private:
    static bool load_superblock(m3::SuperBlock *sb, bool clear, m3::Disk *disk);

    m3::Disk *_disk;
    bool _clear;
    bool _revoke_first;
    size_t _extend;
    m3::SuperBlock _sb;
    FileBuffer _filebuffer;
    MetaBuffer _metabuffer;
    Allocator _blocks;
    Allocator _inodes;
    OpenFiles _files;
    void *_parent_sess;
};
