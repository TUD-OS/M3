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

class FSTestSuite : public test::TestSuite {
private:
    class DirTestCase : public test::TestCase {
    public:
        explicit DirTestCase() : test::TestCase("Directories") {
        }
        virtual void run() override;
    };
    class FileTestCase : public test::TestCase {
    public:
        explicit FileTestCase() : test::TestCase("Files") {
        }
        virtual void run() override;
    };
    class BufferedFileTestCase : public test::TestCase {
    public:
        explicit BufferedFileTestCase() : test::TestCase("Buffered files") {
        }
        virtual void run() override;
    };
    class WriteFileTestCase : public test::TestCase {
    public:
        explicit WriteFileTestCase() : test::TestCase("Writing files") {
        }
        virtual void run() override;
    private:
        void check_content(const char *filename, size_t size);
    };
    class MetaFileTestCase : public test::TestCase {
    public:
        explicit MetaFileTestCase() : test::TestCase("Meta operations") {
        }
        virtual void run() override;
    private:
        void check_content(const char *filename, size_t size);
    };

public:
    explicit FSTestSuite()
        : TestSuite("FS") {
        add(new DirTestCase());
        add(new FileTestCase());
        add(new BufferedFileTestCase());
        add(new WriteFileTestCase());
        add(new MetaFileTestCase());
    }
};
