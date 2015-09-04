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

#include <m3/arch/host/HWInterrupts.h>
#include <m3/arch/host/DTUBackend.h>
#include <m3/cap/MemGate.h>
#include <m3/Log.h>
#include <m3/DTU.h>
#include <m3/Config.h>
#include <cstdio>
#include <cstring>
#include <sstream>

namespace m3 {

static void dumpBytes(uint8_t *bytes, size_t length) {
    std::ostringstream tmp;
    tmp << std::hex << std::setfill('0');
    for(size_t i = 0; i < length; ++i) {
        if(i > 0 && i % 8 == 0) {
            LOG(DTUERR, "  " << tmp.str().c_str());
            tmp = std::ostringstream();
            tmp << std::hex << std::setfill('0');
        }
        tmp << "0x" << std::setw(2) << (unsigned)bytes[i] << " ";
    }
    if(!tmp.str().empty())
        LOG(DTUERR, "  " << tmp.str().c_str());
}

DTU DTU::inst INIT_PRIORITY(106);
DTU::Buffer DTU::_buf INIT_PRIORITY(106);

DTU::DTU() : _run(true), _cmdregs(), _epregs(), _tid() {
}

void DTU::start() {
#if USE_MSGBACKEND
    _backend = new MsgBackend();
#else
    _backend = new SocketBackend();
#endif
    if(Config::get().is_kernel())
        _backend->create();

    int res = pthread_create(&_tid, nullptr, thread, this);
    if(res != 0)
        PANIC("pthread_create");
}

void DTU::reset() {
    memset(ep_regs(), 0, EPS_RCNT * CHAN_COUNT * sizeof(word_t));

    _backend->reset();
}

bool DTU::wait() {
    usleep(1);
    return _run;
}

void DTU::configure_recv(int chan, uintptr_t buf, uint order, uint msgorder, int flags) {
    set_ep(chan, EP_BUF_ADDR, buf);
    set_ep(chan, EP_BUF_ORDER, order);
    set_ep(chan, EP_BUF_MSGORDER, msgorder);
    set_ep(chan, EP_BUF_ROFF, 0);
    set_ep(chan, EP_BUF_WOFF, 0);
    set_ep(chan, EP_BUF_MSGCNT, 0);
    set_ep(chan, EP_BUF_FLAGS, flags);
}

int DTU::check_cmd(int chan, int op, word_t label, word_t credits, size_t offset, size_t length) {
    if(op == READ || op == WRITE || op == CMPXCHG) {
        uint perms = label & MemGate::RWX;
        if(!(perms & (1 << op))) {
            LOG(DTUERR, "DMA-error: operation not permitted on chan " << chan << " (perms="
                    << perms << ", op=" << op << ")");
            return CTRL_ERROR;
        }
        if(offset >= credits || offset + length < offset || offset + length > credits) {
            LOG(DTUERR, "DMA-error: invalid parameters (credits=" << credits
                    << ", offset=" << offset << ", datalen=" << length << ")");
            return CTRL_ERROR;
        }
    }
    return 0;
}

int DTU::prepare_reply(int chanid,int &dstcore,int &dstchan) {
    const void *src = reinterpret_cast<const void*>(get_cmd(CMD_ADDR));
    const size_t size = get_cmd(CMD_SIZE);
    const size_t reply = get_cmd(CMD_OFFSET);

    if(get_ep(chanid, EP_BUF_FLAGS) & FLAG_NO_HEADER) {
        LOG(DTUERR, "DMA-error: want to reply, but header is disabled");
        return CTRL_ERROR;
    }

    const word_t msgord = get_ep(chanid, EP_BUF_MSGORDER);
    const word_t ringbuf = get_ep(chanid, EP_BUF_ADDR);
    Buffer *buf = reinterpret_cast<Buffer*>(ringbuf + (reply << msgord));
    assert(buf->has_replycap);

    if(reply >= MAX_MSGS || !buf->has_replycap) {
        LOG(DTUERR, "DMA-error: invalid reply index (idx=" << reply << ", chan=" << chanid << ")");
        return CTRL_ERROR;
    }

    dstcore = buf->core;
    dstchan = buf->rpl_chanid;
    _buf.label = buf->replylabel;
    _buf.credits = buf->length + HEADER_SIZE;
    _buf.crd_chan = buf->snd_chanid;
    _buf.length = size;
    memcpy(_buf.data, src, size);
    // invalidate message for replying
    buf->has_replycap = false;
    return 0;
}

int DTU::prepare_send(int chanid,int &dstcore,int &dstchan) {
    const void *src = reinterpret_cast<const void*>(get_cmd(CMD_ADDR));
    const word_t credits = get_ep(chanid, EP_CREDITS);
    const size_t size = get_cmd(CMD_SIZE);
    // check if we have enough credits
    if(credits != static_cast<word_t>(-1)) {
        if(size + HEADER_SIZE > credits) {
            LOG(DTUERR, "DMA-error: insufficient credits on chan " << chanid
                    << " (have #" << fmt(credits, "x") << ", need #" << fmt(size + HEADER_SIZE, "x")
                    << ")." << " Ignoring send-command");
            return CTRL_ERROR;
        }
        set_ep(chanid, EP_CREDITS, credits - (size + HEADER_SIZE));
    }

    dstcore = get_ep(chanid, EP_COREID);
    dstchan = get_ep(chanid, EP_CHANID);
    _buf.credits = 0;
    _buf.label = get_ep(chanid, EP_LABEL);

    _buf.length = size;
    memcpy(_buf.data, src, size);
    return 0;
}

int DTU::prepare_read(int chanid,int &dstcore,int &dstchan) {
    dstcore = get_ep(chanid, EP_COREID);
    dstchan = get_ep(chanid, EP_CHANID);

    _buf.credits = 0;
    _buf.label = get_ep(chanid, EP_LABEL);
    _buf.length = sizeof(word_t) * 3;
    reinterpret_cast<word_t*>(_buf.data)[0] = get_cmd(CMD_OFFSET);
    reinterpret_cast<word_t*>(_buf.data)[1] = get_cmd(CMD_LENGTH);
    reinterpret_cast<word_t*>(_buf.data)[2] = get_cmd(CMD_ADDR);
    return 0;
}

int DTU::prepare_write(int chanid,int &dstcore,int &dstchan) {
    const void *src = reinterpret_cast<const void*>(get_cmd(CMD_ADDR));
    const size_t size = get_cmd(CMD_SIZE);
    dstcore = get_ep(chanid, EP_COREID);
    dstchan = get_ep(chanid, EP_CHANID);

    _buf.credits = 0;
    _buf.label = get_ep(chanid, EP_LABEL);
    _buf.length = sizeof(word_t) * 2;
    reinterpret_cast<word_t*>(_buf.data)[0] = get_cmd(CMD_OFFSET);
    reinterpret_cast<word_t*>(_buf.data)[1] = get_cmd(CMD_LENGTH);
    memcpy(_buf.data + _buf.length, src, size);
    _buf.length += size;
    return 0;
}

int DTU::prepare_cmpxchg(int chanid,int &dstcore,int &dstchan) {
    const void *src = reinterpret_cast<const void*>(get_cmd(CMD_ADDR));
    const size_t size = get_cmd(CMD_SIZE);
    dstcore = get_ep(chanid, EP_COREID);
    dstchan = get_ep(chanid, EP_CHANID);

    if(size != get_cmd(CMD_LENGTH) * 2) {
        LOG(DTUERR, "DMA-error: cmpxchg: CMD_SIZE != CMD_LENGTH * 2. Ignoring send-command");
        return CTRL_ERROR;
    }

    _buf.credits = 0;
    _buf.label = get_ep(chanid, EP_LABEL);
    _buf.length = sizeof(word_t) * 3;
    reinterpret_cast<word_t*>(_buf.data)[0] = get_cmd(CMD_OFFSET);
    reinterpret_cast<word_t*>(_buf.data)[1] = get_cmd(CMD_LENGTH);
    reinterpret_cast<word_t*>(_buf.data)[2] = get_cmd(CMD_ADDR);
    memcpy(_buf.data + _buf.length, src, size);
    _buf.length += size;
    return 0;
}

int DTU::prepare_sendcrd(int chanid, int &dstcore, int &dstchan) {
    const size_t size = get_cmd(CMD_SIZE);
    const int crdchan = get_cmd(CMD_OFFSET);

    dstcore = get_ep(chanid, EP_COREID);
    dstchan = get_ep(chanid, EP_CHANID);
    _buf.credits = size + HEADER_SIZE;
    _buf.length = 1;    // can't be 0
    _buf.crd_chan = crdchan;
    return 0;
}

int DTU::prepare_ackmsg(int chanid) {
    word_t flags = get_ep(chanid, EP_BUF_FLAGS);
    size_t roff = get_ep(chanid, EP_BUF_ROFF);

    // increase read offset
    if(~flags & FLAG_NO_RINGBUF) {
        size_t ord = get_ep(chanid, EP_BUF_ORDER);
        size_t msgord = get_ep(chanid, EP_BUF_MSGORDER);
        roff = (roff + (1UL << msgord)) & ((1UL << (ord + 1)) - 1);
        set_ep(chanid, EP_BUF_ROFF, roff);
    }

    // decrease message count
    word_t msgs = get_ep(chanid, EP_BUF_MSGCNT);
    if(msgs == 0) {
        LOG(DTUERR, "DMA-error: Unable to ack message: message count in EP" << chanid << " is 0");
        return CTRL_ERROR;
    }
    msgs--;
    set_ep(chanid, EP_BUF_MSGCNT, msgs);

    LOG(DTU, "EP" << chanid << ": acked message"
        << " (msgcnt=" << msgs << ", roff=#" << fmt(roff, "x") << ")");
    return 0;
}

void DTU::handle_command(int core) {
    word_t newctrl = 0;
    int dstcoreid, dstchanid;

    // clear error
    set_cmd(CMD_CTRL, get_cmd(CMD_CTRL) & ~CTRL_ERROR);

    // get regs
    const int chanid = get_cmd(CMD_CHANID);
    const int reply_chanid = get_cmd(CMD_REPLY_CHANID);
    const word_t ctrl = get_cmd(CMD_CTRL);
    int op = (ctrl >> 3) & 0x7;
    if(chanid >= CHAN_COUNT) {
        LOG(DTUERR, "DMA-error: invalid chan-id (" << chanid << ")");
        newctrl |= CTRL_ERROR;
        goto error;
    }

    newctrl |= check_cmd(chanid, op, get_ep(chanid, EP_LABEL), get_ep(chanid, EP_CREDITS),
        get_cmd(CMD_OFFSET), get_cmd(CMD_LENGTH));
    switch(op) {
        case REPLY:
            newctrl |= prepare_reply(chanid, dstcoreid, dstchanid);
            break;
        case SEND:
            newctrl |= prepare_send(chanid, dstcoreid, dstchanid);
            break;
        case READ:
            newctrl |= prepare_read(chanid, dstcoreid, dstchanid);
            break;
        case WRITE:
            newctrl |= prepare_write(chanid, dstcoreid, dstchanid);
            break;
        case CMPXCHG:
            newctrl |= prepare_cmpxchg(chanid, dstcoreid, dstchanid);
            break;
        case SENDCRD:
            newctrl |= prepare_sendcrd(chanid, dstcoreid, dstchanid);
            break;
        case ACKMSG:
            newctrl |= prepare_ackmsg(chanid);
            set_cmd(CMD_CTRL, newctrl);
            return;
    }
    if(newctrl & CTRL_ERROR)
        goto error;

    // prepare message (add length and label)
    _buf.opcode = op;
    if(ctrl & CTRL_DEL_REPLY_CAP) {
        _buf.has_replycap = 1;
        _buf.core = core;
        _buf.snd_chanid = chanid;
        _buf.rpl_chanid = reply_chanid;
        _buf.replylabel = get_cmd(CMD_REPLYLBL);
    }
    else
        _buf.has_replycap = 0;

    send_msg(chanid, dstcoreid, dstchanid, op == REPLY);

error:
    set_cmd(CMD_CTRL, newctrl);
}

void DTU::send_msg(int chanid, int dstcoreid, int dstchanid, bool isreply) {
    LOG(DTU, (isreply ? ">> " : "-> ") << fmt(_buf.length, 3) << "b"
            << " lbl=" << fmt(_buf.label, "#0x", sizeof(label_t) * 2)
            << " over " << chanid << " to c:ch=" << dstcoreid << ":" << dstchanid
            << " (crd=#" << fmt((long)get_ep(dstchanid, EP_CREDITS), "x") << ")");

    _backend->send(dstcoreid, dstchanid, &_buf);
}

void DTU::handle_read_cmd(int chanid) {
    word_t base = _buf.label & ~MemGate::RWX;
    word_t offset = base + reinterpret_cast<word_t*>(_buf.data)[0];
    word_t length = reinterpret_cast<word_t*>(_buf.data)[1];
    word_t dest = reinterpret_cast<word_t*>(_buf.data)[2];
    LOG(DTU, "(read) " << length << " bytes from #" << fmt(base, "x")
            << "+#" << fmt(offset - base, "x") << " -> " << fmt(dest, "p"));
    int dstcoreid = _buf.core;
    int dstchanid = _buf.rpl_chanid;
    assert(length <= sizeof(_buf.data));

    _buf.opcode = RESP;
    _buf.credits = 0;
    _buf.label = 0;
    _buf.length = sizeof(word_t) * 3;
    reinterpret_cast<word_t*>(_buf.data)[0] = dest;
    reinterpret_cast<word_t*>(_buf.data)[1] = length;
    reinterpret_cast<word_t*>(_buf.data)[2] = 0;
    memcpy(_buf.data + _buf.length, reinterpret_cast<void*>(offset), length);
    _buf.length += length;
    send_msg(chanid, dstcoreid, dstchanid, true);
}

void DTU::handle_write_cmd(int) {
    word_t base = _buf.label & ~MemGate::RWX;
    word_t offset = base + reinterpret_cast<word_t*>(_buf.data)[0];
    word_t length = reinterpret_cast<word_t*>(_buf.data)[1];
    LOG(DTU, "(write) " << length << " bytes to #" << fmt(base, "x")
            << "+#" << fmt(offset - base, "x"));
    assert(length <= sizeof(_buf.data));
    memcpy(reinterpret_cast<void*>(offset), _buf.data + sizeof(word_t) * 2, length);
}

void DTU::handle_resp_cmd() {
    word_t base = _buf.label & ~MemGate::RWX;
    word_t offset = base + reinterpret_cast<word_t*>(_buf.data)[0];
    word_t length = reinterpret_cast<word_t*>(_buf.data)[1];
    word_t resp = reinterpret_cast<word_t*>(_buf.data)[2];
    LOG(DTU, "(resp) " << length << " bytes to #" << fmt(base, "x")
            << "+#" << fmt(offset - base, "x") << " -> " << resp);
    assert(length <= sizeof(_buf.data));
    memcpy(reinterpret_cast<void*>(offset), _buf.data + sizeof(word_t) * 3, length);
    /* provide feedback to SW */
    set_cmd(CMD_CTRL, get_cmd(CMD_CTRL) | resp);
    set_cmd(CMD_SIZE, 0);
}

void DTU::handle_cmpxchg_cmd(int chanid) {
    word_t base = _buf.label & ~MemGate::RWX;
    word_t offset = base + reinterpret_cast<word_t*>(_buf.data)[0];
    word_t length = reinterpret_cast<word_t*>(_buf.data)[1];
    LOG(DTU, "(cmpxchg) " << length << " bytes @ #" << fmt(base, "x")
            << "+#" << fmt(offset - base, "x"));
    int dstcoreid = _buf.core;
    int dstchanid = _buf.rpl_chanid;

    // do the compare exchange; no need to lock anything or so because our DTU is single-threaded
    word_t res;
    if(memcmp(reinterpret_cast<void*>(offset), _buf.data + sizeof(word_t) * 3, length) == 0) {
        memcpy(reinterpret_cast<void*>(offset), _buf.data + sizeof(word_t) * 3 + length, length);
        res = 0;
    }
    else {
        uint8_t *expected = reinterpret_cast<uint8_t*>(_buf.data) + sizeof(word_t) * 3;
        uint8_t *actual = reinterpret_cast<uint8_t*>(offset);
        LOG(DTUERR, "(cmpxchg) failed; expected:");
        dumpBytes(expected, length);
        LOG(DTUERR, "actual:");
        dumpBytes(actual, length);
        res = CTRL_ERROR;
    }

    // use the write command to send the data back to the desired location
    _buf.opcode = RESP;
    _buf.credits = 0;
    _buf.label = 0;
    _buf.length = sizeof(word_t) * 3;
    reinterpret_cast<word_t*>(_buf.data)[0] = 0;
    reinterpret_cast<word_t*>(_buf.data)[1] = 0;
    reinterpret_cast<word_t*>(_buf.data)[2] = res;
    send_msg(chanid, dstcoreid, dstchanid, true);
}

void DTU::handle_receive(int i) {
    const size_t size = 1UL << get_ep(i, EP_BUF_ORDER);
    const size_t roffraw = get_ep(i, EP_BUF_ROFF);
    size_t woffraw = get_ep(i, EP_BUF_WOFF);
    size_t roff = roffraw & (size - 1);
    size_t woff = woffraw & (size - 1);
    const size_t maxmsgord = get_ep(i, EP_BUF_MSGORDER);
    const size_t maxmsgsize = 1UL << maxmsgord;
    const word_t flags = get_ep(i, EP_BUF_FLAGS);

    // without ringbuffer, we can always overwrite the whole buffer
    size_t avail = size;
    if(~flags & FLAG_NO_RINGBUF) {
        // completely full?
        if(woffraw == (roffraw ^ size))
            avail = 0;
        else if(woff >= roff)
            avail = (size - woff) + roff;
        else
            avail = roff - woff;
        // with header, it can't be more than maxmsgsize
        if(~flags & FLAG_NO_HEADER)
            avail = std::min<size_t>(avail, maxmsgsize);
    }
    // if we don't store the header, we can receive more
    if(flags & FLAG_NO_HEADER)
        avail += HEADER_SIZE;

    ssize_t res = _backend->recv(i, &_buf);
    if(res == -1)
        return;
    const int op = _buf.opcode;
    const bool store = (~flags & FLAG_NO_RINGBUF) || op == SEND;

    if(store && (size_t)res > avail) {
        if((~flags & FLAG_NO_HEADER) || avail - HEADER_SIZE == 0) {
            LOG(DTUERR, "DMA-error: dropping message because space is not sufficient"
                    << " (required: " << res << ", available: " << avail << ")");
            return;
        }
        LOG(DTUERR, "DMA-warning: cropping message from " << res << " to " << avail << " bytes");
        res = avail;
    }

    char *const addr = reinterpret_cast<char*>(get_ep(i, EP_BUF_ADDR));
    const size_t msgsize = (flags & FLAG_NO_HEADER) ? res - HEADER_SIZE : res;
    const char *src = (flags & FLAG_NO_HEADER) ? _buf.data : (char*)&_buf;

    if(store && (~flags & FLAG_NO_HEADER) && msgsize > maxmsgsize) {
        LOG(DTUERR, "DMA-error: message too large (" << msgsize << " vs. " << maxmsgsize << ")");
        return;
    }

    // put message into receive buffer
    if((op != SEND && op != REPLY) || (flags & FLAG_NO_RINGBUF)) {
        switch(op) {
            case READ:
                handle_read_cmd(i);
                break;
            case RESP:
                handle_resp_cmd();
                break;
            case WRITE:
                handle_write_cmd(i);
                break;
            case CMPXCHG:
                handle_cmpxchg_cmd(i);
                break;
            case SEND:
                memcpy(addr, src, msgsize);
                break;
        }
    }
    else if(flags & FLAG_NO_HEADER) {
        size_t cpysize = msgsize;
        // starting writing til the end, if possible
        if(woff >= roff) {
            size_t rem = std::min<size_t>(cpysize, size - woff);
            memcpy(addr + woff, src, rem);
            if(cpysize > rem) {
                src += rem;
                cpysize -= rem;
                woff = 0;
            }
        }
        // continue at the beginning
        if(cpysize)
            memcpy(addr + woff, src, cpysize);
        set_ep(i, EP_BUF_WOFF, (woffraw + msgsize) & ((size << 1) - 1));
    }
    else {
        memcpy(addr + woff, src, msgsize);
        set_ep(i, EP_BUF_WOFF, (woffraw + maxmsgsize) & ((size << 1) - 1));
    }

    if(op != SENDCRD)
        set_ep(i, EP_BUF_MSGCNT, get_ep(i, EP_BUF_MSGCNT) + 1);

    // refill credits
    if(_buf.crd_chan >= CHAN_COUNT)
        LOG(DTUERR, "DMA-error: should give credits to channel " << _buf.crd_chan);
    else {
        word_t credits = get_ep(_buf.crd_chan, EP_CREDITS);
        if(_buf.credits && credits != static_cast<word_t>(-1)) {
            LOG(DTU, "Refilling credits of chan " << _buf.crd_chan
                << " from #" << fmt(credits, "x") << " to #" << fmt(credits + _buf.credits, "x"));
            set_ep(_buf.crd_chan, EP_CREDITS, credits + _buf.credits);
        }
    }

    if(store && op != SENDCRD) {
        LOG(DTU, "<- " << fmt(res - HEADER_SIZE, 3)
                << "b lbl=" << fmt(_buf.label, "#0x", sizeof(label_t) * 2)
                << " ch=" << i
                << " (" << "roff=#" << fmt(roff, "x") << ",woff=#"
                << fmt(get_ep(i, EP_BUF_WOFF) & (size - 1), "x") << ",cnt=#"
                << fmt(get_ep(i, EP_BUF_MSGCNT), "x") << ",crd=#"
                << fmt((long)get_ep(i, EP_CREDITS), "x")
                << ")");
    }
}

void *DTU::thread(void *arg) {
    DTU *dma = static_cast<DTU*>(arg);
    int core = coreid();

    // don't allow any interrupts here
    HWInterrupts::Guard noints;
    while(dma->_run) {
        // should we send something?
        if(dma->get_cmd(CMD_CTRL) & CTRL_START)
            dma->handle_command(core);

        // have we received a message?
        for(int i = 0; i < CHAN_COUNT; ++i)
            dma->handle_receive(i);

        dma->wait();
    }

    if(Config::get().is_kernel())
        dma->_backend->destroy();
    delete dma->_backend;
    return 0;
}

}
