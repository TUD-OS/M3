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
#define EP_COUNT          16

#define USE_MSGBACKEND      0

namespace m3 {

class Gate;
class RecvGate;
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

    struct Header {
        long int length;        // = mtype -> has to be non-zero
        unsigned char opcode;   // should actually be part of length but causes trouble in msgsnd
        label_t label;
        struct {
            unsigned has_replycap : 1,
                     core : 15,
                     rpl_epid : 8,
                     snd_epid : 8;
        } PACKED;
        label_t replylabel;
        word_t credits : sizeof(word_t) * 8 - 16,
               crd_ep : 16;
    } PACKED;

    struct Buffer : public Header {
        char data[MAX_DATA_SIZE];
    };

public:
    struct Message : public Header {
        int send_epid() const {
            return snd_epid;
        }
        int reply_epid() const {
            return rpl_epid;
        }

        unsigned char data[];
    } PACKED;

    class Backend {
    public:
        virtual ~Backend() {
        }
        virtual void create() = 0;
        virtual void destroy() = 0;
        virtual void reset() = 0;
        virtual void send(int core, int ep, const DTU::Buffer *buf) = 0;
        virtual ssize_t recv(int ep, DTU::Buffer *buf) = 0;
    };

    static constexpr size_t HEADER_SIZE         = sizeof(Buffer) - MAX_DATA_SIZE;

    static constexpr size_t MAX_MSGS            = sizeof(word_t) * 8;

