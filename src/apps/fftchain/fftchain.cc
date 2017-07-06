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

using namespace m3;
using namespace accel;

static const int WARMUP    = 1;
static const int REPEATS   = 2;

static const size_t INPUT_BUF_SIZE      = 64 * 1024;
static const uintptr_t INPUT_BUF_SFFT   = 0x20000000;

alignas(64) static float kernel_org[] = {
    -1, -1, -1, -1
    -1,  8,  8, -1,
    -1,  8,  8, -1,
    -1, -1, -1, -1,
};
alignas(64) static float kernel_freq[ARRAY_SIZE(kernel_org)];

struct ChainMember {
    explicit ChainMember(VPE &_vpe, uintptr_t _rbuf, size_t rbSize, RecvGate &rgdst, label_t label)
        : vpe(_vpe), rbuf(_rbuf),
          rgate(RecvGate::create_for(vpe, getnextlog2(rbSize),
                                          getnextlog2(StreamAccel::MSG_SIZE))),
          sgate(SendGate::create(&rgdst, label)) {
    }

    void send_caps() {
        vpe.delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, rgate.sel(), 1), StreamAccel::RGATE_SEL);
        vpe.delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sgate.sel(), 1), StreamAccel::SGATE_SEL);
    }
    void activate_recv() {
        if(rbuf)
            rgate.activate(StreamAccel::EP_RECV, rbuf);
        else
            rgate.activate(StreamAccel::EP_RECV);
    }
    void activate_send() {
        sgate.activate_for(vpe, StreamAccel::EP_SEND);
    }

    void init(size_t bufsize, size_t outsize, size_t reportsize, cycles_t comptime) {
        StreamAccel::InitCommand init;
        init.cmd = static_cast<int64_t>(StreamAccel::Command::INIT);
        init.buf_size = bufsize;
        init.out_size = outsize;
        init.report_size = reportsize;
        init.comp_time = comptime;

        SendGate sgate = SendGate::create(&rgate);
        send_msg(sgate, &init, sizeof(init));
    }

    VPE &vpe;
    uintptr_t rbuf;
    RecvGate rgate;
    SendGate sgate;
};

static void sendRequest(SendGate &sgate, size_t off, uint64_t len) {
    StreamAccel::UpdateCommand req;
    req.cmd = static_cast<uint64_t>(StreamAccel::Command::UPDATE);
    req.off = off;
    req.len = len;
    req.eof = true;
    send_msg(sgate, &req, sizeof(req));
}

