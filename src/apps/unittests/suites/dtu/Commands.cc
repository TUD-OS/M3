/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <m3/cap/MemGate.h>
#include <m3/Log.h>
#include <m3/RecvBuf.h>
#include <sys/mman.h>

#include "Commands.h"

using namespace m3;

static void *map_page() {
    void *addr = mmap(0, 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON,-1,0);
    if(addr == MAP_FAILED) {
        LOG(DEF, "mmap failed. Skipping test.");
        return nullptr;
    }
    return addr;
}
static void unmap_page(void *addr) {
    munmap(addr, 0x1000);
}

void CommandsTestSuite::ReadCmdTestCase::run() {
    const size_t rcvchanid = 3;
    const size_t sndchanid = 4;
    DTU &dtu = DTU::get();
    RecvBuf buf = RecvBuf::create(sndchanid, nextlog2<256>::val, nextlog2<128>::val, 0);
    // only necessary to set the msgqid
    RecvBuf rbuf = RecvBuf::create(rcvchanid, nextlog2<64>::val, RecvBuf::NO_RINGBUF);

    void *addr = map_page();
    if(!addr)
        return;

    const size_t datasize = sizeof(word_t) * 4;
    word_t *data = reinterpret_cast<word_t*>(addr);
    data[0] = 1234;
    data[1] = 5678;
    data[2] = 1122;
    data[3] = 3344;

    Serial::get() << "-- Test errors --\n";
    {
        dtu.configure(sndchanid, reinterpret_cast<word_t>(data) | MemGate::R, coreid(),
            rcvchanid, datasize);

        dmacmd(nullptr, 0, sndchanid, 0, datasize, DTU::WRITE);
        assert_true(dtu.get_cmd(DTU::CMD_CTRL) & DTU::CTRL_ERROR);

        dmacmd(nullptr, 0, sndchanid, 0, datasize + 1, DTU::READ);
        assert_true(dtu.get_cmd(DTU::CMD_CTRL) & DTU::CTRL_ERROR);

        dmacmd(nullptr, 0, sndchanid, datasize, 0, DTU::READ);
        assert_true(dtu.get_cmd(DTU::CMD_CTRL) & DTU::CTRL_ERROR);

        dmacmd(nullptr, 0, sndchanid, sizeof(word_t), datasize, DTU::READ);
        assert_true(dtu.get_cmd(DTU::CMD_CTRL) & DTU::CTRL_ERROR);
    }

    Serial::get() << "-- Test reading --\n";
    {
        dtu.configure(sndchanid, reinterpret_cast<word_t>(data) | MemGate::R, coreid(),
            rcvchanid, datasize);

        word_t buf[datasize / sizeof(word_t)];

        dmacmd(buf, datasize, sndchanid, 0, datasize, DTU::READ);
        assert_false(dtu.get_cmd(DTU::CMD_CTRL) & DTU::CTRL_ERROR);
        dtu.wait_for_mem_cmd();
        for(size_t i = 0; i < 4; ++i)
            assert_word(buf[i], data[i]);
    }

    unmap_page(addr);
    dtu.configure(sndchanid, 0, 0, 0, 0);
}

void CommandsTestSuite::WriteCmdTestCase::run() {
    const size_t rcvchanid = 3;
    const size_t sndchanid = 4;
    DTU &dtu = DTU::get();
    RecvBuf buf = RecvBuf::create(sndchanid, nextlog2<64>::val, nextlog2<32>::val, 0);
    // only necessary to set the msgqid
    RecvBuf rbuf = RecvBuf::create(rcvchanid, nextlog2<64>::val, RecvBuf::NO_RINGBUF);

    void *addr = map_page();
    if(!addr)
        return;

    Serial::get() << "-- Test errors --\n";
    {
        word_t data[] = {1234, 5678, 1122, 3344};
        dtu.configure(sndchanid, reinterpret_cast<word_t>(addr) | MemGate::W, coreid(),
            rcvchanid, sizeof(data));

        dmacmd(nullptr, 0, sndchanid, 0, sizeof(data), DTU::READ);
        assert_true(dtu.get_cmd(DTU::CMD_CTRL) & DTU::CTRL_ERROR);
    }

    Serial::get() << "-- Test writing --\n";
    {
        word_t data[] = {1234, 5678, 1122, 3344};
        dtu.configure(sndchanid, reinterpret_cast<word_t>(addr) | MemGate::W, coreid(),
            rcvchanid, sizeof(data));

        dmacmd(data, sizeof(data), sndchanid, 0, sizeof(data), DTU::WRITE);
        assert_false(dtu.get_cmd(DTU::CMD_CTRL) & DTU::CTRL_ERROR);
        getmsg(rcvchanid, 1);
        for(size_t i = 0; i < sizeof(data) / sizeof(data[0]); ++i)
            assert_word(reinterpret_cast<word_t*>(addr)[i], data[i]);
        ChanMng::get().ack_message(rcvchanid);
    }

    unmap_page(addr);
    dtu.configure(sndchanid, 0, 0, 0, 0);
}

