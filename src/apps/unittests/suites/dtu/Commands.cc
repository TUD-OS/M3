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

#include <base/Env.h>

#include <m3/com/MemGate.h>
#include <m3/com/RecvBuf.h>
#include <m3/stream/Standard.h>

#include <sys/mman.h>

#include "Commands.h"

using namespace m3;

static void *map_page() {
    void *addr = mmap(0, 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if(addr == MAP_FAILED) {
        exitmsg("mmap failed. Skipping test.");
        return nullptr;
    }
    return addr;
}
static void unmap_page(void *addr) {
    munmap(addr, 0x1000);
}

void CommandsTestSuite::ReadCmdTestCase::run() {
    const epid_t rcvep = VPE::self().alloc_ep();
    const epid_t sndep = VPE::self().alloc_ep();
    DTU &dtu = DTU::get();

    void *addr = map_page();
    if(!addr)
        return;

    const size_t datasize = sizeof(word_t) * 4;
    word_t *data = reinterpret_cast<word_t*>(addr);
    data[0] = 1234;
    data[1] = 5678;
    data[2] = 1122;
    data[3] = 3344;

    cout << "-- Test errors --\n";
    {
        dtu.configure(sndep, reinterpret_cast<word_t>(data) | MemGate::R, env()->coreid,
            rcvep, datasize);

        dmacmd(nullptr, 0, sndep, 0, datasize, DTU::WRITE);
        assert_true(dtu.get_cmd(DTU::CMD_CTRL) & DTU::CTRL_ERROR);

        dmacmd(nullptr, 0, sndep, 0, datasize + 1, DTU::READ);
        assert_true(dtu.get_cmd(DTU::CMD_CTRL) & DTU::CTRL_ERROR);

        dmacmd(nullptr, 0, sndep, datasize, 0, DTU::READ);
        assert_true(dtu.get_cmd(DTU::CMD_CTRL) & DTU::CTRL_ERROR);

        dmacmd(nullptr, 0, sndep, sizeof(word_t), datasize, DTU::READ);
        assert_true(dtu.get_cmd(DTU::CMD_CTRL) & DTU::CTRL_ERROR);
    }

    cout << "-- Test reading --\n";
    {
        dtu.configure(sndep, reinterpret_cast<word_t>(data) | MemGate::R, env()->coreid,
            rcvep, datasize);

        word_t buf[datasize / sizeof(word_t)];

        dmacmd(buf, datasize, sndep, 0, datasize, DTU::READ);
        assert_false(dtu.get_cmd(DTU::CMD_CTRL) & DTU::CTRL_ERROR);
        dtu.wait_for_mem_cmd();
        for(size_t i = 0; i < 4; ++i)
            assert_word(buf[i], data[i]);
    }

    unmap_page(addr);
    dtu.configure(sndep, 0, 0, 0, 0);
    VPE::self().free_ep(sndep);
    VPE::self().free_ep(rcvep);
}

void CommandsTestSuite::WriteCmdTestCase::run() {
    const epid_t rcvep = VPE::self().alloc_ep();
    const epid_t sndep = VPE::self().alloc_ep();
    DTU &dtu = DTU::get();

    void *addr = map_page();
    if(!addr)
        return;

    cout << "-- Test errors --\n";
    {
        word_t data[] = {1234, 5678, 1122, 3344};
        dtu.configure(sndep, reinterpret_cast<word_t>(addr) | MemGate::W, env()->coreid,
            rcvep, sizeof(data));

        dmacmd(nullptr, 0, sndep, 0, sizeof(data), DTU::READ);
        assert_true(dtu.get_cmd(DTU::CMD_CTRL) & DTU::CTRL_ERROR);
    }

    cout << "-- Test writing --\n";
    {
        word_t data[] = {1234, 5678, 1122, 3344};
        dtu.configure(sndep, reinterpret_cast<word_t>(addr) | MemGate::W, env()->coreid,
            rcvep, sizeof(data));

        dmacmd(data, sizeof(data), sndep, 0, sizeof(data), DTU::WRITE);
        assert_false(dtu.get_cmd(DTU::CMD_CTRL) & DTU::CTRL_ERROR);
        volatile const word_t *words = reinterpret_cast<const word_t*>(addr);
        // TODO we do current not know when this is finished
        while(words[0] == 0)
            ;
        for(size_t i = 0; i < sizeof(data) / sizeof(data[0]); ++i)
            assert_word(words[i], data[i]);
    }

    unmap_page(addr);
    dtu.configure(sndep, 0, 0, 0, 0);
    VPE::self().free_ep(sndep);
    VPE::self().free_ep(rcvep);
}

void CommandsTestSuite::CmpxchgCmdTestCase::run() {
    const epid_t rcvep = VPE::self().alloc_ep();
    const epid_t sndep = VPE::self().alloc_ep();
    DTU &dtu = DTU::get();

    void *addr = map_page();
    if(!addr)
        return;

    const size_t refdatasize = sizeof(word_t) * 2;
    word_t *refdata = reinterpret_cast<word_t*>(addr);
    refdata[0] = 1234;
    refdata[1] = 5678;

    cout << "-- Test errors --\n";
    {
        word_t data[] = {1234, 567, 1122, 3344};
        dtu.configure(sndep, reinterpret_cast<word_t>(refdata) | MemGate::X, env()->coreid,
            rcvep, refdatasize);

        dmacmd(data, sizeof(data), sndep, 0, sizeof(refdata), DTU::READ);
        assert_true(dtu.get_cmd(DTU::CMD_CTRL) & DTU::CTRL_ERROR);

        dmacmd(data, sizeof(data), sndep, 0, sizeof(data), DTU::CMPXCHG);
        assert_true(dtu.get_cmd(DTU::CMD_CTRL) & DTU::CTRL_ERROR);
    }

    cout << "-- Test failure --\n";
    {
        word_t data[] = {1234, 567, 1122, 3344};
        dtu.configure(sndep, reinterpret_cast<word_t>(refdata) | MemGate::X, env()->coreid,
            rcvep, refdatasize);

        dmacmd(data, sizeof(data), sndep, 0, refdatasize, DTU::CMPXCHG);
        assert_false(dtu.wait_for_mem_cmd());
        assert_word(refdata[0], 1234);
        assert_word(refdata[1], 5678);
    }

    cout << "-- Test success --\n";
    {
        word_t data[] = {1234, 5678, 1122, 3344};
        dtu.configure(sndep, reinterpret_cast<word_t>(refdata) | MemGate::X, env()->coreid,
            rcvep, refdatasize);

        dmacmd(data, sizeof(data), sndep, 0, refdatasize, DTU::CMPXCHG);
        assert_true(dtu.wait_for_mem_cmd());
        assert_word(refdata[0], 1122);
        assert_word(refdata[1], 3344);
    }

    cout << "-- Test offset --\n";
    {
        word_t data[] = {3344, 4455};
        dtu.configure(sndep, reinterpret_cast<word_t>(refdata) | MemGate::X, env()->coreid,
            rcvep, refdatasize);

        dmacmd(data, sizeof(data), sndep, sizeof(word_t), sizeof(word_t), DTU::CMPXCHG);
        assert_true(dtu.wait_for_mem_cmd());
        assert_word(refdata[0], 1122);
        assert_word(refdata[1], 4455);
    }

    unmap_page(addr);
    dtu.configure(sndep, 0, 0, 0, 0);
    VPE::self().free_ep(sndep);
    VPE::self().free_ep(rcvep);
}
