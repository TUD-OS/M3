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

#include <m3/Config.h>
#include <m3/RecvBuf.h>
#include "Ringbuffer.h"

using namespace m3;

void RingbufferTestSuite::SendAckTestCase::run() {
    const size_t sendchanid = 3;
    const size_t chanid = 4;
    word_t data = 1234;
    DTU &dtu = DTU::get();
    RecvBuf buf = RecvBuf::create(chanid, nextlog2<128>::val, nextlog2<64>::val, 0);
    label_t lbl = 0xDEADBEEF;
    dtu.configure(sendchanid, lbl, coreid(), buf.chanid(), -1);

    {
        dmasend(&data, sizeof(data), sendchanid);
        DTU::Message *msg = getmsgat(buf.chanid(), 1, 0);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_ROFF), (1UL << buf.msgorder()) * 0);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_WOFF), (1UL << buf.msgorder()) * 1);
        assert_true(msg->label == lbl);
        assert_size(msg->length, sizeof(data));
        dtu.ack_message(buf.chanid());
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_ROFF), (1UL << buf.msgorder()) * 1);
    }

    {
        dmasend(&data, sizeof(data), sendchanid);
        DTU::Message *msg = getmsgat(buf.chanid(), 1, 1);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_ROFF), (1UL << buf.msgorder()) * 1);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_WOFF), (1UL << buf.msgorder()) * 2);
        assert_true(msg->label == lbl);
        assert_size(msg->length, sizeof(data));
        dtu.ack_message(buf.chanid());
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_ROFF), (1UL << buf.msgorder()) * 2);
    }

    dtu.configure(sendchanid, 0, 0, 0, 0);
}

void RingbufferTestSuite::IterationTestCase::run() {
    const size_t sendchanid = 3;
    const size_t chanid = 4;
    word_t data = 1234;
    DTU &dtu = DTU::get();
    RecvBuf buf = RecvBuf::create(chanid, nextlog2<128>::val, nextlog2<64>::val, 0);
    label_t lbl = 0xDEADBEEF;
    dtu.configure(sendchanid, lbl, coreid(), buf.chanid(), -1);

    {
        dmasend(&data, sizeof(data), sendchanid);
        DTU::Message *msg = getmsgat(buf.chanid(), 1, 0);
        assert_true(msg->label == lbl);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_ROFF), (1UL << buf.msgorder()) * 0);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_WOFF), (1UL << buf.msgorder()) * 1);
    }

    {
        dmasend(&data, sizeof(data), sendchanid);
        DTU::Message *msg = getmsgat(buf.chanid(), 2, 1);
        assert_true(msg->label == lbl);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_ROFF), (1UL << buf.msgorder()) * 0);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_WOFF), (1UL << buf.msgorder()) * 2);
    }

    dtu.ack_message(buf.chanid());
    dtu.ack_message(buf.chanid());
    assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_ROFF), (1UL << buf.msgorder()) * 2);
    assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_WOFF), (1UL << buf.msgorder()) * 2);

    {
        dmasend(&data, sizeof(data), sendchanid);
        DTU::Message *msg = getmsgat(buf.chanid(), 1, 0);
        assert_true(msg->label == lbl);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_ROFF), (1UL << buf.msgorder()) * 2);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_WOFF), (1UL << buf.msgorder()) * 3);
    }

    {
        dmasend(&data, sizeof(data), sendchanid);
        DTU::Message *msg = getmsgat(buf.chanid(), 2, 1);
        assert_true(msg->label == lbl);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_ROFF), (1UL << buf.msgorder()) * 2);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_WOFF), (1UL << buf.msgorder()) * 0);
    }

    {
        data = 5678;
        dmasend(&data, sizeof(data), sendchanid);
        data = 1234;
    }

    dtu.ack_message(buf.chanid());
    dtu.ack_message(buf.chanid());
    assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_ROFF), (1UL << buf.msgorder()) * 0);
    assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_WOFF), (1UL << buf.msgorder()) * 0);

    {
        dmasend(&data, sizeof(data), sendchanid);
        DTU::Message *msg = getmsgat(buf.chanid(), 1, 0);
        word_t *dataptr = reinterpret_cast<word_t*>(msg->data);
        assert_true(msg->label == lbl);
        assert_word(*dataptr, 1234);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_ROFF), (1UL << buf.msgorder()) * 0);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_WOFF), (1UL << buf.msgorder()) * 1);
    }

    dtu.configure(sendchanid, 0, 0, 0, 0);
}

