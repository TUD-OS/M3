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

#include <base/Common.h>
#include <base/stream/IStringStream.h>
#include <base/log/Services.h>
#include <base/Errors.h>

#include <m3/session/M3FS.h>
#include <m3/server/RequestHandler.h>
#include <m3/server/Server.h>

#include "FSHandle.h"
#include "INodes.h"
#include "Dirs.h"

using namespace m3;

class M3FSRequestHandler;

struct LimitedCapContainer {
    static constexpr size_t MAX_CAPS    = 32;

    explicit LimitedCapContainer() : pos(), victim() {
        for(size_t j = 0; j < MAX_CAPS; ++j)
            caps[j] = ObjCap::INVALID;
    }
    ~LimitedCapContainer() {
        // request all to revoke all caps
        request(MAX_CAPS);
    }

    void add(const KIF::CapRngDesc &crd) {
        for(size_t i = 0; i < crd.count(); ++i) {
            assert(caps[pos] == ObjCap::INVALID);
            caps[pos] = crd.start() + i;
            pos = (pos + 1) % MAX_CAPS;
        }
    }

    void request(size_t count) {
        while(count > 0) {
            capsel_t first = caps[victim];
            caps[victim] = ObjCap::INVALID;
            victim = (victim + 1) % MAX_CAPS;
            count--;

            if(first != ObjCap::INVALID) {
                KIF::CapRngDesc caps = get_linear(first, count);
                VPE::self().free_caps(caps.start(), caps.count());
                VPE::self().revoke(caps);
            }
        }
    }

    KIF::CapRngDesc get_linear(capsel_t first, size_t &rem) {
        capsel_t last = first;
        while(rem > 0 && caps[victim] == last + 1) {
            caps[victim] = ObjCap::INVALID;
            victim = (victim + 1) % MAX_CAPS;
            rem--;
            last++;
        }
        return KIF::CapRngDesc(KIF::CapRngDesc::OBJ, first, 1 + last - first);
    }

    size_t pos;
    size_t victim;
    capsel_t caps[MAX_CAPS];
};

class M3FSSessionData : public RequestSessionData {
    static constexpr size_t MAX_FILES   = 16;

public:
    struct OpenCap : public SListItem {
        capsel_t sel;
    };

    // TODO reference counting
    struct OpenFile {
        explicit OpenFile() : ino(), flags(), orgsize(), orgextent(), orgoff(), caps() {
        }
        explicit OpenFile(ino_t _ino, int _flags, uint32_t _orgsize, size_t _orgextent, size_t _orgoff)
            : ino(_ino), flags(_flags), orgsize(_orgsize), orgextent(_orgextent), orgoff(_orgoff),
              caps() {
        }

        inodeno_t ino;
        int flags;
        uint32_t orgsize;
        size_t orgextent;
        size_t orgoff;
        LimitedCapContainer caps;
    };

    explicit M3FSSessionData() : RequestSessionData(), _files() {
    }
    virtual ~M3FSSessionData() {
        for(size_t i = 0; i < MAX_FILES; ++i)
            release_fd(i);
    }

    OpenFile *get(int fd) {
        if(fd >= 0 && fd < static_cast<int>(MAX_FILES))
            return _files[fd];
        return nullptr;
    }
    int request_fd(inodeno_t ino, int flags, off_t orgsize, size_t orgextent, size_t orgoff) {
        assert(flags != 0);
        for(size_t i = 0; i < MAX_FILES; ++i) {
            if(_files[i] == NULL) {
                _files[i] = new OpenFile(ino, flags, orgsize, orgextent, orgoff);
                return i;
            }
        }
        return -1;
    }
    void release_fd(int fd) {
        if(fd >= 0 && fd < static_cast<int>(MAX_FILES)) {
            delete _files[fd];
            _files[fd] = NULL;
        }
    }

private:
    OpenFile *_files[MAX_FILES];
};

using m3fs_reqh_base_t = RequestHandler<
    M3FSRequestHandler, M3FS::Operation, M3FS::COUNT, M3FSSessionData
>;

