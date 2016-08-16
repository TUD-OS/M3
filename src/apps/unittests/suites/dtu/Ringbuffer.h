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

#pragma once

#include <test/TestSuite.h>

#include "BaseTestCase.h"

class RingbufferTestSuite : public test::TestSuite {
private:
    class SendAckTestCase : public BaseTestCase {
    public:
        explicit SendAckTestCase() : BaseTestCase("Send ack") {
        }
        virtual void run() override;
    };

    class IterationTestCase : public BaseTestCase {
    public:
        explicit IterationTestCase() : BaseTestCase("Iteration") {
        }
        virtual void run() override;
    };

    class NoHeaderTestCase : public BaseTestCase {
    public:
        explicit NoHeaderTestCase() : BaseTestCase("No header") {
        }
        virtual void run() override;

    private:
        void check(word_t *addr, word_t w1, word_t w2, word_t w3, word_t w4) {
            assert_word(addr[0], static_cast<word_t>(w1));
            assert_word(addr[1], static_cast<word_t>(w2));
            assert_word(addr[2], static_cast<word_t>(w3));
            assert_word(addr[3], static_cast<word_t>(w4));
        }
    };

    class NoRingNoHeaderTestCase : public BaseTestCase {
    public:
        explicit NoRingNoHeaderTestCase() : BaseTestCase("No ringbuffer & no header") {
        }
        virtual void run() override;
    };

public:
    explicit RingbufferTestSuite()
        : TestSuite("Ringbuffer") {
        add(new SendAckTestCase());
        add(new IterationTestCase());
        add(new NoHeaderTestCase());
        add(new NoRingNoHeaderTestCase());
    }
};
