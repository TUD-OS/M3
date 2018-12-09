/*
 * Copyright (C) 2015-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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
#include "backend/Backend.h"
#include "data/Allocator.h"
#include "sess/OpenFiles.h"

class FSHandle {
public:
    explicit FSHandle(Backend *backend, size_t extend, bool clear, bool revoke_first, size_t max_load);

    m3::SuperBlock &sb() {
        return _sb;
    }
    Backend *backend() {
        return _backend;
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
        _backend->store_sb(_sb);
    }

    void shutdown() {
        _backend->shutdown();
    }

private:
    static bool load_superblock(Backend *backend, m3::SuperBlock *sb, bool clear);

    Backend *_backend;
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
