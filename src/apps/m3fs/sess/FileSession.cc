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

#include <m3/session/M3FS.h>
#include <m3/Syscalls.h>

#include "../data/INodes.h"
#include "FileSession.h"
#include "MetaSession.h"

using namespace m3;

M3FSFileSession::M3FSFileSession(capsel_t srv, M3FSMetaSession *_meta, const m3::String &_filename,
                                 int _flags, const m3::INode &_inode)
    : M3FSSession(), extent(), extoff(), lastoff(), extlen(), filename(_filename),
      epcap(ObjCap::INVALID), sess(m3::VPE::self().alloc_caps(2)),
      sgate(m3::SendGate::create(&_meta->rgate(), reinterpret_cast<label_t>(this), MSG_SIZE, nullptr, sess + 1)),
      oflags(_flags), xstate(TransactionState::NONE), inode(_inode),
      last(ObjCap::INVALID), capscon(), meta(_meta) {
    Syscalls::get().createsessat(sess, srv, reinterpret_cast<word_t>(this));
}

M3FSFileSession::~M3FSFileSession() {
    SLOG(FS, fmt((word_t)this, "#x") << ": file::close(path=" << filename << ")");

    do_commit(extent, extoff);
    meta->remove_file(this);

    if(last != ObjCap::INVALID)
        VPE::self().revoke(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, last, 1));
}

Errors::Code M3FSFileSession::clone(capsel_t srv, KIF::Service::ExchangeData &data) {
    SLOG(FS, fmt((word_t)this, "#x") << ": file::clone(path=" << filename << ")");

    auto nfile =  new M3FSFileSession(srv, meta, filename, oflags, inode);

    data.args.count = 0;
    data.caps = KIF::CapRngDesc(KIF::CapRngDesc::OBJ, nfile->sess, 2).value();

    return Errors::NONE;
}

Errors::Code M3FSFileSession::get_locs(KIF::Service::ExchangeData &data) {
    EVENT_TRACER_FS_getlocs();
    if(data.args.count != 3)
        return Errors::INV_ARGS;

    size_t offset = data.args.vals[0];
    size_t count = data.args.vals[1];
    uint flags = data.args.vals[2];

    SLOG(FS, fmt((word_t)this, "#x") << ": file::get_locs(path=" << filename
        << ", offset=" << offset << ", count=" << count
        << ", flags=" << fmt(flags, "#x") << ")");

    if(count == 0) {
        SLOG(FS, fmt((word_t)this, "#x") << ": Invalid request");
        return Errors::INV_ARGS;
    }

    // don't try to extend the file, if we're not writing
    if(~oflags & FILE_W)
        flags &= ~static_cast<uint>(M3FS::EXTEND);

    // determine extent from byte offset
    size_t firstOff = 0;
    if(flags & M3FS::BYTE_OFFSET) {
        size_t tmp_extent, tmp_extoff;
        size_t rem = offset;
        INodes::seek(meta->handle(), &inode, rem, M3FS_SEEK_SET, tmp_extent, tmp_extoff);
        offset = tmp_extent;
        firstOff = rem;
    }

    KIF::CapRngDesc crd;
    bool extended = false;
    Errors::last = Errors::NONE;
    loclist_type *locs = INodes::get_locs(meta->handle(), &inode, offset, count,
        (flags & M3FS::EXTEND) ? meta->handle().extend() : 0, oflags & MemGate::RWX, crd, extended);
    if(!locs) {
        SLOG(FS, fmt((word_t)this, "#x") << ": Determining locations failed: "
            << Errors::to_string(Errors::last));
        return Errors::last;
    }

    // start/continue transaction
    if(extended && xstate != TransactionState::ABORTED)
        xstate = TransactionState::OPEN;

    data.caps = crd.value();
    data.args.count = 2 + locs->count();
    data.args.vals[0] = extended;
    data.args.vals[1] = firstOff;
    for(size_t i = 0; i < locs->count(); ++i)
        data.args.vals[2 + i] = locs->get_len(i);

    if(ServiceLog::level & ServiceLog::FS) {
        SLOG(FS, "Received " << locs->count() << " capabilities:");
        for(size_t i = 0; i < locs->count(); ++i)
            SLOG(FS, "  " << fmt(locs->get_len(i), "#x"));
    }

    capscon.add(crd);
    return Errors::NONE;
}