class M3FSRequestHandler : public m3fs_reqh_base_t {
public:
    explicit M3FSRequestHandler(size_t fssize)
            : m3fs_reqh_base_t(),
              _mem(MemGate::create_global_for(FS_IMG_OFFSET,
                Math::round_up(fssize, (size_t)1 << MemGate::PERM_BITS), MemGate::RWX)),
              _handle(_mem.sel()) {
        add_operation(M3FS::OPEN, &M3FSRequestHandler::open);
        add_operation(M3FS::STAT, &M3FSRequestHandler::stat);
        add_operation(M3FS::FSTAT, &M3FSRequestHandler::fstat);
        add_operation(M3FS::SEEK, &M3FSRequestHandler::seek);
        add_operation(M3FS::MKDIR, &M3FSRequestHandler::mkdir);
        add_operation(M3FS::RMDIR, &M3FSRequestHandler::rmdir);
        add_operation(M3FS::LINK, &M3FSRequestHandler::link);
        add_operation(M3FS::UNLINK, &M3FSRequestHandler::unlink);
        add_operation(M3FS::CLOSE, &M3FSRequestHandler::close);
    }

    virtual size_t credits() override {
        return Server<M3FSRequestHandler>::DEF_MSGSIZE;
    }

    virtual Errors::Code handle_obtain(M3FSSessionData *sess, RecvBuf *rcvbuf,
            KIF::Service::ExchangeData &data) override {
        if(!sess->send_gate())
            return m3fs_reqh_base_t::handle_obtain(sess, rcvbuf, data);

        EVENT_TRACER_FS_getlocs();
        if(data.argcount != 5)
            return Errors::INV_ARGS;

        int fd = data.args[0];
        size_t offset = data.args[1];
        size_t count = data.args[2];
        size_t blocks = data.args[3];
        int flags = data.args[4];

        SLOG(FS, fmt((word_t)sess, "#x") << ": fs::get_locs(fd=" << fd << ", offset=" << offset
            << ", count=" << count << ", blocks=" << blocks << ", flags=" << flags << ")");

        M3FSSessionData::OpenFile *of = sess->get(fd);
        if(!of || count == 0) {
            SLOG(FS, fmt((word_t)sess, "#x") << ": Invalid request (of=" << of << ")");
            return Errors::INV_ARGS;
        }
        m3::INode *inode = INodes::get(_handle, of->ino);

        // acquire space for the new caps
        of->caps.request(count);

        // don't try to extend the file, if we're not writing
        if(~of->flags & FILE_W)
            blocks = 0;

        // determine extent from byte offset
        off_t firstOff = 0;
        if(flags & M3FS::BYTE_OFFSET) {
            size_t extent, extoff;
            off_t rem = offset;
            INodes::seek(_handle, of->ino, rem, SEEK_SET, extent, extoff);
            offset = extent;
            firstOff = rem;
        }

        KIF::CapRngDesc crd;
        bool extended = false;
        Errors::last = Errors::NO_ERROR;
        m3::loclist_type *locs = INodes::get_locs(_handle, inode, offset, count, blocks,
            of->flags & MemGate::RWX, crd, extended);
        if(!locs) {
            SLOG(FS, fmt((word_t)sess, "#x") << ": Determining locations failed: "
                << Errors::to_string(Errors::last));
            return Errors::last;
        }

        data.caps = crd.value();
        data.argcount = 2 + locs->count();
        data.args[0] = extended;
        data.args[1] = firstOff;
        for(size_t i = 0; i < locs->count(); ++i)
            data.args[2 + i] = locs->get(i);

        of->caps.add(crd);
        return Errors::NO_ERROR;
    }