void CommandsTestSuite::CmpxchgCmdTestCase::run() {
    const size_t rcvchanid = 3;
    const size_t sndchanid = 4;
    DTU &dtu = DTU::get();
    RecvBuf buf = RecvBuf::create(sndchanid, nextlog2<128>::val, nextlog2<64>::val, 0);
    // only necessary to set the msgqid
    RecvBuf rbuf = RecvBuf::create(rcvchanid, nextlog2<1>::val, RecvBuf::NO_RINGBUF);

    void *addr = map_page();
    if(!addr)
        return;

    const size_t refdatasize = sizeof(word_t) * 2;
    word_t *refdata = reinterpret_cast<word_t*>(addr);
    refdata[0] = 1234;
    refdata[1] = 5678;

    Serial::get() << "-- Test errors --\n";
    {
        word_t data[] = {1234, 567, 1122, 3344};
        dtu.configure(sndchanid, reinterpret_cast<word_t>(refdata) | MemGate::X, coreid(),
            rcvchanid, refdatasize);

        dmacmd(data, sizeof(data), sndchanid, 0, sizeof(refdata), DTU::READ);
        assert_true(dtu.get_cmd(DTU::CMD_CTRL) & DTU::CTRL_ERROR);

        dmacmd(data, sizeof(data), sndchanid, 0, sizeof(data), DTU::CMPXCHG);
        assert_true(dtu.get_cmd(DTU::CMD_CTRL) & DTU::CTRL_ERROR);
    }

    Serial::get() << "-- Test failure --\n";
    {
        word_t data[] = {1234, 567, 1122, 3344};
        dtu.configure(sndchanid, reinterpret_cast<word_t>(refdata) | MemGate::X, coreid(),
            rcvchanid, refdatasize);

        dmacmd(data, sizeof(data), sndchanid, 0, refdatasize, DTU::CMPXCHG);
        assert_false(dtu.wait_for_mem_cmd());
        assert_word(refdata[0], 1234);
        assert_word(refdata[1], 5678);
    }

    Serial::get() << "-- Test success --\n";
    {
        word_t data[] = {1234, 5678, 1122, 3344};
        dtu.configure(sndchanid, reinterpret_cast<word_t>(refdata) | MemGate::X, coreid(),
            rcvchanid, refdatasize);

        dmacmd(data, sizeof(data), sndchanid, 0, refdatasize, DTU::CMPXCHG);
        assert_true(dtu.wait_for_mem_cmd());
        assert_word(refdata[0], 1122);
        assert_word(refdata[1], 3344);
    }

    Serial::get() << "-- Test offset --\n";
    {
        word_t data[] = {3344, 4455};
        dtu.configure(sndchanid, reinterpret_cast<word_t>(refdata) | MemGate::X, coreid(),
            rcvchanid, refdatasize);

        dmacmd(data, sizeof(data), sndchanid, sizeof(word_t), sizeof(word_t), DTU::CMPXCHG);
        assert_true(dtu.wait_for_mem_cmd());
        assert_word(refdata[0], 1122);
        assert_word(refdata[1], 4455);
    }

    unmap_page(addr);
    dtu.configure(sndchanid, 0, 0, 0, 0);
}
