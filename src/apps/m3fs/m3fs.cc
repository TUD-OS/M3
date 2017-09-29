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
#include <base/CmdArgs.h>
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

enum class TransactionState {
    NONE,
    OPEN,
    ABORTED
};

class M3FSSessionData : public RequestSessionData {
public:
    static constexpr size_t MAX_FILES   = 16;

    // TODO reference counting
    struct OpenFile {
        explicit OpenFile() : ino(), flags(), xstate(TransactionState::NONE), inode(), caps() {
        }
        explicit OpenFile(inodeno_t _ino, int _flags, const INode &_inode)
            : ino(_ino), flags(_flags), xstate(TransactionState::NONE), inode(_inode), caps() {
        }

        inodeno_t ino;
        int flags;
        TransactionState xstate;
        INode inode;
        LimitedCapContainer caps;
    };

    explicit M3FSSessionData() : RequestSessionData(), _files() {
    }
    virtual ~M3FSSessionData() {
        for(size_t i = 0; i < MAX_FILES; ++i)
            release_fd(static_cast<int>(i));
    }

    OpenFile *get(int fd) {
        if(fd >= 0 && fd < static_cast<int>(MAX_FILES))
            return _files[fd];
        return nullptr;
    }
    int request_fd(inodeno_t ino, int flags, const INode &inode) {
        assert(flags != 0);
        for(size_t i = 0; i < MAX_FILES; ++i) {
            if(_files[i] == NULL) {
                _files[i] = new OpenFile(ino, flags, inode);
                return static_cast<int>(i);
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
    explicit M3FSRequestHandler(size_t fssize, size_t extend, bool clear)
            : m3fs_reqh_base_t(),
              _mem(MemGate::create_global_for(FS_IMG_OFFSET,
                Math::round_up(fssize, (size_t)1 << MemGate::PERM_BITS), MemGate::RWX)),
              _extend(extend),
              _handle(_mem.sel(), clear) {
        add_operation(M3FS::OPEN, &M3FSRequestHandler::open);
        add_operation(M3FS::STAT, &M3FSRequestHandler::stat);
        add_operation(M3FS::FSTAT, &M3FSRequestHandler::fstat);
        add_operation(M3FS::SEEK, &M3FSRequestHandler::seek);
        add_operation(M3FS::MKDIR, &M3FSRequestHandler::mkdir);
        add_operation(M3FS::RMDIR, &M3FSRequestHandler::rmdir);
        add_operation(M3FS::LINK, &M3FSRequestHandler::link);
        add_operation(M3FS::UNLINK, &M3FSRequestHandler::unlink);
        add_operation(M3FS::COMMIT, &M3FSRequestHandler::commit);
        add_operation(M3FS::CLOSE, &M3FSRequestHandler::close);
    }

    virtual Errors::Code handle_obtain(M3FSSessionData *sess, KIF::Service::ExchangeData &data) override {
        if(!sess->send_gate())
            return m3fs_reqh_base_t::handle_obtain(sess, data);

        EVENT_TRACER_FS_getlocs();
        if(data.argcount != 5)
            return Errors::INV_ARGS;

        int fd = data.args[0];
        size_t offset = data.args[1];
        size_t count = data.args[2];
        bool extend = data.args[3];
        int flags = data.args[4];

        SLOG(FS, fmt((word_t)sess, "#x") << ": fs::get_locs(fd=" << fd << ", offset=" << offset
            << ", count=" << count << ", extend=" << extend << ", flags=" << flags << ")");

        M3FSSessionData::OpenFile *of = sess->get(fd);
        if(!of || count == 0) {
            SLOG(FS, fmt((word_t)sess, "#x") << ": Invalid request (of=" << of << ")");
            return Errors::INV_ARGS;
        }

        // acquire space for the new caps
        of->caps.request(count);

        // don't try to extend the file, if we're not writing
        if(~of->flags & FILE_W)
            extend = false;

        // determine extent from byte offset
        size_t firstOff = 0;
        if(flags & M3FS::BYTE_OFFSET) {
            size_t extent, extoff;
            size_t rem = offset;
            INodes::seek(_handle, &of->inode, rem, M3FS_SEEK_SET, extent, extoff);
            offset = extent;
            firstOff = rem;
        }

        KIF::CapRngDesc crd;
        bool extended = false;
        Errors::last = Errors::NONE;
        m3::loclist_type *locs = INodes::get_locs(_handle, &of->inode, offset, count,
            extend ? _extend : 0, of->flags & MemGate::RWX, crd, extended);
        if(!locs) {
            SLOG(FS, fmt((word_t)sess, "#x") << ": Determining locations failed: "
                << Errors::to_string(Errors::last));
            return Errors::last;
        }

        // start/continue transaction
        if(extended && of->xstate != TransactionState::ABORTED)
            of->xstate = TransactionState::OPEN;

        data.caps = crd.value();
        data.argcount = 2 + locs->count();
        data.args[0] = extended;
        data.args[1] = firstOff;
        for(size_t i = 0; i < locs->count(); ++i)
            data.args[2 + i] = locs->get(i);

        of->caps.add(crd);
        return Errors::NONE;
    }

    void open(GateIStream &is) {
        EVENT_TRACER_FS_open();
        M3FSSessionData *sess = is.label<M3FSSessionData*>();
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
        if(flags & FILE_TRUNC) {
            INodes::truncate(_handle, inode, 0, 0);
            // TODO revoke access, if necessary
        }

        // for directories: ensure that we don't have a changed version in the cache
        if(M3FS_ISDIR(inode->mode))
            INodes::write_back(_handle, inode);

        fd = sess->request_fd(inode->inode, flags, *inode);
        SLOG(FS, fmt((word_t)sess, "#x") << ": -> fd=" << fd << ", inode=" << inode->inode);
        reply_vmsg(is, Errors::NONE, fd);
    }

    void seek(GateIStream &is) {
        EVENT_TRACER_FS_seek();
        M3FSSessionData *sess = is.label<M3FSSessionData*>();
        int fd, whence;
        size_t off;
        size_t extent, extoff;
        is >> fd >> off >> whence >> extent >> extoff;
        SLOG(FS, fmt((word_t)sess, "#x") << ": fs::seek(fd=" << fd
            << ", off=" << off << ", whence=" << whence << ")");

        M3FSSessionData::OpenFile *of = sess->get(fd);
        if(!of) {
            SLOG(FS, fmt((word_t)sess, "#x") << ": seek failed: "
                << Errors::to_string(Errors::INV_ARGS));
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        size_t pos = INodes::seek(_handle, &of->inode, off, whence, extent, extoff);
        reply_vmsg(is, Errors::NONE, extent, extoff, pos + off);
    }

    void stat(GateIStream &is) {
        EVENT_TRACER_FS_stat();
        M3FSSessionData *sess = is.label<M3FSSessionData*>();
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

        m3::INode *inode = INodes::get(_handle, ino);
        assert(inode != nullptr);

        m3::FileInfo info;
        INodes::stat(_handle, inode, info);
        reply_vmsg(is, Errors::NONE, info);
    }

    void fstat(GateIStream &is) {
        EVENT_TRACER_FS_fstat();
        M3FSSessionData *sess = is.label<M3FSSessionData*>();
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
        INodes::stat(_handle, &of->inode, info);
        reply_vmsg(is, Errors::NONE, info);
    }

    void mkdir(GateIStream &is) {
        EVENT_TRACER_FS_mkdir();
        M3FSSessionData *sess = is.label<M3FSSessionData*>();
        String path;
        mode_t mode;
        is >> path >> mode;
        SLOG(FS, fmt((word_t)sess, "#x") << ": fs::mkdir(path=" << path
            << ", mode=" << fmt(mode, "o") << ")");

        Errors::Code res = Dirs::create(_handle, path.c_str(), mode);
        if(res != Errors::NONE)
            SLOG(FS, fmt((word_t)sess, "#x") << ": mkdir failed: " << Errors::to_string(res));
        reply_error(is, res);
    }

    void rmdir(GateIStream &is) {
        EVENT_TRACER_FS_rmdir();
        M3FSSessionData *sess = is.label<M3FSSessionData*>();
        String path;
        is >> path;
        SLOG(FS, fmt((word_t)sess, "#x") << ": fs::rmdir(path=" << path << ")");

        Errors::Code res = Dirs::remove(_handle, path.c_str());
        if(res != Errors::NONE)
            SLOG(FS, fmt((word_t)sess, "#x") << ": rmdir failed: " << Errors::to_string(res));
        reply_error(is, res);
    }

    void link(GateIStream &is) {
        EVENT_TRACER_FS_link();
        M3FSSessionData *sess = is.label<M3FSSessionData*>();
        String oldpath, newpath;
        is >> oldpath >> newpath;
        SLOG(FS, fmt((word_t)sess, "#x") << ": fs::link(oldpath=" << oldpath
            << ", newpath=" << newpath << ")");

        Errors::Code res = Dirs::link(_handle, oldpath.c_str(), newpath.c_str());
        if(res != Errors::NONE)
            SLOG(FS, fmt((word_t)sess, "#x") << ": link failed: " << Errors::to_string(res));
        reply_error(is, res);
    }

    void unlink(GateIStream &is) {
        EVENT_TRACER_FS_unlink();
        M3FSSessionData *sess = is.label<M3FSSessionData*>();
        String path;
        is >> path;
        SLOG(FS, fmt((word_t)sess, "#x") << ": fs::unlink(path=" << path << ")");

        Errors::Code res = Dirs::unlink(_handle, path.c_str(), false);
        if(res != Errors::NONE)
            SLOG(FS, fmt((word_t)sess, "#x") << ": unlink failed: " << Errors::to_string(res));
        reply_error(is, res);
    }

    void commit(GateIStream &is) {
        EVENT_TRACER_FS_commit();
        M3FSSessionData *sess = is.label<M3FSSessionData*>();
        int fd;
        size_t extent, extoff;
        is >> fd >> extent >> extoff;
        SLOG(FS, fmt((word_t)sess, "#x") << ": fs::commit(fd=" << fd
            << ", extent=" << extent << ", extoff=" << extoff << ")");

        Errors::Code res = do_commit(sess, fd, extent, extoff);

        reply_error(is, res);
    }

    void close(GateIStream &is) {
        EVENT_TRACER_FS_close();
        M3FSSessionData *sess = is.label<M3FSSessionData*>();
        int fd;
        size_t extent, extoff;
        is >> fd >> extent >> extoff;
        SLOG(FS, fmt((word_t)sess, "#x") << ": fs::close(fd=" << fd
            << ", extent=" << extent << ", extoff=" << extoff << ")");

        Errors::Code res = do_commit(sess, fd, extent, extoff);

        sess->release_fd(fd);

        reply_error(is, res);
    }

    virtual void handle_shutdown() override {
        m3fs_reqh_base_t::handle_shutdown();
        _handle.flush_cache();
    }

private:
    Errors::Code do_commit(M3FSSessionData *sess, int fd, size_t extent, size_t extoff) {
        M3FSSessionData::OpenFile *of = sess->get(fd);
        if(extent != 0 || extoff != 0) {
            if(!of || (~of->flags & FILE_W))
                return Errors::INV_ARGS;
            if(of->xstate == TransactionState::ABORTED)
                return Errors::COMMIT_FAILED;

            // have we increased the filesize?
            m3::INode *inode = INodes::get(_handle, of->ino);
            if(of->inode.size > inode->size) {
                // get the old offset within the last extent
                size_t orgoff = 0;
                if(inode->extents > 0) {
                    Extent *indir = nullptr;
                    Extent *ch = INodes::get_extent(_handle, inode, inode->extents - 1, &indir, false);
                    assert(ch != nullptr);
                    orgoff = ch->length * _handle.sb().blocksize;
                    size_t mod;
                    if(((mod = inode->size % _handle.sb().blocksize)) > 0)
                        orgoff -= _handle.sb().blocksize - mod;
                }

                // then cut it to either the org size or the max. position we've written to,
                // whatever is bigger
                if(inode->extents == 0 || extent > inode->extents - 1 ||
                   (extent == inode->extents - 1 && extoff > orgoff)) {
                    INodes::truncate(_handle, &of->inode, extent, extoff);
                }
                else {
                    INodes::truncate(_handle, &of->inode, inode->extents - 1, orgoff);
                    of->inode.size = inode->size;
                }
                memcpy(inode, &of->inode, sizeof(*inode));

                // update the inode in all open files
                // and let all future commits for this file fail
                for(auto s = begin(); s != end(); ++s) {
                    if(&*s == sess)
                        continue;
                    for(size_t i = 0; i < M3FSSessionData::MAX_FILES; ++i) {
                        M3FSSessionData::OpenFile *f = s->get(i);
                        if(f && f->ino == of->ino && f->xstate == TransactionState::OPEN) {
                            memcpy(&f->inode, inode, sizeof(*inode));
                            f->xstate = TransactionState::ABORTED;
                            // TODO revoke access, if necessary
                        }
                    }
                }
            }
        }

        of->xstate = TransactionState::NONE;

        return Errors::NONE;
    }

    MemGate _mem;
    size_t _extend;
    FSHandle _handle;
};

static void usage(const char *name) {
    Serial::get() << "Usage: " << name << " [-n <name>] [-e <blocks>] [-c] <size>\n";
    Serial::get() << "  -n: the name of the service (m3fs by default)\n";
    Serial::get() << "  -e: the number of blocks to extend files when appending\n";
    Serial::get() << "  -c: clear allocated blocks\n";
    exit(1);
}

int main(int argc, char *argv[]) {
    const char *name = "m3fs";
    size_t extend = 128;
    bool clear = false;

    int opt;
    while((opt = CmdArgs::get(argc, argv, "n:e:c")) != -1) {
        switch(opt) {
            case 'n': name = CmdArgs::arg; break;
            case 'e': extend = IStringStream::read_from<size_t>(CmdArgs::arg); break;
            case 'c': clear = true; break;
            default:
                usage(argv[0]);
        }
    }
    if(CmdArgs::ind >= argc)
        usage(argv[0]);

    size_t size = IStringStream::read_from<size_t>(argv[CmdArgs::ind]);
    Server<M3FSRequestHandler> srv(name, new M3FSRequestHandler(size, extend, clear));

    env()->workloop()->multithreaded(4);
    env()->workloop()->run();
    return 0;
}
