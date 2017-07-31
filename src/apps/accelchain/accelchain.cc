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
#include <base/util/Profile.h>
#include <base/PEDesc.h>

#include <m3/stream/Standard.h>
#include <m3/session/Pager.h>
#include <m3/vfs/VFS.h>

#include <accel/stream/Stream.h>

#include "accelchain.h"

using namespace m3;
using namespace accel;

static Errors::Code execute(RecvGate &rgate, ChainMember **chain, size_t num,
                            const char *ipath, const char *opath) {
    fd_t infd = VFS::open(ipath, FILE_R);
    if(infd == FileTable::INVALID)
        return Errors::last;
    fd_t outfd = VFS::open(opath, FILE_W | FILE_TRUNC | FILE_CREATE);
    if(outfd == FileTable::INVALID)
        return Errors::last;

    File *in = VPE::self().fds()->get(infd);
    File *out = VPE::self().fds()->get(outfd);
    size_t inpos = 0, outpos = 0;
    size_t inlen = 0, outlen = 0;
    size_t inoff, outoff;
    capsel_t inmem, outmem, lastin = ObjCap::INVALID;

    SendGate sgate = SendGate::create(&chain[0]->rgate);

    Errors::Code err = Errors::NONE;
    while(1) {
        // input depleted?
        if(inpos == inlen) {
            // request next memory cap for input
            if((err = in->read_next(&inmem, &inoff, &inlen)) != Errors::NONE)
                goto error;

            if(inlen == 0)
                break;

            inpos = 0;
            if(inmem != lastin) {
                MemGate::bind(inmem).activate_for(chain[0]->vpe, StreamAccel::EP_INPUT);
                lastin = inmem;
            }
        }

        // output depleted?
        if(outpos == outlen) {
            // request next memory cap for output
            if((err = out->begin_write(&outmem, &outoff, &outlen)) != Errors::NONE)
                goto error;

            outpos = 0;
        }

        // activate output mem with new offset
        MemGate::bind(outmem).activate_for(chain[num - 1]->vpe, StreamAccel::EP_OUTPUT, outoff + outpos);

        // use the minimum of both, because input and output have to be of the same size atm
        size_t amount = std::min(inlen - inpos, outlen - outpos);
        amount = requestResponse(sgate, rgate, inoff + inpos, amount);

        inpos += amount;
        outpos += amount;
        out->commit_write(amount);
    }

error:
    VFS::close(outfd);
    VFS::close(infd);
    return err;
}

static Errors::Code execute_indirect(RecvGate &rgate, ChainMember **chain, size_t num,
                                     const char *ipath, const char *opath, size_t bufsize) {
    uint8_t *buffer = new uint8_t[bufsize];

    fd_t infd = VFS::open(ipath, FILE_R);
    if(infd == FileTable::INVALID)
        return Errors::last;
    fd_t outfd = VFS::open(opath, FILE_W | FILE_TRUNC | FILE_CREATE);
    if(outfd == FileTable::INVALID)
        return Errors::last;

    Errors::Code err = Errors::NONE;

    File *in = VPE::self().fds()->get(infd);
    File *out = VPE::self().fds()->get(outfd);

    SendGate **sgates = new SendGate*[num];
    for(size_t i = 0; i < num; ++i)
        sgates[i] = new SendGate(SendGate::create(&chain[i]->rgate));

    MemGate buf1 = chain[0]->vpe.mem().derive(StreamAccel::BUF_ADDR, StreamAccel::BUF_MAX_SIZE);
    MemGate bufn = chain[num - 1]->vpe.mem().derive(StreamAccel::BUF_ADDR, StreamAccel::BUF_MAX_SIZE);

    size_t total = 0, seen = 0;
    ssize_t count = in->read(buffer, bufsize);
    if(count < 0) {
        err = Errors::last;
        goto error;
    }
    buf1.write(buffer, static_cast<size_t>(count), 0);
    sendRequest(*sgates[0], 0, static_cast<size_t>(count));
    total += static_cast<size_t>(count);

    count = in->read(buffer, bufsize);

    while(seen < total) {
        GateIStream is = receive_msg(rgate);
        label_t label = is.label<label_t>();

        // cout << "got msg from " << label << "\n";

        if(label == num - 1) {
            auto *upd = reinterpret_cast<const StreamAccel::UpdateCommand*>(is.message().data);
            bufn.read(buffer, upd->len, 0);
            // cout << "write " << upd->len << " bytes\n";
            out->write(buffer, upd->len);
            seen += upd->len;
        }

        if(label == 0) {
            if(num > 1)
                send_msg(*sgates[1], is.message().data, is.message().length);

            total += static_cast<size_t>(count);
            if(count > 0) {
                buf1.write(buffer, static_cast<size_t>(count), 0);
                sendRequest(*sgates[0], 0, static_cast<size_t>(count));

                count = in->read(buffer, bufsize);
                // cout << "read " << count << " bytes\n";
                if(count < 0) {
                    err = Errors::last;
                    goto error;
                }
            }
        }
        else if(label != num - 1)
            send_msg(*sgates[label + 1], is.message().data, is.message().length);

        // cout << seen << " / " << total << "\n";
    }

error:
    for(size_t i = 0; i < num; ++i)
        delete sgates[i];
    delete[] sgates;
    delete[] buffer;
    VFS::close(outfd);
    VFS::close(infd);
    return err;
}

