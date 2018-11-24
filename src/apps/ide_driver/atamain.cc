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

#include <m3/com/MemGate.h>
#include <m3/server/RequestHandler.h>
#include <m3/server/Server.h>
#include <m3/session/Disk.h>
#include <m3/stream/Standard.h>

#include "Session.h"
#include "ata.h"
#include "controller.h"
#include "custom_types.h"
#include "device.h"
#include "partition.h"

using namespace m3;

static ulong handleRead(sATADevice *device, sPartition *part, MemGate &mem, size_t memoff,
                        uint offset, uint count);
static ulong handleWrite(sATADevice *device, sPartition *part, MemGate &mem, size_t memoff,
                         uint offset, uint count);

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

struct ATAPartitionDevice;

static size_t drvCount = 0;
static ATAPartitionDevice *devs[PARTITION_COUNT * DEVICE_COUNT];

struct ATAPartitionDevice {
    uint32_t id;
    uint32_t partition;

    explicit ATAPartitionDevice(uint32_t id, uint32_t partition)
        : id(id), partition(partition) {
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
        if(dev >= ARRAY_SIZE(devs) || devs[dev] == nullptr)
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
        ATAPartitionDevice *pdev = devs[sess->device()];
        sATADevice *dev = ctrl_getDevice(pdev->id);
        sPartition *part = dev->partTable + pdev->partition;

        PRINT(sess, "read blocks " << start << ":" << len
                                   << " @ " << fmt(off, "x")
                                   << " in " << blocksize << "b blocks");

        if(len * blocksize < 512) {
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        Errors::Code res = Errors::NONE;

        capsel_t m_cap = caps.find(cap)->_mem;
        if(m_cap != ObjCap::INVALID) {
            MemGate m = MemGate::bind(m_cap);
            ctrl_setupDMA(m);

            off *= blocksize;
            start *= blocksize;
            len *= blocksize;

            while(len >= MAX_DMA_SIZE) {
                handleRead(dev, part, m, off, start, MAX_DMA_SIZE);
                start += MAX_DMA_SIZE;
                off += MAX_DMA_SIZE;
                len -= MAX_DMA_SIZE;
            }
            // now read the rest
            if(len)
                handleRead(dev, part, m, off, start, len);
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
        ATAPartitionDevice *pdev = devs[sess->device()];
        sATADevice *dev = ctrl_getDevice(pdev->id);
        sPartition *part = dev->partTable + pdev->partition;

        PRINT(sess, "write blocks " << start << ":" << len
                                    << " @ " << fmt(off, "x")
                                    << " in " << blocksize << "b blocks");

        if(len * blocksize < 512) {
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        Errors::Code res = Errors::NONE;

        capsel_t m_cap = caps.find(cap)->_mem;
        if(m_cap != ObjCap::INVALID) {
            MemGate m = MemGate::bind(m_cap);
            ctrl_setupDMA(m);

            off *= blocksize;
            start *= blocksize;
            len *= blocksize;

            while(len >= MAX_DMA_SIZE) {
                handleWrite(dev, part, m, off, start, MAX_DMA_SIZE);
                start += MAX_DMA_SIZE;
                off += MAX_DMA_SIZE;
                len -= MAX_DMA_SIZE;
            }
            // now write the rest
            if(len)
                handleWrite(dev, part, m, off, start, len);
        }
        else
            res = Errors::NO_PERM;

        reply_error(is, res);
    }

private:
    RecvGate _rgate;
    Treap<CapNode> caps;
};

static const int RETRY_COUNT    = 3;

static void initDrives(void);

int main(int argc, char **argv) {
    bool useDma = true;
    bool useIRQ = true;

    for(size_t i = 2; (int)i < argc; i++) {
        if(strcmp(argv[i], "nodma") == 0)
            useDma = false;
        else if(strcmp(argv[i], "noirq") == 0)
            useIRQ = false;
    }

    /* detect and init all devices */
    ctrl_init(useDma, useIRQ);
    initDrives();

    srv = new Server<DiskRequestHandler>("disk", new DiskRequestHandler());

    // env()->workloop()->multithreaded(8);
    env()->workloop()->run();

    delete srv;

    ctrl_deinit();
    return 0;
}

static ulong handleRead(sATADevice *ataDev, sPartition *part, MemGate &mem, size_t memoff,
                        uint offset, uint count) {
    /* we have to check whether it is at least one sector. otherwise ATA can't
	 * handle the request */
    SLOG(IDE_ALL, "" << offset << " + " << count << " <= " << part->size << " * " << ataDev->secSize);
    if(offset + count <= part->size * ataDev->secSize && offset + count > offset) {
        uint rcount = m3::Math::round_up((size_t)count, ataDev->secSize);
        int i;
        SLOG(IDE_ALL, "Reading " << rcount << " bytes @ " << offset << " from device " << ataDev->id);
        for(i = 0; i < RETRY_COUNT; i++) {
            if(i > 0)
                SLOG(IDE, "Read failed; retry " << i);
            if(ataDev->rwHandler(ataDev, OP_READ, mem, memoff,
                                 offset / ataDev->secSize + part->start,
                                 ataDev->secSize, rcount / ataDev->secSize)) {
                return count;
            }
        }
        SLOG(IDE, "Giving up after " << i << " retries");
        return 0;
    }
    SLOG(IDE, "Invalid read-request: offset=" << offset << ", count=" << count
                                              << ", partSize=" << part->size * ataDev->secSize << " (device "
                                              << ataDev->id << ")");
    return 0;
}

static ulong handleWrite(sATADevice *ataDev, sPartition *part, MemGate &mem, size_t memoff,
                         uint offset, uint count) {
    SLOG(IDE_ALL, "ataDev->secSize: " << ataDev->secSize << ", count: " << count);
    if(offset + count <= part->size * ataDev->secSize && offset + count > offset) {
        int i;
        SLOG(IDE_ALL,
             "Writing " << count << " bytes @ 0x" << m3::fmt(offset, "x") << " to device " << ataDev->id);
        for(i = 0; i < RETRY_COUNT; i++) {
            if(i > 0)
                SLOG(IDE, "Write failed; retry " << i);
            if(ataDev->rwHandler(ataDev, OP_WRITE, mem, memoff,
                                 offset / ataDev->secSize + part->start,
                                 ataDev->secSize, count / ataDev->secSize)) {
                return count;
            }
        }
        SLOG(IDE, "Giving up after " << i << " retries");
        return 0;
    }
    SLOG(IDE, "Invalid write-request: offset=0x" << m3::fmt(offset, "x") << ", count=" << count
                                                 << ", partSize=" << part->size * ataDev->secSize
                                                 << " (device " << ataDev->id << ")");
    return 0;
}

static void initDrives(void) {
    uint deviceIds[] = {DEVICE_PRIM_MASTER, DEVICE_PRIM_SLAVE, DEVICE_SEC_MASTER, DEVICE_SEC_SLAVE};
    for(size_t i = 0; i < DEVICE_COUNT; i++) {
        sATADevice *ataDev = ctrl_getDevice(deviceIds[i]);
        if(ataDev->present == 0)
            continue;

        size_t size = (ataDev->info.userSectorCount * 512) / (1024 * 1024);
        SLOG(IDE, "Found disk device '" << device_model_name(ataDev) << "' (" << size << " MiB)");

        /* register device for every partition */
        for(size_t p = 0; p < PARTITION_COUNT; p++) {
            if(ataDev->partTable[p].present) {
                devs[drvCount] = new ATAPartitionDevice(ataDev->id, p);
                SLOG(IDE, "Registered partition " << drvCount
                                                  << " (device " << ataDev->id
                                                  << ", partition " << p + 1 << ")");

                drvCount++;
            }
        }
    }
}
