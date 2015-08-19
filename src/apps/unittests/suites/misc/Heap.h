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

#include <test/TestSuite.h>
#include <m3/Heap.h>

class HeapTestSuite : public test::TestSuite {
private:
    class BaseTestCase : public test::TestCase {
    public:
        explicit BaseTestCase(const m3::String &name) : TestCase(name) {
        }
        void check_heap_before() {
            heap_before = m3::Heap::free_memory();
        }
        void check_heap_after() {
            size_t after = m3::Heap::free_memory();
            assert_size(after, heap_before);
        }

    private:
        size_t heap_before;
    };

    class TestCase1 : public BaseTestCase {
    public:
        explicit TestCase1() : BaseTestCase("Allocate, then free in same direction") {
        }
        virtual void run() override;
    };
    class TestCase2 : public BaseTestCase {
    public:
        explicit TestCase2() : BaseTestCase("Allocate, then free in opposite direction") {
        }
        virtual void run() override;
    };
    class TestCase3 : public BaseTestCase {
    public:
        explicit TestCase3() : BaseTestCase("Allocate, then free in \"random\" direction 1") {
        }
        virtual void run() override;
    };
    class TestCase4 : public BaseTestCase {
    public:
        explicit TestCase4() : BaseTestCase("Allocate, then free in \"random\" direction 2") {
        }
        virtual void run() override;
    };
    class TestCase5 : public BaseTestCase {
    public:
        explicit TestCase5() : BaseTestCase("Allocate and free with different sizes") {
        }
        virtual void run() override;
    };
    class TestCase6 : public BaseTestCase {
    public:
        explicit TestCase6() : BaseTestCase("Allocate single bytes") {
        }
        virtual void run() override;
    };
    class TestCase7 : public BaseTestCase {
    public:
        explicit TestCase7() : BaseTestCase("Allocate 3 regions") {
        }
        virtual void run() override;
    };
    class TestCase8 : public BaseTestCase {
    public:
        explicit TestCase8() : BaseTestCase("Reallocate") {
        }
        virtual void run() override;
    };
    class TestCase9 : public BaseTestCase {
    public:
        explicit TestCase9() : BaseTestCase("Allocate all and free it again") {
        }
        virtual void run() override;
    };

public:
    explicit HeapTestSuite()
        : TestSuite("Heap") {
        add(new TestCase1());
        add(new TestCase2());
        add(new TestCase3());
        add(new TestCase4());
        add(new TestCase5());
        add(new TestCase6());
        add(new TestCase7());
        add(new TestCase8());
        add(new TestCase9());
    }
};
