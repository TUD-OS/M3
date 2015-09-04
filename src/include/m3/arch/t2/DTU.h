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

#pragma once

#include <m3/Common.h>
#include <m3/Config.h>
#include <m3/stream/OStream.h>
#include <m3/tracing/Tracing.h>
#include <m3/util/Util.h>
#include <assert.h>

#define DTU_PKG_SIZE        (static_cast<size_t>(8))

namespace m3 {

class DTU {
    static const uintptr_t DRAM_START       = 0x8000;

    explicit DTU() {
        reset();
    }

public:
    struct Header {
        label_t label;
        struct {
            word_t has_replycap : 1,
                   core : 15,
                   chanid : 16;
        } PACKED;
        label_t replylabel;
        word_t length;        // has to be non-zero
    } PACKED;

    static const size_t HEADER_SIZE         = sizeof(Header);
    static const size_t PACKET_SIZE         = 8;

    // TODO not yet supported
    static const int FLAG_NO_RINGBUF        = 0;
    static const int FLAG_NO_HEADER         = 0;

    enum Operation {
        WRITE   = 0x0,      // write from local to remote
        READ    = 0x1,      // read from remote to local
        CMPXCHG = 0x2,
        SEND    = 0x3,
        REPLY   = 0x4,
    };

    static DTU &get() {
        return inst;
    }

    void reset();

    void configure(int i, label_t label, int coreid, int chanid, word_t credits) {
        ChanConf *cfg = conf(i);
        cfg->label = label;
        cfg->dstcore = coreid;
        cfg->dstchan = chanid;
        cfg->credits = credits;
    }

    void configure_recv(UNUSED int chan, UNUSED uintptr_t buf, UNUSED uint order, UNUSED uint msgorder, UNUSED int flags) {
        // nothing to do
    }

    void send(int chan, const void *msg, size_t size, label_t replylbl, int reply_chan);
    void reply(int chan, const void *msg, size_t size, size_t msgidx);
    void read(int chan, void *msg, size_t size, size_t off);
    void write(int chan, const void *msg, size_t size, size_t off);
    void cmpxchg(UNUSED int chan, UNUSED const void *msg, UNUSED size_t msgsize, UNUSED size_t off, UNUSED size_t size) {
    }
    void sendcrd(UNUSED int chan, UNUSED int crdchan, UNUSED size_t size) {
    }

    bool wait() {
        return true;
    }

    void wait_until_ready(int) {
        volatile uint *ptr = reinterpret_cast<uint*>(PE_IDMA_OVERALL_SLOT_STATUS_ADDRESS);
        while(ptr[0] != 0)
            ;
        EVENT_TRACE_MEM_FINISH();
    }

    bool wait_for_mem_cmd() {
        // we've already waited
        return true;
    }
    size_t get_remaining(int chan);

    size_t recvbuf_offset(int core, int chan) {
        assert(core >= FIRST_PE_ID);
        return (core - FIRST_PE_ID) * RECV_BUF_MSGSIZE * CHAN_COUNT + chan * RECV_BUF_MSGSIZE;
    }

    void set_target(int slot, uchar dst, uintptr_t addr, uint credits = 0xFF, uchar perm = 0x3);
    void fire(int slot, Operation op, const void *msg, size_t size);


private:
    void check_rw_access(uintptr_t base, size_t len, size_t off, size_t size, int perms, int type);

    ChanConf *conf(int chan) {
        return coreconf()->chans + chan;
    }

    static DTU inst;
};

}

#ifdef __t2_sim__
#   include <m3/arch/t2-sim/DTU.h>
#else
#   include <m3/arch/t2-chip/DTU.h>
#endif
