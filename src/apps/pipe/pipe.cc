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

#include <m3/stream/Serial.h>
#include <m3/stream/IStringStream.h>
#include <m3/stream/FStream.h>
#include <m3/vfs/Pipe.h>
#include <m3/pipe/PipeFS.h>
#include <m3/Log.h>

using namespace m3;

alignas(DTU_PKG_SIZE) static char buffer[0x100];

int main() {
    VFS::mount("/pipes", new PipeFS());

    {
        VPE reader("reader");
        Pipe pipe(reader, VPE::self(), 64);

        String rpath = pipe.get_path('r', "/pipes/");
        reader.run([rpath] {
            FStream f(rpath.c_str(), FILE_R);
            if(!f)
                PANIC("Unable to open " << rpath);
            while(f.good()) {
                String s;
                f >> s;
                Serial::get() << "Read " << s.length() << ": '" << s << "'\n";
            }
            return 0;
        });

        {
            String wpath = pipe.get_path('w', "/pipes/");
            FStream f(wpath.c_str(), FILE_W);
            if(!f)
                PANIC("Unable to open " << wpath);
            for(int i = 0; i < 10; ++i)
                f << "Hello World from parent " << i << "!!!\n";
        }
        reader.wait();
    }

    {
        VPE writer("writer");
        Pipe pipe(VPE::self(), writer, 0x1000);

        writer.run([&pipe] {
            PipeWriter wr(pipe);
            for(int i = 0; i < 10; ++i) {
                OStringStream os(buffer, sizeof(buffer));
                os << "Hello World from child " << i << "!";
                wr.write(buffer, Math::round_up(strlen(buffer) + 1, DTU_PKG_SIZE));
            }
            return 0;
        });

        {
            PipeReader rd(pipe);
            size_t res, i = 0;
            while(i++ < 3 && (res = rd.read(buffer, sizeof(buffer))) > 0)
                Serial::get() << "Read " << res << ": '" << buffer << "'\n";
        }
        writer.wait();
    }

    {
        VPE reader("reader");
        VPE writer("writer");
        Pipe pipe(reader, writer, 0x1000);

        reader.run([&pipe] {
            PipeReader rd(pipe);
            size_t res, i = 0;
            while(i++ < 3 && (res = rd.read(buffer, sizeof(buffer))) > 0)
                Serial::get() << "Read " << res << ": '" << buffer << "'\n";
            return 0;
        });
        writer.run([&pipe] {
            PipeWriter wr(pipe);
            for(int i = 0; i < 10; ++i) {
                OStringStream os(buffer, sizeof(buffer));
                os << "Hello World from sibling " << i << "!";
                wr.write(buffer, Math::round_up(strlen(buffer) + 1, DTU_PKG_SIZE));
            }
            return 0;
        });
        reader.wait();
        writer.wait();
    }

    // does not work on T2 because we need at least a capacity of 2 in each recvbuf
#if !defined(__t2__)
    {
        VPE reader("reader");
        VPE writer1("writer1");
        VPE writer2("writer2");

        Pipe pipe1(reader, writer1, 0x1000);
        Pipe pipe2(reader, writer2, 0x1000);

        writer1.run([&pipe1] {
            PipeWriter wr(pipe1);
            for(int i = 0; i < 10; ++i) {
                OStringStream os(buffer, sizeof(buffer));
                os << "Hello World from VPE " << coreid() << "!";
                wr.write(buffer, Math::round_up(strlen(buffer) + 1, DTU_PKG_SIZE));
            }
            return 0;
        });
        writer2.run([&pipe2] {
            PipeWriter wr(pipe2);
            for(int i = 0; i < 10; ++i) {
                OStringStream os(buffer, sizeof(buffer));
                os << "Hello World from VPE " << coreid() << "!";
                wr.write(buffer, Math::round_up(strlen(buffer) + 1, DTU_PKG_SIZE));
            }
            return 0;
        });

        reader.run([&pipe1, &pipe2] {
            PipeReader rd1(pipe1);
            PipeReader rd2(pipe2);
            while(!rd1.eof() || !rd2.eof()) {
                size_t res = 0;
                if(!rd1.eof() && rd1.has_data())
                    res = rd1.read(buffer, sizeof(buffer));
                else if(!rd2.eof() && rd2.has_data())
                    res = rd2.read(buffer, sizeof(buffer));
                if(res)
                    Serial::get() << "Read " << res << ": '" << buffer << "'\n";
            }
            return 0;
        });

        writer1.wait();
        writer2.wait();
        reader.wait();
    }
#endif

    {
        VPE t1("t1");
        VPE t2("t2");

        // t1 -> t2
        Pipe p1(t2, t1, 0x1000);

        t1.run([&p1] {
            PipeWriter wr(p1);
            for(int i = 0; i < 10; ++i) {
                OStringStream os(buffer, sizeof(buffer));
                os << "Hello World from child " << i << "!";
                wr.write(buffer, Math::round_up(strlen(buffer) + 1, DTU_PKG_SIZE));
            }
            return 0;
        });

        // t2 -> self
        Pipe p2(VPE::self(), t2, 0x1000);

        t2.run([&p1, &p2] {
            PipeReader rd(p1);
            PipeWriter wr(p2);
            while(!rd.eof()) {
                size_t res = rd.read(buffer, sizeof(buffer));
                if(res) {
                    Serial::get() << "Received " << res << "b: '" << buffer << "'\n";
                    wr.write(buffer, res);
                }
            }
            return 0;
        });

        {
            PipeReader rd(p2);
            while(!rd.eof()) {
                size_t res = rd.read(buffer, sizeof(buffer));
                if(res)
                    Serial::get() << "Received " << res << "b: '" << buffer << "'\n";
            }
        }
        t1.wait();
        t2.wait();
    }
    return 0;
}