void M3FSFileSession::read_write(GateIStream &is, bool write) {
    size_t submit;
    is >> submit;

    SLOG(FS, fmt((word_t)this, "#x")
        << ": file::" << (write ? "write" : "read") << "(submit=" << submit << "); "
        << "file[path=" << filename << ", extent=" << extent << ", extoff=" << extoff << "]");

    if((write && !(oflags & FILE_W)) || (!write && !(oflags & FILE_R))) {
        reply_error(is, Errors::NO_PERM);
        return;
    }

    if(submit > 0) {
        if(extent == 0) {
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        if(lastoff + submit < extlen) {
            extent--;
            extoff = lastoff + submit;
        }
        Errors::Code res = write ? do_commit(extent, extoff) : Errors::NONE;
        reply_vmsg(is, res, inode.size);
        return;
    }

    // get next mem cap
    KIF::CapRngDesc crd;
    bool extended = false;
    Errors::last = Errors::NONE;
    loclist_type *locs = INodes::get_locs(meta->handle(), &inode, extent, 1,
        write ? meta->handle().extend() : 0, oflags & MemGate::RWX, crd, extended);
    if(!locs) {
        SLOG(FS, fmt((word_t)this, "#x") << ": Determining locations failed: "
            << Errors::to_string(Errors::last));
        reply_error(is, Errors::last);
        return;
    }

    lastoff = extoff;
    extlen = locs->get_len(0);
    if(extlen > 0) {
        // activate mem cap for client
        if(Syscalls::get().activate(epcap, crd.start(), 0) != Errors::NONE) {
            SLOG(FS, fmt((word_t)this, "#x") << ": activate failed: "
                << Errors::to_string(Errors::last));
            reply_error(is, Errors::last);
            return;
        }

        // move forward
        extent += 1;
        extoff = 0;
    }

    // start/continue transaction
    if(extended && xstate != TransactionState::ABORTED)
        xstate = TransactionState::OPEN;

    // reply first
    reply_vmsg(is, Errors::NONE, lastoff, extlen - lastoff);

    // revoke last mem cap and remember new one
    if(last != ObjCap::INVALID)
        VPE::self().revoke(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, last, 1));
    last = crd.start();
}

void M3FSFileSession::read(GateIStream &is) {
    read_write(is, false);
}

void M3FSFileSession::write(GateIStream &is) {
    read_write(is, true);
}

void M3FSFileSession::seek(GateIStream &is) {
    int whence;
    size_t off;
    is >> off >> whence;
    SLOG(FS, fmt((word_t)this, "#x") << ": file::seek(path="
        << filename << ", off=" << off << ", whence=" << whence << ")");

    if(whence == SEEK_CUR) {
        reply_error(is, Errors::INV_ARGS);
        return;
    }

    size_t pos = INodes::seek(meta->handle(), &inode, off, whence, extent, extoff);
    reply_vmsg(is, Errors::NONE, pos, off);
}

void M3FSFileSession::fstat(GateIStream &is) {
    SLOG(FS, fmt((word_t)this, "#x") << ": file::fstat(path=" << filename << ")");

    m3::FileInfo info;
    INodes::stat(meta->handle(), &inode, info);
    reply_vmsg(is, Errors::NONE, info);
}

Errors::Code M3FSFileSession::do_commit(size_t extent, size_t extoff) {
    if(extent != 0 || extoff != 0) {
        if(~oflags & FILE_W)
            return Errors::INV_ARGS;
        if(xstate == TransactionState::ABORTED)
            return Errors::COMMIT_FAILED;

        // have we increased the filesize?
        m3::INode *ninode = INodes::get(meta->handle(), inode.inode);
        if(inode.size > ninode->size) {
            // get the old offset within the last extent
            size_t orgoff = 0;
            if(ninode->extents > 0) {
                Extent *indir = nullptr;
                Extent *ch = INodes::get_extent(meta->handle(), ninode, ninode->extents - 1, &indir, false);
                assert(ch != nullptr);
                orgoff = ch->length * meta->handle().sb().blocksize;
                size_t mod;
                if(((mod = ninode->size % meta->handle().sb().blocksize)) > 0)
                    orgoff -= meta->handle().sb().blocksize - mod;
            }

            // then cut it to either the org size or the max. position we've written to,
            // whatever is bigger
            if(ninode->extents == 0 || extent > ninode->extents - 1 ||
               (extent == ninode->extents - 1 && extoff > orgoff)) {
                INodes::truncate(meta->handle(), &inode, extent, extoff);
            }
            else {
                INodes::truncate(meta->handle(), &inode, ninode->extents - 1, orgoff);
                inode.size = ninode->size;
            }
            memcpy(ninode, &inode, sizeof(*ninode));

            // update the inode in all open files
            // and let all future commits for this file fail
            // TODO
            // for(auto s = begin(); s != end(); ++s) {
            //     if(&*s == sess)
            //         continue;
            //     for(size_t i = 0; i < M3FSSession::MAX_FILES; ++i) {
            //         M3FSSession::OpenFile *f = s->get(static_cast<int>(i));
            //         if(f && f->ino == of->ino && f->xstate == TransactionState::OPEN) {
            //             memcpy(&f->inode, ninode, sizeof(*ninode));
            //             f->xstate = TransactionState::ABORTED;
            //             // TODO revoke access, if necessary
            //         }
            //     }
            // }
        }
    }

    xstate = TransactionState::NONE;

    return Errors::NONE;
}
