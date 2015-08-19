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
#include <m3/util/String.h>
#include <m3/util/Util.h>
#include <pthread.h>
#include <ostream>
#include <iomanip>
#include <assert.h>

// bad place, but prevents circular dependencies of headers
#define HEAP_SIZE           (1024 * 1024)

// we have no alignment or size requirements here
#define DTU_PKG_SIZE        (static_cast<size_t>(8))
#define CHAN_COUNT          16

#define USE_MSGBACKEND      0

namespace m3 {

class Gate;
class MsgBackend;
class SocketBackend;

class DTU {
    friend class Gate;
    friend class MsgBackend;
    friend class SocketBackend;

#if USE_MSGBACKEND
    static constexpr size_t MAX_DATA_SIZE   = 8192 - (sizeof(long int) + sizeof(word_t) * 4);
#else
    static constexpr size_t MAX_DATA_SIZE   = HEAP_SIZE;
#endif

    struct Buffer {
        long int length;        // = mtype -> has to be non-zero
        unsigned char opcode;   // should actually be part of length but causes trouble in msgsnd
        label_t label;
        struct {
            unsigned has_replycap : 1,
                     core : 15,
                     rpl_chanid : 8,
                     snd_chanid : 8;
        } PACKED;
        label_t replylabel;
        word_t credits : sizeof(word_t) * 8 - 16,
               crd_chan : 16;
        char data[MAX_DATA_SIZE];
    } PACKED;

public:
    class Backend {
    public:
        virtual ~Backend() {
        }
        virtual void create() = 0;
        virtual void destroy() = 0;
        virtual void reset() = 0;
        virtual void send(int core, int chan, const DTU::Buffer *buf) = 0;
        virtual ssize_t recv(int chan, DTU::Buffer *buf) = 0;
    };

    static constexpr size_t HEADER_SIZE         = sizeof(Buffer) - MAX_DATA_SIZE;

    static constexpr size_t MAX_MSGS            = sizeof(word_t) * 8;

    // register starts and counts
    static constexpr size_t CMDS_START          = 0;

    // command registers
    static constexpr size_t CMD_ADDR            = CMDS_START + 0;
    static constexpr size_t CMD_SIZE            = CMDS_START + 1;
    static constexpr size_t CMD_CHANID          = CMDS_START + 2;
    static constexpr size_t CMD_CTRL            = CMDS_START + 3;
    static constexpr size_t CMD_OFFSET          = CMDS_START + 4;
    static constexpr size_t CMD_REPLYLBL        = CMDS_START + 5;
    static constexpr size_t CMD_REPLY_CHANID    = CMDS_START + 6;
    static constexpr size_t CMD_LENGTH          = CMDS_START + 7;

    // register starts and counts (cont.)
    static constexpr size_t CMDS_RCNT           = 1 + CMD_LENGTH - CMD_ADDR;
    static constexpr size_t REPS_START          = CMDS_RCNT;
    static constexpr size_t SEPS_START          = 0;

    // REP registers
    static constexpr size_t REP_ADDR            = REPS_START + 0;
    static constexpr size_t REP_ORDER           = REPS_START + 1;
    static constexpr size_t REP_MSGORDER        = REPS_START + 2;
    static constexpr size_t REP_ROFF            = REPS_START + 3;
    static constexpr size_t REP_WOFF            = REPS_START + 4;
    static constexpr size_t REP_MSGCNT          = REPS_START + 5;
    static constexpr size_t REP_MSGQID          = REPS_START + 6;
    static constexpr size_t REP_FLAGS           = REPS_START + 7;
    static constexpr size_t REP_VALID_MASK      = REPS_START + 8;

    // SEP registers
    static constexpr size_t SEP_COREID          = SEPS_START + 0;
    static constexpr size_t SEP_CHANID          = SEPS_START + 1;
    static constexpr size_t SEP_LABEL           = SEPS_START + 2;
    static constexpr size_t SEP_CREDITS         = SEPS_START + 3;

    // bits in REP flags register
    static constexpr word_t FLAG_NO_RINGBUF     = 0x1;
    static constexpr word_t FLAG_NO_HEADER      = 0x2;

    // bits in ctrl register
    static constexpr word_t CTRL_START          = 0x1;
    static constexpr word_t CTRL_DEL_REPLY_CAP  = 0x2;
    static constexpr word_t CTRL_ERROR          = 0x4;

    // register counts (cont.)
    static constexpr size_t REPS_RCNT           = 1 + REP_VALID_MASK - REPS_START;
    static constexpr size_t SEPS_RCNT           = 1 + SEP_CREDITS - SEPS_START;

    // total regs count
    static constexpr size_t LREG_COUNT          = CMDS_RCNT + CHAN_COUNT * REPS_RCNT;
    static constexpr size_t RREG_COUNT          = CHAN_COUNT * SEPS_RCNT;

    explicit DTU();

    void reset() {
        _backend->reset();
    }

    word_t *sep_regs() {
        return const_cast<word_t*>(_rregs);
    }

    word_t get_cmd(size_t reg) {
        return _lregs[reg];
    }
    void set_cmd(size_t reg, word_t val) {
        _lregs[reg] = val;
    }

