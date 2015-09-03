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

    // command registers
    static constexpr size_t CMD_ADDR            = 0;
    static constexpr size_t CMD_SIZE            = 1;
    static constexpr size_t CMD_CHANID          = 2;
    static constexpr size_t CMD_CTRL            = 3;
    static constexpr size_t CMD_OFFSET          = 4;
    static constexpr size_t CMD_REPLYLBL        = 5;
    static constexpr size_t CMD_REPLY_CHANID    = 6;
    static constexpr size_t CMD_LENGTH          = 7;

    // register starts and counts (cont.)
    static constexpr size_t CMDS_RCNT           = 1 + CMD_LENGTH;

    // receive buffer registers
    static constexpr size_t EP_BUF_ADDR         = 0;
    static constexpr size_t EP_BUF_ORDER        = 1;
    static constexpr size_t EP_BUF_MSGORDER     = 2;
    static constexpr size_t EP_BUF_ROFF         = 3;
    static constexpr size_t EP_BUF_WOFF         = 4;
    static constexpr size_t EP_BUF_MSGCNT       = 5;
    static constexpr size_t EP_BUF_MSGQID       = 6;
    static constexpr size_t EP_BUF_FLAGS        = 7;

    // for sending message and accessing memory
    static constexpr size_t EP_COREID           = 8;
    static constexpr size_t EP_CHANID           = 9;
    static constexpr size_t EP_LABEL            = 10;
    static constexpr size_t EP_CREDITS          = 11;

    // bits in EP_BUF_FLAGS register
    static constexpr word_t FLAG_NO_RINGBUF     = 0x1;
    static constexpr word_t FLAG_NO_HEADER      = 0x2;

    // bits in ctrl register
    static constexpr word_t CTRL_START          = 0x1;
    static constexpr word_t CTRL_DEL_REPLY_CAP  = 0x2;
    static constexpr word_t CTRL_ERROR          = 0x4;

    // register counts (cont.)
    static constexpr size_t EPS_RCNT            = 1 + EP_CREDITS;

    enum Op {
        READ    = 0,
        WRITE   = 1,
        CMPXCHG = 2,
        SEND    = 3,
        REPLY   = 4,
        RESP    = 5,
        SENDCRD = 6,
    };

    explicit DTU();

    void reset();

    word_t get_cmd(size_t reg) {
        return _cmdregs[reg];
    }
    void set_cmd(size_t reg, word_t val) {
        _cmdregs[reg] = val;
    }

    word_t *ep_regs() {
        return const_cast<word_t*>(_epregs);
    }

    word_t get_ep(int i, size_t reg) {
        return _epregs[i * EPS_RCNT + reg];
    }
    void set_ep(int i, size_t reg, word_t val) {
        _epregs[i * EPS_RCNT + reg] = val;
    }

    static DTU &get() {
        return inst;
    }

    void configure(int i, label_t label, int coreid, int chanid, word_t credits) {
        configure(const_cast<word_t*>(_epregs), i, label, coreid, chanid, credits);
    }
    static void configure(word_t *eps, int i, label_t label, int coreid, int chanid, word_t credits) {
        eps[i * EPS_RCNT + EP_LABEL] = label;
        eps[i * EPS_RCNT + EP_COREID] = coreid;
        eps[i * EPS_RCNT + EP_CHANID] = chanid;
        eps[i * EPS_RCNT + EP_CREDITS] = credits;
    }

    void configure_recv(int chan, uintptr_t buf, uint order, uint msgorder, int flags);

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
    volatile word_t _cmdregs[CMDS_RCNT];
    // have to be aligned by 8 because it shouldn't collide with MemGate::RWX bits
    alignas(8) volatile word_t _epregs[EPS_RCNT * CHAN_COUNT];
    Backend *_backend;
    pthread_t _tid;
    static Buffer _buf;
    static DTU inst;
};

}