static void execchain(const char *in, const char *out, size_t num, size_t bufsize,
                      cycles_t comptime, bool direct) {
    RecvGate rgate = RecvGate::create(nextlog2<8 * 64>::val, nextlog2<64>::val);
    rgate.activate();

    ChainMember *chain[num];
    StreamAccel *accel[num];

    accel[num - 1] = StreamAccel::create(PEISA::ACCEL_FFT);
    chain[num - 1] = new ChainMember(accel[num - 1]->vpe(), accel[num - 1]->getRBAddr(),
        StreamAccel::RB_SIZE, rgate, num - 1);

    for(ssize_t i = static_cast<ssize_t>(num) - 2; i >= 0; --i) {
        accel[i] = StreamAccel::create(PEISA::ACCEL_FFT);
        chain[i] = new ChainMember(accel[i]->vpe(), accel[i]->getRBAddr(),
            StreamAccel::RB_SIZE, direct ? chain[i + 1]->rgate : rgate, static_cast<label_t>(i));
    }

    for(auto *m : chain) {
        m->send_caps();
        m->activate_recv();
    }

    for(auto *m : chain) {
        m->vpe.start();
        m->activate_send();
    }

    for(size_t i = 0; i < num - 1; ++i) {
        MemGate *buf = new MemGate(
            chain[i + 1]->vpe.mem().derive(StreamAccel::BUF_ADDR, bufsize));
        buf->activate_for(chain[i]->vpe, StreamAccel::EP_OUTPUT);

        chain[i]->init(bufsize, bufsize, direct ? bufsize / 2 : bufsize, comptime);
    }

    Errors::Code res;
    if(direct) {
        chain[num - 1]->init(bufsize, static_cast<size_t>(-1), static_cast<size_t>(-1), comptime);
        res = execute(rgate, chain, num, in, out);
    }
    else {
        chain[num - 1]->init(bufsize, bufsize, bufsize, comptime);
        res = execute_indirect(rgate, chain, num, in, out, bufsize);
    }
    if(res != Errors::NONE)
        errmsg("Operation failed: " << Errors::to_string(res));

    for(auto *a : accel)
        delete a;
}

int main(int argc, char **argv) {
    if(argc < 7)
        exitmsg("Usage: " << argv[0] << " <in> <out> <direct> <bufsize> <comptime> <num>");

    if(VFS::mount("/", new M3FS("m3fs")) != Errors::NONE) {
        if(Errors::last != Errors::EXISTS)
            exitmsg("Unable to mount filesystem\n");
    }

    const char *in = argv[1];
    const char *out = argv[2];
    bool direct = IStringStream::read_from<int>(argv[3]);
    size_t bufsize = IStringStream::read_from<size_t>(argv[4]);
    cycles_t comptime = IStringStream::read_from<cycles_t>(argv[5]);
    size_t num = IStringStream::read_from<size_t>(argv[6]);

    cycles_t start = Profile::start(0);
    execchain(in, out, num, bufsize, comptime, direct);
    cycles_t end = Profile::stop(0);

    cout << "Total time: " << (end - start) << " cycles\n";
    return 0;
}