    word_t get_sep(int i, size_t reg) {
        return _rregs[i * SEPS_RCNT + reg];
    }
    void set_sep(int i, size_t reg, word_t val) {
        _rregs[i * SEPS_RCNT + reg] = val;
    }

    word_t get_rep(int i, size_t reg) {
        return _lregs[i * REPS_RCNT + reg];
    }
    void set_rep(int i, size_t reg, word_t val);

    enum Op {
        READ    = 0,
        WRITE   = 1,
        CMPXCHG = 2,
        SEND    = 3,
        REPLY   = 4,
        RESP    = 5,
        SENDCRD = 6,
    };

    static DTU &get() {
        return inst;
    }

    void configure(int i, label_t label, int coreid, int chanid, word_t credits) {
        configure(const_cast<word_t*>(_rregs), i, label, coreid, chanid, credits);
    }
    static void configure(word_t *seps, int i, label_t label, int coreid, int chanid, word_t credits) {
        seps[i * SEPS_RCNT + SEP_LABEL] = label;
        seps[i * SEPS_RCNT + SEP_COREID] = coreid;
        seps[i * SEPS_RCNT + SEP_CHANID] = chanid;
        seps[i * SEPS_RCNT + SEP_CREDITS] = credits;
    }

    void send(int chan, const void *msg, size_t size, label_t replylbl, int replychan) {
        fire(chan, SEND, msg, size, 0, 0, replylbl, replychan);
    }
    void reply(int chan, const void *msg, size_t size, size_t msgidx) {
        fire(chan, REPLY, msg, size, msgidx, 0, label_t(), 0);
    }
    void read(int chan, void *msg, size_t size, size_t off) {
        fire(chan, READ, msg, size, off, size, label_t(), 0);
    }
    void write(int chan, const void *msg, size_t size, size_t off) {
        fire(chan, WRITE, msg, size, off, size, label_t(), 0);
    }
    void cmpxchg(int chan, const void *msg, size_t msgsize, size_t off, size_t size) {
        fire(chan, CMPXCHG, msg, msgsize, off, size, label_t(), 0);
    }
    void sendcrd(int chan, int crdchan, size_t size) {
        set_cmd(CMD_CHANID, chan);
        set_cmd(CMD_SIZE, size);
        set_cmd(CMD_OFFSET, crdchan);
        set_cmd(CMD_CTRL, (SENDCRD << 3) | CTRL_START);
    }

    bool is_ready() {
        return (get_cmd(CMD_CTRL) & CTRL_START) == 0;
    }
    bool wait_for_mem_cmd() {
        while((get_cmd(CMD_CTRL) & CTRL_ERROR) == 0 && get_cmd(CMD_SIZE) > 0)
            wait();
        return (get_cmd(CMD_CTRL) & CTRL_ERROR) == 0;
    }
    void wait_until_ready(int) {
        while(!is_ready())
            wait();
    }

    void set_receiving(int chan, uintptr_t buf, uint order, uint msgorder, int flags);

    void fire(int chan, int op, const void *msg, size_t size, size_t offset, size_t len,
            label_t replylbl, int replychan) {
        assert(((uintptr_t)msg & (DTU_PKG_SIZE - 1)) == 0);
        assert((size & (DTU_PKG_SIZE - 1)) == 0);
        set_cmd(CMD_ADDR, reinterpret_cast<word_t>(msg));
        set_cmd(CMD_SIZE, size);
        set_cmd(CMD_CHANID, chan);
        set_cmd(CMD_OFFSET, offset);
        set_cmd(CMD_LENGTH, len);
        set_cmd(CMD_REPLYLBL, replylbl);
        set_cmd(CMD_REPLY_CHANID, replychan);
        if(op == REPLY)
            set_cmd(CMD_CTRL, (op << 3) | CTRL_START);
        else
            set_cmd(CMD_CTRL, (op << 3) | CTRL_START | CTRL_DEL_REPLY_CAP);
    }

    void start();
    void stop() {
        _run = false;
    }
    pthread_t tid() const {
        return _tid;
    }
    bool wait();

private:
    int prepare_reply(int chanid, int &dstcore, int &dstchan);
    int prepare_send(int chanid, int &dstcore, int &dstchan);
    int prepare_read(int chanid, int &dstcore, int &dstchan);
    int prepare_write(int chanid, int &dstcore, int &dstchan);
    int prepare_cmpxchg(int chanid, int &dstcore, int &dstchan);
    int prepare_sendcrd(int chanid, int &dstcore, int &dstchan);

    void send_msg(int chanid, int dstcoreid, int dstchanid, bool isreply);
    void handle_read_cmd(int chanid);
    void handle_write_cmd(int chanid);
    void handle_resp_cmd();
    void handle_cmpxchg_cmd(int chanid);
    void handle_command(int core);
    void handle_receive(int i);

    static int check_cmd(int chan, int op, word_t addr, word_t credits, size_t offset, size_t length);
    static void *thread(void *arg);

    volatile bool _run;
    // have to be aligned by 8 because it shouldn't collide with MemGate::RWX bits
    alignas(8) volatile word_t _rregs[RREG_COUNT];
    volatile word_t _lregs[LREG_COUNT];
    Backend *_backend;
    pthread_t _tid;
    static Buffer _buf;
    static DTU inst;
};

}