    void open(GateIStream &is) {
        EVENT_TRACER_FS_open();
        M3FSSessionData *sess = is.gate().session<M3FSSessionData>();
        String path;
        int fd, flags;
        is >> path >> flags;
        SLOG(FS, fmt((word_t)sess, "#x") << ": fs::open(path=" << path
            << ", flags=" << fmt(flags, "#x") << ")");

        m3::inodeno_t ino = Dirs::search(_handle, path.c_str(), flags & FILE_CREATE);
        if(ino == INVALID_INO) {
            SLOG(FS, fmt((word_t)sess, "#x") << ": open failed: "
                << Errors::to_string(Errors::last));
            reply_error(is, Errors::last);
            return;
        }
        m3::INode *inode = INodes::get(_handle, ino);
        if(((flags & FILE_W) && (~inode->mode & M3FS_IWUSR)) ||
            ((flags & FILE_R) && (~inode->mode & M3FS_IRUSR))) {
            SLOG(FS, fmt((word_t)sess, "#x") << ": open failed: "
                << Errors::to_string(Errors::NO_PERM));
            reply_error(is, Errors::NO_PERM);
            return;
        }

        // only determine the current size, if we're writing and the file isn't empty
        size_t extent = 0, off = 0;
        if(flags & FILE_TRUNC)
            INodes::truncate(_handle, inode, 0, 0);
        else if(inode->extents > 0 && (flags & FILE_W)) {
            Extent *indir = nullptr;
            Extent *ch = INodes::get_extent(_handle, inode, inode->extents - 1, &indir, false);
            assert(ch != nullptr);
            extent = inode->extents - 1;
            off = ch->length * _handle.sb().blocksize;
            size_t mod;
            if(((mod = inode->size % _handle.sb().blocksize)) > 0)
                off -= _handle.sb().blocksize - mod;
        }

        // for directories: ensure that we don't have a changed version in the cache
        if(M3FS_ISDIR(inode->mode))
            INodes::write_back(_handle, inode);

        fd = sess->request_fd(inode->inode, flags, inode->size, extent, off);
        SLOG(FS, fmt((word_t)sess, "#x") << ": -> fd=" << fd << ", inode=" << inode->inode
            << ", size=" << inode->size << ", extent=" << extent << ", extoff=" << off);
        reply_vmsg(is, Errors::NO_ERROR, fd);
    }