static uint64_t requestResponse(SendGate &sgate, RecvGate &rgate, size_t off, uint64_t len) {
    sendRequest(sgate, off, len);

    size_t done = 0;
    while(done < len) {
        StreamAccel::UpdateCommand req;
        GateIStream is = receive_msg(rgate);
        is >> req;
        cout << "Finished off=" << req.off << ", len=" << req.len << "\n";
        done += req.len;
    }
    return done;
}

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
        else if(label == 0) {
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
        else {
            send_msg(*sgates[label + 1], is.message().data, is.message().length);
        }

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

static void fftchain(const char *in, const char *out, size_t num, size_t bufsize, cycles_t comptime, bool direct) {
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

static int swfilter() {
    RecvGate rg = RecvGate::bind(StreamAccel::RGATE_SEL, getnextlog2(StreamAccel::RB_SIZE));
    SendGate sg = SendGate::bind(StreamAccel::SGATE_SEL);
    MemGate out = MemGate::bind(ObjCap::INVALID);

    // gates are already activated
    rg.ep(StreamAccel::EP_RECV);
    sg.ep(StreamAccel::EP_SEND);
    out.ep(StreamAccel::EP_OUTPUT);

    size_t outSize = 0, reportSize = 0;
    const size_t BUF_SIZE = 4096;
    float *buf = new float[BUF_SIZE / sizeof(float)];

    alignas(64) StreamAccel::UpdateCommand updcmd;
    updcmd.cmd = static_cast<uint64_t>(StreamAccel::Command::UPDATE);

    while(1) {
        GateIStream is = receive_msg(rg);
        uint64_t cmd;
        is >> cmd;

        if(static_cast<StreamAccel::Command>(cmd) == StreamAccel::Command::INIT) {
            auto *init = reinterpret_cast<const StreamAccel::InitCommand*>(is.message().data);
            outSize = init->out_size;
            reportSize = init->report_size;
            continue;
        }

        auto *upd = reinterpret_cast<const StreamAccel::UpdateCommand*>(is.message().data);
        // cout << "got off=" << fmt(upd->off, "#x") << " len=" << fmt(upd->len, "#x") << "\n";

        size_t agg = 0;
        size_t outOff = 0;
        size_t inOff = upd->off;
        size_t rem = upd->len;
        while(rem > 0) {
            size_t amount = Math::min(BUF_SIZE, rem);

            float *fl = reinterpret_cast<float*>(INPUT_BUF_SFFT + inOff);
            size_t num = amount / sizeof(float);
            for(size_t i = 0; i < num; ++i)
                buf[i] = fl[i] * kernel_freq[i % ARRAY_SIZE(kernel_freq)];

            out.write(buf, amount, outOff);

            agg += amount;
            inOff += amount;
            rem -= amount;

            if((rem == 0 && upd->eof) || agg >= reportSize) {
                updcmd.off = outOff + amount - agg;
                updcmd.len = agg;
                updcmd.eof = rem == 0 && upd->eof;
                send_msg(sg, &updcmd, sizeof(updcmd));
                agg = 0;
            }

            outOff = (outOff + amount) % outSize;
        }
    }

    return 0;
}

static void fftconvolution(const char *in, const char *out, size_t bufsize, bool direct) {
    RecvGate rgate = RecvGate::create(nextlog2<8 * 64>::val, nextlog2<64>::val);
    rgate.activate();

    {
        StreamAccel *a = StreamAccel::create(PEISA::ACCEL_FFT);
        ChainMember fft(a->vpe(), a->getRBAddr(), StreamAccel::RB_SIZE, rgate, 0);
        fft.send_caps();
        fft.activate_recv();
        fft.activate_send();

        MemGate in  = VPE::self().mem().derive(reinterpret_cast<size_t>(kernel_org), sizeof(kernel_org));
        MemGate out = VPE::self().mem().derive(reinterpret_cast<size_t>(kernel_freq), sizeof(kernel_freq));
        in.activate_for(fft.vpe, StreamAccel::EP_INPUT);
        out.activate_for(fft.vpe, StreamAccel::EP_OUTPUT);

        fft.vpe.start();

        fft.init(bufsize, sizeof(kernel_freq), sizeof(kernel_freq), 0);

        SendGate sgate = SendGate::create(&fft.rgate);
        requestResponse(sgate, rgate, 0, sizeof(kernel_freq));
        delete a;
    }

    enum {
        IFFT    = 2,
        MUL     = 1,
        SFFT    = 0,
    };

    StreamAccel *accel[2];
    ChainMember *chain[3];

    // IFFT
    accel[1] = StreamAccel::create(PEISA::ACCEL_FFT);
    chain[IFFT] = new ChainMember(accel[1]->vpe(), accel[1]->getRBAddr(), StreamAccel::RB_SIZE,
        rgate, IFFT);

    // multiplier
    VPE mul("mul");
    mul.fds(*VPE::self().fds());
    mul.obtain_fds();
    chain[MUL] = new ChainMember(mul, 0, StreamAccel::MSG_SIZE * 16,
        direct ? chain[IFFT]->rgate : rgate, MUL);

    // SFFT
    accel[0] = StreamAccel::create(PEISA::ACCEL_FFT);
    chain[SFFT] = new ChainMember(accel[0]->vpe(), accel[0]->getRBAddr(), StreamAccel::RB_SIZE,
        direct ? chain[MUL]->rgate : rgate, SFFT);

    for(auto *m : chain) {
        m->send_caps();
        m->activate_recv();
    }

    for(size_t i = 0; i < ARRAY_SIZE(chain); ++i) {
        if(i != MUL)
            chain[i]->vpe.start();
        else
            chain[i]->vpe.run(swfilter);
        chain[i]->activate_send();
    }

    uintptr_t virt = INPUT_BUF_SFFT;
    // TODO + PAGE_SIZE because gem5 accesses 4 bytes beyond the end. apparently, loading a 4-byte
    // float into an XMM register causes an 8-byte load??
    chain[MUL]->vpe.pager()->map_anon(&virt, INPUT_BUF_SIZE + PAGE_SIZE, Pager::Prot::RW, 0);
    MemGate sfftbuf = chain[MUL]->vpe.mem().derive(virt, INPUT_BUF_SIZE);
    sfftbuf.activate_for(chain[SFFT]->vpe, StreamAccel::EP_OUTPUT);

    MemGate ifftbuf = chain[IFFT]->vpe.mem().derive(StreamAccel::BUF_ADDR, bufsize);
    ifftbuf.activate_for(chain[MUL]->vpe, StreamAccel::EP_OUTPUT);

    Errors::Code res;
    if(direct) {
        chain[SFFT]->init(bufsize, bufsize * 2, bufsize, 0);
        chain[MUL]->init(0, bufsize, bufsize / 2, 0);
        chain[IFFT]->init(bufsize, static_cast<size_t>(-1), static_cast<size_t>(-1), 0);

        cycles_t start = Profile::start(1);
        res = execute(rgate, chain, 3, in, out);
        cycles_t end = Profile::stop(1);
        cout << "Exec time: " << (end - start) << " cycles\n";
    }
    else {
        chain[SFFT]->init(bufsize, bufsize, bufsize, 0);
        chain[MUL]->init(0, bufsize, bufsize, 0);
        chain[IFFT]->init(bufsize, bufsize, bufsize, 0);

        cycles_t start = Profile::start(1);
        res = execute_indirect(rgate, chain, 3, in, out, bufsize);
        cycles_t end = Profile::stop(1);
        cout << "Exec time: " << (end - start) << " cycles\n";
    }
    if(res != Errors::NONE)
        errmsg("Operation failed: " << Errors::to_string(res));

    // uint8_t mybuf[1024];
    // FStream fs(out);
    // while(!fs.eof()) {
    //     size_t res = fs.read(mybuf, sizeof(mybuf));
    //     cout << "Got " << res << " bytes\n";
    //     for(size_t i = 0; i < res; ++i) {
    //         cout << fmt(mybuf[i], "#0x", 2) << " ";
    //         if(i % 16 == 15)
    //             cout << "\n";
    //     }
    // }
    // cout << "\n";

    for(auto *a : accel)
        delete a;
}

int main(int argc, char **argv) {
    if(argc < 8)
        exitmsg("Usage: " << argv[0] << " <in> <out> (chain|convol) <direct> <bufsize> <comptime> <num>");

    if(VFS::mount("/", new M3FS("m3fs")) != Errors::NONE) {
        if(Errors::last != Errors::EXISTS)
            exitmsg("Unable to mount filesystem\n");
    }

    const char *in = argv[1];
    const char *out = argv[2];
    const char *mode = argv[3];
    bool direct = IStringStream::read_from<int>(argv[4]);
    size_t bufsize = IStringStream::read_from<size_t>(argv[5]);
    cycles_t comptime = IStringStream::read_from<cycles_t>(argv[6]);
    size_t num = IStringStream::read_from<size_t>(argv[7]);

    cycles_t start = Profile::start(0);
    if(String(mode) == "chain")
        fftchain(in, out, num, bufsize, comptime, direct);
    else
        fftconvolution(in, out, bufsize, direct);
    cycles_t end = Profile::stop(0);

    cout << "Total time: " << (end - start) << " cycles\n";
    return 0;
}
