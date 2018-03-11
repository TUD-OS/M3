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

#include <limits>

#include "sess/FileSession.h"
#include "sess/MetaSession.h"
#include "FSHandle.h"
#include "INodes.h"
#include "Dirs.h"

using namespace m3;

class M3FSRequestHandler;

using base_class = RequestHandler<
    M3FSRequestHandler, M3FS::Operation, M3FS::COUNT, M3FSSession
>;

static Server<M3FSRequestHandler> *srv;

class M3FSRequestHandler : public base_class {
public:
    explicit M3FSRequestHandler(size_t fssize, size_t extend, bool clear)
        : base_class(),
          _rgate(RecvGate::create(nextlog2<32 * M3FSSession::MSG_SIZE>::val,
                                  nextlog2<M3FSSession::MSG_SIZE>::val)),
          _mem(MemGate::create_global_for(FS_IMG_OFFSET,
               Math::round_up(fssize, (size_t)1 << MemGate::PERM_BITS), MemGate::RWX)),
          _handle(_mem.sel(), extend, clear) {
        add_operation(M3FS::READ, &M3FSRequestHandler::read);
        add_operation(M3FS::WRITE, &M3FSRequestHandler::write);
        add_operation(M3FS::FSTAT, &M3FSRequestHandler::fstat);
        add_operation(M3FS::SEEK, &M3FSRequestHandler::seek);
        add_operation(M3FS::STAT, &M3FSRequestHandler::stat);
        add_operation(M3FS::MKDIR, &M3FSRequestHandler::mkdir);
        add_operation(M3FS::RMDIR, &M3FSRequestHandler::rmdir);
        add_operation(M3FS::LINK, &M3FSRequestHandler::link);
        add_operation(M3FS::UNLINK, &M3FSRequestHandler::unlink);

        using std::placeholders::_1;
        _rgate.start(std::bind(&M3FSRequestHandler::handle_message, this, _1));
    }

    virtual Errors::Code open(M3FSSession **sess, word_t) override {
        *sess = new M3FSMetaSession(_rgate, _handle);
        return Errors::NONE;
    }

    virtual Errors::Code obtain(M3FSSession *sess, KIF::Service::ExchangeData &data) override {
        if(sess->type() == M3FSSession::META) {
            auto meta = static_cast<M3FSMetaSession*>(sess);
            if(!meta->sgate())
                return meta->get_sgate(data);
            return meta->open_file(srv->sel(), data);
        }
        else {
            auto file = static_cast<M3FSFileSession*>(sess);
            if(data.args.count == 0)
                return file->clone(srv->sel(), data);
            return file->get_locs(data);
        }
    }

    virtual Errors::Code delegate(M3FSSession *sess, KIF::Service::ExchangeData &data) override {
        if(sess->type() == M3FSSession::META || data.args.count != 0 || data.caps != 1)
            return Errors::NOT_SUP;

        capsel_t sel = VPE::self().alloc_cap();
        static_cast<M3FSFileSession*>(sess)->set_ep(sel);
        data.caps = KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sel, data.caps).value();
        return Errors::NONE;
    }

    virtual Errors::Code close(M3FSSession *sess) override {
        sess->close();
        delete sess;
        return Errors::NONE;
    }

    virtual void shutdown() override {
        _rgate.stop();
        _handle.flush_cache();
    }

    void read(GateIStream &is) {
        M3FSSession *sess = is.label<M3FSSession*>();
        if(sess->type() != M3FSSession::FILE) {
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        static_cast<M3FSFileSession*>(sess)->read(is);
    }

    void write(GateIStream &is) {
        M3FSSession *sess = is.label<M3FSSession*>();
        if(sess->type() != M3FSSession::FILE) {
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        static_cast<M3FSFileSession*>(sess)->write(is);
    }

    void seek(GateIStream &is) {
        M3FSSession *sess = is.label<M3FSSession*>();
        if(sess->type() != M3FSSession::FILE) {
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        static_cast<M3FSFileSession*>(sess)->seek(is);
    }

    void fstat(GateIStream &is) {
        M3FSSession *sess = is.label<M3FSSession*>();
        if(sess->type() != M3FSSession::FILE) {
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        static_cast<M3FSFileSession*>(sess)->fstat(is);
    }

    void stat(GateIStream &is) {
        EVENT_TRACER_FS_stat();
        M3FSSession *sess = is.label<M3FSSession*>();
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

    void mkdir(GateIStream &is) {
        EVENT_TRACER_FS_mkdir();
        M3FSSession *sess = is.label<M3FSSession*>();
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
        M3FSSession *sess = is.label<M3FSSession*>();
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
        M3FSSession *sess = is.label<M3FSSession*>();
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
        M3FSSession *sess = is.label<M3FSSession*>();
        String path;
        is >> path;
        SLOG(FS, fmt((word_t)sess, "#x") << ": fs::unlink(path=" << path << ")");

        Errors::Code res = Dirs::unlink(_handle, path.c_str(), false);
        if(res != Errors::NONE)
            SLOG(FS, fmt((word_t)sess, "#x") << ": unlink failed: " << Errors::to_string(res));
        reply_error(is, res);
    }

private:
    RecvGate _rgate;
    MemGate _mem;
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
    srv = new Server<M3FSRequestHandler>(name, new M3FSRequestHandler(size, extend, clear));

    env()->workloop()->multithreaded(4);
    env()->workloop()->run();

    delete srv;
    return 0;
}
