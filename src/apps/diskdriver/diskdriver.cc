/*
 * Copyright (C) 2018, Sebastian Reimers <sebastian.reimers@mailbox.tu-dresden.de>
 * Copyright (C) 2017, Lukas Landgraf <llandgraf317@gmail.com>
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

/**
 * Modifications in 2017 by Lukas Landgraf, llandgraf317@gmail.com
 * This file is copied and modified from Escape OS.
 */

#include <base/col/Treap.h>
#include <base/util/Math.h>
#include <base/CmdArgs.h>

#include <m3/com/MemGate.h>
#include <m3/server/RequestHandler.h>
#include <m3/server/Server.h>
#include <m3/session/Disk.h>
#include <m3/stream/Standard.h>

#include "session.h"
#include "disk.h"

using namespace m3;

struct CapNode : public TreapNode<CapNode, blockno_t> {
    friend class TreapNode;

    explicit CapNode(blockno_t bno, capsel_t mem, size_t len)
        : TreapNode(bno),
          _len(len),
          _mem(mem) {
    }
    size_t _len;
    capsel_t _mem;

    bool matches(blockno_t bno) {
        return (key() <= bno) && (bno < key() + _len);
    }
};

class DiskRequestHandler;
using base_class = RequestHandler<
    DiskRequestHandler, Disk::Operation, Disk::COUNT, DiskSrvSession
>;
static Server<DiskRequestHandler> *srv;

// we can only read 255 sectors (<31 blocks) at once (see ata.cc ata_setupCommand)
// and the max DMA size is 0x10000 in gem5
static constexpr size_t MAX_DMA_SIZE = Math::min(255 * 512, 0x10000);

class DiskRequestHandler : public base_class {
public:
    explicit DiskRequestHandler()
        : base_class(),
          _rgate(RecvGate::create(nextlog2<32 * Disk::MSG_SIZE>::val,
                                  nextlog2<Disk::MSG_SIZE>::val)) {
        add_operation(Disk::READ, &DiskRequestHandler::read);
        add_operation(Disk::WRITE, &DiskRequestHandler::write);

        using std::placeholders::_1;
        _rgate.start(std::bind(&DiskRequestHandler::handle_message, this, _1));
    }

    virtual Errors::Code obtain(DiskSrvSession *sess, KIF::Service::ExchangeData &data) override {
        if(data.args.count != 0 || data.caps != 1)
            return Errors::INV_ARGS;

        return sess->get_sgate(data);
    }

    virtual Errors::Code open(DiskSrvSession **sess, capsel_t srv_sel, word_t dev) override {
        if(!disk_exists(dev))
            return Errors::INV_ARGS;

        *sess = new DiskSrvSession(dev, srv_sel, &_rgate);
        PRINT(*sess, "new session for partition " << dev);
        return Errors::NONE;
    }

    virtual Errors::Code delegate(DiskSrvSession *sess, KIF::Service::ExchangeData &data) override {
        if(data.args.count != 2 || data.caps != 1)
            return Errors::NOT_SUP;

        capsel_t sel = VPE::self().alloc_sel();
        data.caps    = KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sel, data.caps).value();

        PRINT(sess, "received caps: bno=" << data.args.vals[0] << ", len=" << data.args.vals[1]);

        CapNode *node = caps.find(data.args.vals[0]);
        if(node) {
            caps.remove(node);
            delete node;
        }
        caps.insert(new CapNode(data.args.vals[0], sel, data.args.vals[1]));
        return Errors::NONE;
    }

    virtual Errors::Code close(DiskSrvSession *sess) override {
        // TODO remove CapNodes from the tree
        delete sess;
        return Errors::NONE;
    }

    virtual void shutdown() override {
        _rgate.stop();
    }

    void read(GateIStream &is) {
        blockno_t cap, start;
        size_t len, blocksize;
        goff_t off;
        is >> cap;
        is >> start;
        is >> len;
        is >> blocksize;
        is >> off;

        DiskSrvSession *sess = is.label<DiskSrvSession*>();

        PRINT(sess, "read blocks " << start << ":" << len
                                   << " @ " << fmt(off, "#x")
                                   << " in " << blocksize << "b blocks");

        if(len * blocksize < 512) {
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        Errors::Code res = Errors::NONE;

        capsel_t m_cap = caps.find(cap)->_mem;
        if(m_cap != ObjCap::INVALID) {
            MemGate m = MemGate::bind(m_cap);

            start *= blocksize;
            len *= blocksize;

            while(len >= MAX_DMA_SIZE) {
                disk_read(sess->device(), m, off, start, MAX_DMA_SIZE);
                start += MAX_DMA_SIZE;
                off += MAX_DMA_SIZE;
                len -= MAX_DMA_SIZE;
            }
            // now read the rest
            if(len)
                disk_read(sess->device(), m, off, start, len);
        }
        else
            res = Errors::NO_PERM;

        reply_error(is, res);
    }

    void write(GateIStream &is) {
        blockno_t cap, start;
        size_t len, blocksize;
        goff_t off;
        is >> cap;
        is >> start;
        is >> len;
        is >> blocksize;
        is >> off;

        DiskSrvSession *sess = is.label<DiskSrvSession*>();

        PRINT(sess, "write blocks " << start << ":" << len
                                    << " @ " << fmt(off, "#x")
                                    << " in " << blocksize << "b blocks");

        if(len * blocksize < 512) {
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        Errors::Code res = Errors::NONE;

        capsel_t m_cap = caps.find(cap)->_mem;
        if(m_cap != ObjCap::INVALID) {
            MemGate m = MemGate::bind(m_cap);

            start *= blocksize;
            len *= blocksize;

            while(len >= MAX_DMA_SIZE) {
                disk_write(sess->device(), m, off, start, MAX_DMA_SIZE);
                start += MAX_DMA_SIZE;
                off += MAX_DMA_SIZE;
                len -= MAX_DMA_SIZE;
            }
            // now write the rest
            if(len)
                disk_write(sess->device(), m, off, start, len);
        }
        else
            res = Errors::NO_PERM;

        reply_error(is, res);
    }

private:
    RecvGate _rgate;
    Treap<CapNode> caps;
};

static void usage(const char *name) {
    cerr << "Usage: " << name << " [-d] [-i] [-f <file>]\n";
    cerr << "  -d: enable DMA\n";
    cerr << "  -i: enable IRQs\n";
    cerr << "  -f: the disk file to use (host only)\n";
    exit(1);
}

int main(int argc, char **argv) {
    bool useDma = false;
    bool useIRQ = false;
    const char *disk = nullptr;

    int opt;
    while((opt = CmdArgs::get(argc, argv, "dif:")) != -1) {
        switch(opt) {
            case 'd': useDma = true; break;
            case 'i': useIRQ = true; break;
            case 'f': disk = CmdArgs::arg; break;
            default:
                usage(argv[0]);
        }
    }

    /* detect and init all devices */
    disk_init(useDma, useIRQ, disk);

    srv = new Server<DiskRequestHandler>("disk", new DiskRequestHandler());

    // env()->workloop()->multithreaded(8);
    env()->workloop()->run();

    delete srv;

    disk_deinit();
    return 0;
}