    void seek(GateIStream &is) {
        EVENT_TRACER_FS_seek();
        M3FSSessionData *sess = is.gate().session<M3FSSessionData>();
        int fd, whence;
        off_t off;
        size_t extent, extoff;
        is >> fd >> off >> whence >> extent >> extoff;
        SLOG(FS, fmt((word_t)sess, "#x") << ": fs::seek(fd=" << fd
            << ", off=" << off << ", whence=" << whence << ")");

        const M3FSSessionData::OpenFile *of = sess->get(fd);
        if(!of) {
            SLOG(FS, fmt((word_t)sess, "#x") << ": seek failed: "
                << Errors::to_string(Errors::INV_ARGS));
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        off_t pos = INodes::seek(_handle, of->ino, off, whence, extent, extoff);
        reply_vmsg(is, Errors::NO_ERROR, extent, extoff, pos + off);
    }

    void stat(GateIStream &is) {
        EVENT_TRACER_FS_stat();
        M3FSSessionData *sess = is.gate().session<M3FSSessionData>();
        String path;
        is >> path;
        SLOG(FS, fmt((word_t)sess, "#x") << ": fs::stat(path=" << path << ")");

        m3::inodeno_t ino = Dirs::search(_handle, path.c_str(), false);
        if(ino == INVALID_INO) {
            SLOG(FS, fmt((word_t)sess, "#x") << ": stat failed: "
                << Errors::to_string(Errors::last));
            reply_error(is, Errors::last);
            return;
        }

        m3::FileInfo info;
        INodes::stat(_handle, ino, info);
        reply_vmsg(is, Errors::NO_ERROR, info);
    }

    void fstat(GateIStream &is) {
        EVENT_TRACER_FS_fstat();
        M3FSSessionData *sess = is.gate().session<M3FSSessionData>();
        int fd;
        is >> fd;
        SLOG(FS, fmt((word_t)sess, "#x") << ": fs::fstat(fd=" << fd << ")");

        const M3FSSessionData::OpenFile *of = sess->get(fd);
        if(!of) {
            SLOG(FS, fmt((word_t)sess, "#x") << ": fstat failed: "
                << Errors::to_string(Errors::INV_ARGS));
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        m3::FileInfo info;
        INodes::stat(_handle, of->ino, info);
        reply_vmsg(is, Errors::NO_ERROR, info);
    }

    void mkdir(GateIStream &is) {
        EVENT_TRACER_FS_mkdir();
        M3FSSessionData *sess = is.gate().session<M3FSSessionData>();
        String path;
        mode_t mode;
        is >> path >> mode;
        SLOG(FS, fmt((word_t)sess, "#x") << ": fs::mkdir(path=" << path
            << ", mode=" << fmt(mode, "o") << ")");

        Errors::Code res = Dirs::create(_handle, path.c_str(), mode);
        if(res != Errors::NO_ERROR)
            SLOG(FS, fmt((word_t)sess, "#x") << ": mkdir failed: " << Errors::to_string(res));
        reply_error(is, res);
    }

    void rmdir(GateIStream &is) {
        EVENT_TRACER_FS_rmdir();
        M3FSSessionData *sess = is.gate().session<M3FSSessionData>();
        String path;
        is >> path;
        SLOG(FS, fmt((word_t)sess, "#x") << ": fs::rmdir(path=" << path << ")");

        Errors::Code res = Dirs::remove(_handle, path.c_str());
        if(res != Errors::NO_ERROR)
            SLOG(FS, fmt((word_t)sess, "#x") << ": rmdir failed: " << Errors::to_string(res));
        reply_error(is, res);
    }

    void link(GateIStream &is) {
        EVENT_TRACER_FS_link();
        M3FSSessionData *sess = is.gate().session<M3FSSessionData>();
        String oldpath, newpath;
        is >> oldpath >> newpath;
        SLOG(FS, fmt((word_t)sess, "#x") << ": fs::link(oldpath=" << oldpath
            << ", newpath=" << newpath << ")");

        Errors::Code res = Dirs::link(_handle, oldpath.c_str(), newpath.c_str());
        if(res != Errors::NO_ERROR)
            SLOG(FS, fmt((word_t)sess, "#x") << ": link failed: " << Errors::to_string(res));
        reply_error(is, res);
    }

    void unlink(GateIStream &is) {
        EVENT_TRACER_FS_unlink();
        M3FSSessionData *sess = is.gate().session<M3FSSessionData>();
        String path;
        is >> path;
        SLOG(FS, fmt((word_t)sess, "#x") << ": fs::unlink(path=" << path << ")");

        Errors::Code res = Dirs::unlink(_handle, path.c_str(), false);
        if(res != Errors::NO_ERROR)
            SLOG(FS, fmt((word_t)sess, "#x") << ": unlink failed: " << Errors::to_string(res));
        reply_error(is, res);
    }

    void close(GateIStream &is) {
        EVENT_TRACER_FS_close();
        M3FSSessionData *sess = is.gate().session<M3FSSessionData>();
        int fd;
        size_t extent, extoff;
        is >> fd >> extent >> extoff;
        SLOG(FS, fmt((word_t)sess, "#x") << ": fs::close(fd=" << fd
            << ", extent=" << extent << ", extoff=" << extoff << ")");

        if(extoff != 0) {
            const M3FSSessionData::OpenFile *of = sess->get(fd);
            if(!of || (~of->flags & FILE_W)) {
                reply_error(is, Errors::INV_ARGS);
                return;
            }

            // have we increased the filesize?
            m3::INode *inode = INodes::get(_handle, of->ino);
            if(inode->size > of->orgsize) {
                // then cut it to either the org size or the max. position we've written to,
                // whatever is bigger
                if(extent > of->orgextent || (extent == of->orgextent && extoff > of->orgoff))
                    INodes::truncate(_handle, inode, extent, extoff);
                else {
                    INodes::truncate(_handle, inode, of->orgextent, of->orgoff);
                    inode->size = of->orgsize;
                }
            }
        }

        sess->release_fd(fd);

        reply_error(is, Errors::NO_ERROR);
    }

    virtual void handle_shutdown() override {
        _handle.flush_cache();
    }

private:
    MemGate _mem;
    FSHandle _handle;
};

int main(int argc, char *argv[]) {
    if(argc < 2) {
        Serial::get() << "Usage: " << argv[0] << " <size>\n";
        return 1;
    }

    int size = IStringStream::read_from<int>(argv[1]);
    Server<M3FSRequestHandler> srv("m3fs", new M3FSRequestHandler(size));
    env()->workloop()->run();
    return 0;
}