    // command registers
    static constexpr size_t CMD_ADDR            = 0;
    static constexpr size_t CMD_SIZE            = 1;
    static constexpr size_t CMD_EPID            = 2;
    static constexpr size_t CMD_CTRL            = 3;
    static constexpr size_t CMD_OFFSET          = 4;
    static constexpr size_t CMD_REPLYLBL        = 5;
    static constexpr size_t CMD_REPLY_EPID      = 6;
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
    static constexpr size_t EP_EPID             = 9;
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
        ACKMSG  = 7,
    };

    static const int MEM_EP       = 0;
    static const int SYSC_EP      = 1;
    static const int DEF_RECVEP   = 2;

    explicit DTU();

    void reset();

    word_t get_cmd(size_t reg) const {
        return _cmdregs[reg];
    }
    void set_cmd(size_t reg, word_t val) {
        _cmdregs[reg] = val;
    }

    word_t *ep_regs() {
        return const_cast<word_t*>(_epregs);
    }

    word_t get_ep(int i, size_t reg) const {
        return _epregs[i * EPS_RCNT + reg];
    }
    void set_ep(int i, size_t reg, word_t val) {
        _epregs[i * EPS_RCNT + reg] = val;
    }

    static DTU &get() {
        return inst;
    }

    void configure(int i, label_t label, int coreid, int epid, word_t credits) {
        configure(const_cast<word_t*>(_epregs), i, label, coreid, epid, credits);
    }
    static void configure(word_t *eps, int i, label_t label, int coreid, int epid, word_t credits) {
        eps[i * EPS_RCNT + EP_LABEL] = label;
        eps[i * EPS_RCNT + EP_COREID] = coreid;
        eps[i * EPS_RCNT + EP_EPID] = epid;
        eps[i * EPS_RCNT + EP_CREDITS] = credits;
    }

    void configure_recv(int ep, uintptr_t buf, uint order, uint msgorder, int flags);

    void send(int ep, const void *msg, size_t size, label_t replylbl, int replyep) {
        fire(ep, SEND, msg, size, 0, 0, replylbl, replyep);
    }
    void reply(int ep, const void *msg, size_t size, size_t msgidx) {
        fire(ep, REPLY, msg, size, msgidx, 0, label_t(), 0);
    }
    void read(int ep, void *msg, size_t size, size_t off) {
        fire(ep, READ, msg, size, off, size, label_t(), 0);
    }
    void write(int ep, const void *msg, size_t size, size_t off) {
        fire(ep, WRITE, msg, size, off, size, label_t(), 0);
    }
    void cmpxchg(int ep, const void *msg, size_t msgsize, size_t off, size_t size) {
        fire(ep, CMPXCHG, msg, msgsize, off, size, label_t(), 0);
    }
    void sendcrd(int ep, int crdep, size_t size) {
        set_cmd(CMD_EPID, ep);
        set_cmd(CMD_SIZE, size);
        set_cmd(CMD_OFFSET, crdep);
        set_cmd(CMD_CTRL, (SENDCRD << 3) | CTRL_START);
    }

    bool uses_header(int ep) {
        return ~get_ep(ep, EP_BUF_FLAGS) & FLAG_NO_HEADER;
    }

    bool fetch_msg(int ep) {
        return get_ep(ep, EP_BUF_MSGCNT) > 0;
    }

    DTU::Message *message(int ep) const {
        size_t off = get_ep(ep, EP_BUF_ROFF);
        word_t addr = get_ep(ep, EP_BUF_ADDR);
        size_t ord = get_ep(ep, EP_BUF_ORDER);
        return reinterpret_cast<Message*>(addr + (off & ((1UL << ord) - 1)));
    }
    Message *message_at(int ep, size_t msgidx) const {
        word_t addr = get_ep(ep, EP_BUF_ADDR);
        size_t ord = get_ep(ep, EP_BUF_ORDER);
        size_t msgord = get_ep(ep, EP_BUF_MSGORDER);
        return reinterpret_cast<Message*>(addr + ((msgidx << msgord) & ((1UL << ord) - 1)));
    }

    size_t get_msgoff(int ep, RecvGate *rcvgate) const {
        return get_msgoff(ep, rcvgate, message(ep));
    }
    size_t get_msgoff(int ep, RecvGate *, const Message *msg) const {
        word_t addr = get_ep(ep, EP_BUF_ADDR);
        size_t ord = get_ep(ep, EP_BUF_MSGORDER);
        return (reinterpret_cast<word_t>(msg) - addr) >> ord;
    }

    void ack_message(int ep) {
        set_cmd(CMD_EPID, ep);
        set_cmd(CMD_CTRL, (ACKMSG << 3) | CTRL_START);
        wait_until_ready(ep);
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

    void fire(int ep, int op, const void *msg, size_t size, size_t offset, size_t len,
            label_t replylbl, int replyep) {
        assert(((uintptr_t)msg & (DTU_PKG_SIZE - 1)) == 0);
        assert((size & (DTU_PKG_SIZE - 1)) == 0);
        set_cmd(CMD_ADDR, reinterpret_cast<word_t>(msg));
        set_cmd(CMD_SIZE, size);
        set_cmd(CMD_EPID, ep);
        set_cmd(CMD_OFFSET, offset);
        set_cmd(CMD_LENGTH, len);
        set_cmd(CMD_REPLYLBL, replylbl);
        set_cmd(CMD_REPLY_EPID, replyep);
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
    int prepare_reply(int epid, int &dstcore, int &dstep);
    int prepare_send(int epid, int &dstcore, int &dstep);
    int prepare_read(int epid, int &dstcore, int &dstep);
    int prepare_write(int epid, int &dstcore, int &dstep);
    int prepare_cmpxchg(int epid, int &dstcore, int &dstep);
    int prepare_sendcrd(int epid, int &dstcore, int &dstep);
    int prepare_ackmsg(int epid);

    void send_msg(int epid, int dstcoreid, int dstepid, bool isreply);
    void handle_read_cmd(int epid);
    void handle_write_cmd(int epid);
    void handle_resp_cmd();
    void handle_cmpxchg_cmd(int epid);
    void handle_command(int core);
    void handle_receive(int i);

    static int check_cmd(int ep, int op, word_t addr, word_t credits, size_t offset, size_t length);
    static void *thread(void *arg);

    volatile bool _run;
    volatile word_t _cmdregs[CMDS_RCNT];
    // have to be aligned by 8 because it shouldn't collide with MemGate::RWX bits
    alignas(8) volatile word_t _epregs[EPS_RCNT * EP_COUNT];
    Backend *_backend;
    pthread_t _tid;
    static Buffer _buf;
    static DTU inst;
};

}