void RingbufferTestSuite::NoHeaderTestCase::run() {
    const size_t sendchanid = 3;
    const size_t chanid = 4;
    word_t d[] = {0,1,2,3};
    DTU &dtu = DTU::get();
    RecvBuf buf = RecvBuf::create(chanid, nextlog2<sizeof(word_t) * 4>::val, RecvBuf::NO_HEADER);
    label_t lbl = 0xDEADBEEF;
    dtu.configure(sendchanid, lbl, coreid(), buf.chanid(), -1);
    word_t *ringbuf = reinterpret_cast<word_t*>(buf.addr());
    // we assume here that its initialized with zeros
    memset(ringbuf, 0, 1UL << buf.order());

    {
        dmasend(d, sizeof(word_t), sendchanid);
        dmasend(d + 1, sizeof(word_t) * 2, sendchanid);

        getmsg(buf.chanid(), 2);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_ROFF), sizeof(word_t) * 0);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_WOFF), sizeof(word_t) * 3);
        check(ringbuf, d[0], d[1], d[2], 0);

        dtu.set_ep(buf.chanid(), DTU::EP_BUF_ROFF, sizeof(word_t) * 2);

        dmasend(d + 3, sizeof(word_t), sendchanid);

        getmsg(buf.chanid(), 3);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_ROFF), sizeof(word_t) * 2);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_WOFF), sizeof(word_t) * 4);
        check(ringbuf, d[0], d[1], d[2], d[3]);

        dmasend(d + 1, sizeof(word_t) * 3, sendchanid);

        getmsg(buf.chanid(), 4);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_ROFF), sizeof(word_t) * 2);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_WOFF), sizeof(word_t) * 6);
        check(ringbuf, d[1], d[2], d[2], d[3]);

        dtu.set_ep(buf.chanid(), DTU::EP_BUF_ROFF, sizeof(word_t) * 5);

        dmasend(d, sizeof(word_t) * 3, sendchanid);

        getmsg(buf.chanid(), 5);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_ROFF), sizeof(word_t) * 5);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_WOFF), sizeof(word_t) * 1);
        check(ringbuf, d[2], d[2], d[0], d[1]);

        dtu.set_ep(buf.chanid(), DTU::EP_BUF_ROFF, sizeof(word_t) * 1);

        dmasend(d, sizeof(word_t) * 4, sendchanid);

        getmsg(buf.chanid(), 6);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_ROFF), sizeof(word_t) * 1);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_WOFF), sizeof(word_t) * 5);
        check(ringbuf, d[3], d[0], d[1], d[2]);

        dtu.set_ep(buf.chanid(), DTU::EP_BUF_ROFF, sizeof(word_t) * 2);

        dmasend(d + 1, sizeof(word_t) * 16, sendchanid);

        getmsg(buf.chanid(), 7);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_ROFF), sizeof(word_t) * 2);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_WOFF), sizeof(word_t) * 6);
        check(ringbuf, d[3], d[1], d[1], d[2]);
    }

    dtu.configure(sendchanid, 0, 0, 0, 0);
}

void RingbufferTestSuite::NoRingNoHeaderTestCase::run() {
    const size_t sendchanid = 3;
    const size_t chanid = 4;
    word_t data[] = {1234, 5678};
    DTU &dtu = DTU::get();
    RecvBuf buf = RecvBuf::create(chanid, nextlog2<64>::val,
            RecvBuf::NO_HEADER | RecvBuf::NO_RINGBUF);
    word_t *ringbuf = reinterpret_cast<word_t*>(buf.addr());
    label_t lbl = 0xDEADBEEF;
    dtu.configure(sendchanid, lbl, coreid(), buf.chanid(), -1);

    {
        dmasend(data, sizeof(data), sendchanid);
        getmsg(buf.chanid(), 1);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_ROFF), 0);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_WOFF), 0);
        assert_word(ringbuf[0], data[0]);
        assert_word(ringbuf[1], data[1]);
    }

    {
        dmasend(data + 1, sizeof(data[0]), sendchanid);
        getmsg(buf.chanid(), 2);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_ROFF), 0);
        assert_word(dtu.get_ep(buf.chanid(), DTU::EP_BUF_WOFF), 0);
        assert_word(ringbuf[0], data[1]);
        assert_word(ringbuf[1], data[1]);
    }

    dtu.configure(sendchanid, 0, 0, 0, 0);
}
