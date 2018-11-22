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

#include "FSHandle.h"

#include <base/Panic.h>
#include <base/log/Services.h>

#include "data/INodes.h"

using namespace m3;

bool FSHandle::load_superblock(SuperBlock *sb, bool clear, DiskSession *disk) {
    SLOG(FS, "Loading Superblock from disk");
    size_t len  = sizeof(*sb);
    MemGate tmp = MemGate::create_global(512, MemGate::RW);
    KIF::CapRngDesc crd(KIF::CapRngDesc::OBJ, tmp.sel(), 1);
    KIF::ExchangeArgs args;
    args.count   = 2;
    args.vals[0] = static_cast<xfer_t>(0);
    args.vals[1] = static_cast<xfer_t>(1);
    disk->delegate(crd, &args);
    disk->read(0, 0, 1, 512);
    tmp.read(sb, len, 0);

    SLOG(FS, "Superblock:");
    SLOG(FS, "  blocksize=" << sb->blocksize);
    SLOG(FS, "  total_inodes=" << sb->total_inodes);
    SLOG(FS, "  total_blocks=" << sb->total_blocks);
    SLOG(FS, "  free_inodes=" << sb->free_inodes);
    SLOG(FS, "  free_blocks=" << sb->free_blocks);
    SLOG(FS, "  first_free_inode=" << sb->first_free_inode);
    SLOG(FS, "  first_free_block=" << sb->first_free_block);
    if(sb->checksum != sb->get_checksum())
        PANIC("Superblock checksum is invalid. Terminating.");
    return clear;
}

FSHandle::FSHandle(size_t extend, bool clear, bool revoke_first, size_t max_load)
    : _disk(new DiskSession()),
      _clear(load_superblock(&_sb, clear, _disk)),
      _revoke_first(revoke_first),
      _extend(extend),
      _filebuffer(_sb.blocksize, _disk, max_load),
      _metabuffer(_sb.blocksize, _disk),
      _blocks(_sb.first_blockbm_block(), &_sb.first_free_block, &_sb.free_blocks, _sb.total_blocks,
              _sb.blockbm_blocks()),
      _inodes(_sb.first_inodebm_block(), &_sb.first_free_inode, &_sb.free_inodes, _sb.total_inodes,
              _sb.inodebm_blocks()),
      _files(*this) {
}
