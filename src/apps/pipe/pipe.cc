/*
 * Copyright (C) 2015-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/stream/IStringStream.h>

#include <m3/stream/FStream.h>
#include <m3/stream/Standard.h>
#include <m3/pipe/DirectPipe.h>
#include <m3/pipe/IndirectPipe.h>
#include <m3/vfs/VFS.h>

using namespace m3;

alignas(DTU_PKG_SIZE) static char buffer[0x100];

int main() {
    {
        MemGate mem = MemGate::create_global(0x1000, MemGate::RW);
        IndirectPipe pipe(mem, 0x1000);
        VPE reader("reader");
        VPE reader2("reader2");
        VPE writer("writer");

        reader.fds()->set(STDIN_FD, VPE::self().fds()->get(pipe.reader_fd()));
        reader.fds()->set(STDOUT_FD, VPE::self().fds()->get(STDOUT_FD));
        reader.fds()->set(STDERR_FD, VPE::self().fds()->get(STDERR_FD));
        reader.obtain_fds();

        reader.run([] {
            while(cin.getline(buffer, sizeof(buffer)) > 0)
                cout << "[1] Read '" << buffer << "'\n";
            return 0;
        });

        reader2.fds()->set(STDIN_FD, VPE::self().fds()->get(pipe.reader_fd()));
        reader2.fds()->set(STDOUT_FD, VPE::self().fds()->get(STDOUT_FD));
        reader2.fds()->set(STDERR_FD, VPE::self().fds()->get(STDERR_FD));
        reader2.obtain_fds();

        reader2.run([] {
            while(cin.getline(buffer, sizeof(buffer)) > 0)
                cout << "[2] Read '" << buffer << "'\n";
            return 0;
        });

        writer.fds()->set(STDIN_FD, VPE::self().fds()->get(STDIN_FD));
        writer.fds()->set(STDOUT_FD, VPE::self().fds()->get(pipe.writer_fd()));
        writer.fds()->set(STDERR_FD, VPE::self().fds()->get(STDERR_FD));
        writer.obtain_fds();

        writer.run([] {
            File *out = VPE::self().fds()->get(STDOUT_FD);
            for(int i = 0; i < 5; ++i) {
                OStringStream os(buffer, sizeof(buffer));
                os << "Hello World from sibling " << i << "!\n";
                if(out->write(buffer, os.length()) < 0)
                    break;
            }
            return 0;
        });

        pipe.close_reader();

        File *out = VPE::self().fds()->get(pipe.writer_fd());
        for(int i = 0; i < 10; ++i) {
            OStringStream os(buffer, sizeof(buffer));
            os << "Hello World from child " << i << "!\n";
            if(out->write(buffer, os.length()) < 0)
                break;
        }

        pipe.close_writer();

        reader.wait();
        reader2.wait();
        writer.wait();
    }

    {
        MemGate mem = MemGate::create_global(0x1000, MemGate::RW);
        IndirectPipe pipe(mem, 0x1000);
        VPE reader("reader");

        reader.fds()->set(STDIN_FD, VPE::self().fds()->get(pipe.reader_fd()));
        reader.fds()->set(STDOUT_FD, VPE::self().fds()->get(STDOUT_FD));
        reader.fds()->set(STDERR_FD, VPE::self().fds()->get(STDERR_FD));
        reader.obtain_fds();

        reader.run([] {
            size_t i = 0;
            while(i++ < 3 && cin.getline(buffer, sizeof(buffer)) > 0)
                cout << "Read '" << buffer << "'\n";
            return 0;
        });

        pipe.close_reader();

        File *out = VPE::self().fds()->get(pipe.writer_fd());
        for(int i = 0; i < 10; ++i) {
            OStringStream os(buffer, sizeof(buffer));
            os << "Hello World from child " << i << "!\n";
            if(out->write(buffer, os.length()) < 0)
                break;
        }

        pipe.close_writer();

        reader.wait();
    }

    {
        MemGate mem = MemGate::create_global(0x1000, MemGate::RW);
        IndirectPipe pipe(mem, 0x1000);
        VPE reader("reader");

        reader.fds()->set(STDIN_FD, VPE::self().fds()->get(pipe.reader_fd()));
        reader.fds()->set(STDOUT_FD, VPE::self().fds()->get(STDOUT_FD));
        reader.fds()->set(STDERR_FD, VPE::self().fds()->get(STDERR_FD));
        reader.obtain_fds();

        reader.run([] {
            File *in = VPE::self().fds()->get(STDIN_FD);
            while(in->read(buffer, sizeof(buffer)) > 0)
                ;
            return 0;
        });

        pipe.close_reader();

        File *out = VPE::self().fds()->get(pipe.writer_fd());
        for(int i = 0; i < 10; ++i) {
            if(out->write(buffer, sizeof(buffer)) < 0)
                break;
        }

        pipe.close_writer();

        reader.wait();
    }

    {
        VPE writer("writer");
        MemGate mem = MemGate::create_global(0x1000, MemGate::RW);
        DirectPipe pipe(VPE::self(), writer, mem, 0x1000);

        writer.fds()->set(STDIN_FD, VPE::self().fds()->get(STDIN_FD));
        writer.fds()->set(STDOUT_FD, VPE::self().fds()->get(pipe.writer_fd()));
        writer.obtain_fds();

        writer.run([] {
            File *out = VPE::self().fds()->get(STDOUT_FD);
            for(int i = 0; i < 10; ++i) {
                OStringStream os(buffer, sizeof(buffer));
                os << "Hello World from child " << i << "!\n";
                out->write(buffer, os.length());
            }
            return 0;
        });

        pipe.close_writer();

        {
            FStream in(pipe.reader_fd());
            size_t i = 0;
            while(i++ < 3 && in.getline(buffer, sizeof(buffer)) > 0)
                cout << "Read '" << buffer << "'\n";
        }

        pipe.close_reader();
        writer.wait();
    }

    {
        VPE reader("reader");
        VPE writer("writer");
        MemGate mem = MemGate::create_global(0x1000, MemGate::RW);
        DirectPipe pipe(reader, writer, mem, 0x1000);

        reader.fds()->set(STDIN_FD, VPE::self().fds()->get(pipe.reader_fd()));
        reader.fds()->set(STDOUT_FD, VPE::self().fds()->get(STDOUT_FD));
        reader.obtain_fds();

        reader.run([] {
            size_t i = 0;
            while(i++ < 3 && cin.getline(buffer, sizeof(buffer)) > 0)
                cout << "Read '" << buffer << "'\n";
            return 0;
        });

        writer.fds()->set(STDIN_FD, VPE::self().fds()->get(STDIN_FD));
        writer.fds()->set(STDOUT_FD, VPE::self().fds()->get(pipe.writer_fd()));
        writer.obtain_fds();

        writer.run([] {
            File *out = VPE::self().fds()->get(STDOUT_FD);
            for(int i = 0; i < 10; ++i) {
                OStringStream os(buffer, sizeof(buffer));
                os << "Hello World from sibling " << i << "!\n";
                out->write(buffer, os.length());
            }
            return 0;
        });

        pipe.close_writer();
        pipe.close_reader();

        reader.wait();
        writer.wait();
    }

    {
        VPE t1("t1");
        VPE t2("t2");

        // t1 -> t2
        MemGate mem1 = MemGate::create_global(0x1000, MemGate::RW);
        DirectPipe p1(t2, t1, mem1, 0x1000);

        t1.fds()->set(STDIN_FD, VPE::self().fds()->get(STDIN_FD));
        t1.fds()->set(STDOUT_FD, VPE::self().fds()->get(p1.writer_fd()));
        t1.obtain_fds();

        t1.run([] {
            File *out = VPE::self().fds()->get(STDOUT_FD);
            for(int i = 0; i < 10; ++i) {
                OStringStream os(buffer, sizeof(buffer));
                os << "Hello World from child " << i << "!\n";
                out->write(buffer, os.length());
            }
            return 0;
        });

        // t2 -> self
        MemGate mem2 = MemGate::create_global(0x1000, MemGate::RW);
        DirectPipe p2(VPE::self(), t2, mem2, 0x1000);

        t2.fds()->set(STDIN_FD, VPE::self().fds()->get(p1.reader_fd()));
        t2.fds()->set(STDOUT_FD, VPE::self().fds()->get(p2.writer_fd()));
        t2.obtain_fds();

        t2.run([] {
            File *in = VPE::self().fds()->get(STDIN_FD);
            File *out = VPE::self().fds()->get(STDOUT_FD);
            ssize_t res;
            while((res = in->read(buffer, sizeof(buffer))) > 0) {
                out->write(buffer, static_cast<size_t>(res));
            }
            return 0;
        });

        p1.close_writer();
        p1.close_reader();

        p2.close_writer();

        {
            FStream in(p2.reader_fd());
            while(in.getline(buffer, sizeof(buffer)) > 0)
                cout << "Read '" << buffer << "'\n";
        }

        p2.close_reader();

        t1.wait();
        t2.wait();
    }

    {
        VPE reader("reader");

        MemGate mem = MemGate::create_global(64, MemGate::RW);
        DirectPipe pipe(reader, VPE::self(), mem, 64);

        reader.fds()->set(STDIN_FD, VPE::self().fds()->get(pipe.reader_fd()));
        reader.fds()->set(STDOUT_FD, VPE::self().fds()->get(STDOUT_FD));
        reader.obtain_fds();

        reader.run([] {
            while(cin.good()) {
                String s;
                cin >> s;
                cout << "Read " << s.length() << ": '" << s << "'\n";
            }
            return 0;
        });

        pipe.close_reader();

        {
            FStream f(pipe.writer_fd(), FILE_W);
            for(int i = 0; i < 10; ++i)
                f << "Hello World from parent " << i << "!!!\n";
        }

        pipe.close_writer();
        reader.wait();
    }
    return 0;
}
