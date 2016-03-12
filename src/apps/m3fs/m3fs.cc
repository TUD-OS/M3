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

#include <m3/Common.h>
#include <m3/service/M3FS.h>
#include <m3/server/RequestHandler.h>
#include <m3/server/Server.h>
#include <m3/stream/IStringStream.h>
#include <m3/WorkLoop.h>
#include <m3/Log.h>
#include <m3/Errors.h>
#include <cstring>

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

    void add(const CapRngDesc &crd) {
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

            if(first != ObjCap::INVALID)
                get_linear(first, count).free_and_revoke();
        }
    }

    CapRngDesc get_linear(capsel_t first, size_t &rem) {
        capsel_t last = first;
        while(rem > 0 && caps[victim] == last + 1) {
            caps[victim] = ObjCap::INVALID;
            victim = (victim + 1) % MAX_CAPS;
            rem--;
            last++;
        }
        return CapRngDesc(CapRngDesc::OBJ, first, 1 + last - first);
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
    virtual void handle_open(GateIStream &args) override {
        reply_vmsg_on(args, Errors::NO_ERROR, add_session(new M3FSSessionData()));
    }

    virtual void handle_obtain(M3FSSessionData *sess, RecvBuf *rcvbuf, GateIStream &args, uint capcount) override {
        if(!sess->send_gate()) {
            m3fs_reqh_base_t::handle_obtain(sess, rcvbuf, args, capcount);
            return;
        }

        EVENT_TRACER_FS_getlocs();
        int fd, flags;
        size_t offset, count, blocks;
        args >> fd >> offset >> count >> blocks >> flags;
        LOG(FS, "fs::get_locs(fd=" << fd << ", offset=" << offset << ", count=" << count
            << ", blocks=" << blocks << ", flags=" << flags << ")");

        M3FSSessionData::OpenFile *of = sess->get(fd);
        if(!of || count == 0) {
            LOG(FS, "Invalid request (of=" << of << ")");
            reply_vmsg_on(args, Errors::INV_ARGS);
            return;
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

        CapRngDesc crd;
        bool extended = false;
        Errors::last = Errors::NO_ERROR;
        m3::loclist_type *locs = INodes::get_locs(_handle, inode, offset, count, blocks,
            of->flags & MemGate::RWX, crd, extended);
        if(!locs) {
            LOG(FS, "Determining locations failed: " << Errors::to_string(Errors::last));
            reply_vmsg_on(args, Errors::last);
            return;
        }

        reply_vmsg_on(args, Errors::NO_ERROR, crd, *locs, extended, firstOff);
        of->caps.add(crd);
    }

    void open(RecvGate &gate, GateIStream &is) {
        EVENT_TRACER_FS_open();
        M3FSSessionData *sess = gate.session<M3FSSessionData>();
        String path;
        int fd, flags;
        is >> path >> flags;
        LOG(FS, "fs::open(path=" << path << ", flags=" << fmt(flags, "#x") << ")");

        m3::inodeno_t ino = Dirs::search(_handle, path.c_str(), flags & FILE_CREATE);
        if(ino == INVALID_INO) {
            LOG(FS, "fs::open failed: " << Errors::to_string(Errors::last));
            reply_vmsg(gate, Errors::last);
            return;
        }
        m3::INode *inode = INodes::get(_handle, ino);
        if(((flags & FILE_W) && (~inode->mode & S_IWUSR)) ||
            ((flags & FILE_R) && (~inode->mode & S_IRUSR))) {
            LOG(FS, "fs::open failed: " << Errors::to_string(Errors::NO_PERM));
            reply_vmsg(gate, Errors::NO_PERM);
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
        if(S_ISDIR(inode->mode))
            INodes::write_back(_handle, inode);

        fd = sess->request_fd(inode->inode, flags, inode->size, extent, off);
        reply_vmsg(gate, Errors::NO_ERROR, fd);
    }

    void seek(RecvGate &gate, GateIStream &is) {
        EVENT_TRACER_FS_seek();
        M3FSSessionData *sess = gate.session<M3FSSessionData>();
        int fd, whence;
        off_t off;
        size_t extent, extoff;
        is >> fd >> off >> whence >> extent >> extoff;
        LOG(FS, "fs::seek(fd=" << fd << ", off=" << off << ", whence=" << whence << ")");

        const M3FSSessionData::OpenFile *of = sess->get(fd);
        if(!of) {
            LOG(FS, "fs::seek failed: " << Errors::to_string(Errors::INV_ARGS));
            reply_vmsg(gate, Errors::INV_ARGS);
            return;
        }

        off_t pos = INodes::seek(_handle, of->ino, off, whence, extent, extoff);
        reply_vmsg(gate, Errors::NO_ERROR, extent, extoff, pos + off);
    }

    void stat(RecvGate &gate, GateIStream &is) {
        EVENT_TRACER_FS_stat();
        String path;
        is >> path;
        LOG(FS, "fs::stat(path=" << path << ")");

        m3::inodeno_t ino = Dirs::search(_handle, path.c_str(), false);
        if(ino == INVALID_INO) {
            LOG(FS, "fs::stat failed: " << Errors::to_string(Errors::last));
            reply_vmsg(gate, Errors::last);
            return;
        }

        m3::FileInfo info;
        INodes::stat(_handle, ino, info);
        reply_vmsg(gate, Errors::NO_ERROR, info);
    }

    void fstat(RecvGate &gate, GateIStream &is) {
        EVENT_TRACER_FS_fstat();
        M3FSSessionData *sess = gate.session<M3FSSessionData>();
        int fd;
        is >> fd;
        LOG(FS, "fs::fstat(fd=" << fd << ")");

        const M3FSSessionData::OpenFile *of = sess->get(fd);
        if(!of) {
            LOG(FS, "fs::fstat failed: " << Errors::to_string(Errors::INV_ARGS));
            reply_vmsg(gate, Errors::INV_ARGS);
            return;
        }

        m3::FileInfo info;
        INodes::stat(_handle, of->ino, info);
        reply_vmsg(gate, Errors::NO_ERROR, info);
    }

    void mkdir(RecvGate &gate, GateIStream &is) {
        EVENT_TRACER_FS_mkdir();
        String path;
        mode_t mode;
        is >> path >> mode;
        LOG(FS, "fs::mkdir(path=" << path << ", mode=" << fmt(mode, "o") << ")");

        Errors::Code res = Dirs::create(_handle, path.c_str(), mode);
        if(res != Errors::NO_ERROR)
            LOG(FS, "fs::mkdir failed: " << Errors::to_string(res));
        reply_vmsg(gate, res);
    }

    void rmdir(RecvGate &gate, GateIStream &is) {
        EVENT_TRACER_FS_rmdir();
        String path;
        is >> path;
        LOG(FS, "fs::rmdir(path=" << path << ")");

        Errors::Code res = Dirs::remove(_handle, path.c_str());
        if(res != Errors::NO_ERROR)
            LOG(FS, "fs::rmdir failed: " << Errors::to_string(res));
        reply_vmsg(gate, res);
    }

    void link(RecvGate &gate, GateIStream &is) {
        EVENT_TRACER_FS_link();
        String oldpath, newpath;
        is >> oldpath >> newpath;
        LOG(FS, "fs::link(oldpath=" << oldpath << ", newpath=" << newpath << ")");

        Errors::Code res = Dirs::link(_handle, oldpath.c_str(), newpath.c_str());
        if(res != Errors::NO_ERROR)
            LOG(FS, "fs::link failed: " << Errors::to_string(res));
        reply_vmsg(gate, res);
    }

    void unlink(RecvGate &gate, GateIStream &is) {
        EVENT_TRACER_FS_unlink();
        String path;
        is >> path;
        LOG(FS, "fs::unlink(path=" << path << ")");

        Errors::Code res = Dirs::unlink(_handle, path.c_str(), false);
        if(res != Errors::NO_ERROR)
            LOG(FS, "fs::unlink failed: " << Errors::to_string(res));
        reply_vmsg(gate, res);
    }

    void close(RecvGate &gate, GateIStream &is) {
        EVENT_TRACER_FS_close();
        M3FSSessionData *sess = gate.session<M3FSSessionData>();
        int fd;
        size_t extent, extoff;
        is >> fd >> extent >> extoff;
        LOG(FS, "fs::close(fd=" << fd << ", extent=" << extent << ", extoff=" << extoff << ")");

        if(extoff != 0) {
            const M3FSSessionData::OpenFile *of = sess->get(fd);
            if(!of || (~of->flags & FILE_W)) {
                reply_vmsg(gate, Errors::INV_ARGS);
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

        reply_vmsg(gate, Errors::NO_ERROR);
    }

    virtual void handle_shutdown() override {
        LOG(FS, "fs::shutdown()");
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
    WorkLoop::get().run();
    return 0;
}
