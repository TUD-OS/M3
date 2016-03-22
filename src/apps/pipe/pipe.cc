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

#include <base/stream/Serial.h>
#include <base/stream/IStringStream.h>
#include <base/Log.h>

#include <m3/stream/FStream.h>
#include <m3/stream/Standard.h>
#include <m3/pipe/Pipe.h>

using namespace m3;

alignas(DTU_PKG_SIZE) static char buffer[0x100];

int main() {
    {
        VPE writer("writer");
        Pipe pipe(VPE::self(), writer, 0x1000);

        writer.fds()->set(STDOUT_FILENO, VPE::self().fds()->get(pipe.writer_fd()));
        writer.obtain_fds();

        writer.run([] {
            File *out = VPE::self().fds()->get(STDOUT_FILENO);
            for(int i = 0; i < 10; ++i) {
                OStringStream os(buffer, sizeof(buffer));
                os << "Hello World from child " << i << "!";
                out->write(buffer, strlen(buffer) + 1);
            }
            return 0;
        });

        pipe.close_writer();

        File *in = VPE::self().fds()->get(pipe.reader_fd());
        size_t res, i = 0;
        while(i++ < 3 && (res = in->read(buffer, sizeof(buffer))) > 0)
            Serial::get() << "Read " << res << ": '" << buffer << "'\n";

        pipe.close_reader();
        writer.wait();
    }

    {
        VPE reader("reader");
        VPE writer("writer");
        Pipe pipe(reader, writer, 0x1000);

        reader.fds()->set(STDIN_FILENO, VPE::self().fds()->get(pipe.reader_fd()));
        reader.obtain_fds();

        reader.run([] {
            File *in = VPE::self().fds()->get(STDIN_FILENO);
            size_t res, i = 0;
            while(i++ < 3 && (res = in->read(buffer, sizeof(buffer))) > 0)
                Serial::get() << "Read " << res << ": '" << buffer << "'\n";
            return 0;
        });

        writer.fds()->set(STDOUT_FILENO, VPE::self().fds()->get(pipe.writer_fd()));
        writer.obtain_fds();

        writer.run([] {
            File *out = VPE::self().fds()->get(STDOUT_FILENO);
            for(int i = 0; i < 10; ++i) {
                OStringStream os(buffer, sizeof(buffer));
                os << "Hello World from sibling " << i << "!";
                out->write(buffer, strlen(buffer) + 1);
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
        Pipe p1(t2, t1, 0x1000);

        t1.fds()->set(STDOUT_FILENO, VPE::self().fds()->get(p1.writer_fd()));
        t1.obtain_fds();

        t1.run([] {
            File *out = VPE::self().fds()->get(STDOUT_FILENO);
            for(int i = 0; i < 10; ++i) {
                OStringStream os(buffer, sizeof(buffer));
                os << "Hello World from child " << i << "!";
                out->write(buffer, strlen(buffer) + 1);
            }
            return 0;
        });

        // t2 -> self
        Pipe p2(VPE::self(), t2, 0x1000);

        t2.fds()->set(STDIN_FILENO, VPE::self().fds()->get(p1.reader_fd()));
        t2.fds()->set(STDOUT_FILENO, VPE::self().fds()->get(p2.writer_fd()));
        t2.obtain_fds();

        t2.run([] {
            File *in = VPE::self().fds()->get(STDIN_FILENO);
            File *out = VPE::self().fds()->get(STDOUT_FILENO);
            size_t res;
            while((res = in->read(buffer, sizeof(buffer))) > 0) {
                Serial::get() << "Received " << res << "b: '" << buffer << "'\n";
                out->write(buffer, res);
            }
            return 0;
        });

        p1.close_writer();
        p1.close_reader();

        p2.close_writer();

        File *in = VPE::self().fds()->get(p2.reader_fd());
        size_t res;
        while((res = in->read(buffer, sizeof(buffer))) > 0)
            Serial::get() << "Received " << res << "b: '" << buffer << "'\n";

        p2.close_reader();

        t1.wait();
        t2.wait();
    }

    {
        VPE reader("reader");

        Pipe pipe(reader, VPE::self(), 64);

        reader.fds()->set(STDIN_FILENO, VPE::self().fds()->get(pipe.reader_fd()));
        reader.obtain_fds();

        reader.run([] {
            while(cin.good()) {
                String s;
                cin >> s;
                Serial::get() << "Read " << s.length() << ": '" << s << "'\n";
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
