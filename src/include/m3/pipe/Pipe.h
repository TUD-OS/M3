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

#include <m3/Common.h>
#include <m3/cap/MemGate.h>
#include <m3/cap/SendGate.h>
#include <m3/cap/VPE.h>
#include <m3/stream/OStringStream.h>
#include <m3/util/Math.h>

#define DEBUG_PIPE  0
#if DEBUG_PIPE
#   include <m3/stream/Serial.h>
#   define DBG_PIPE(expr)   Serial::get() << expr
#else
#   define DBG_PIPE(...)
#endif

namespace m3 {

/**
 * A uni-directional pipe between two VPEs. An object of this class holds the state of the pipe,
 * i.e. the memory capability and the gate capability for communication. That means that the object
 * should stay alive as long as the pipe communication takes place.
 *
 * To use the pipe, you need to create a read-end and write-end, realized by PipeReadEnd and
 * PipeWriteEnd. To actually read from a pipe, you need to create a PipeReader from the PipeReadEnd
 * object. The same goes for write.
 *
 * A usage example looks like the following:
 * <code>
 *   VPE reader("reader");
 *   Pipe pipe(reader, 0x1000);
 *   PipeReadEnd pre(pipe, reader);
 *   PipeWriteEnd pwe(pipe, VPE::self());
 *
 *   reader.run([pre] {
 *       PipeReader rd(pre);
 *       // read from pipe
 *       return 0;
 *   });
 *
 *   {
 *       PipeWriter wr(pwe);
 *       // write into pipe
 *   } // send EOF before waiting for reader
 *   reader.wait();
 * </code>
 */
class Pipe {
public:
    static const size_t MSG_SIZE        = 64;

#if defined(__t2__)
    // TODO on t2, we can't send multiple messages at once
    static const size_t MSG_BUF_SIZE    = MSG_SIZE;
#else
    static const size_t MSG_BUF_SIZE    = MSG_SIZE * 16;
#endif

#if defined(__t3__)
    // TODO since credits can't be given back on t3 currently, give "unlimited" credits
    static const size_t CREDITS         = 0xFFFF;
#else
    static const size_t CREDITS         = MSG_BUF_SIZE;
#endif

    enum {
        READ_EOF    = 1 << 0,
        WRITE_EOF   = 1 << 1,
    };

    /**
     * Creates a pipe with VPE <rd> as the reader and <wr> as the writer, using a shared memory
     * area of <size> bytes.
     *
     * @param rd the reader of the pipe
     * @param wr the writer of the pipe
     * @param size the size of the shared memory area
     */
    explicit Pipe(VPE &rd, VPE &wr, size_t size)
        : _rd(rd), _recvep(rd.alloc_ep()), _size(size),
          _mem(MemGate::create_global(size, MemGate::RW, VPE::self().alloc_caps(2))),
          _sgate(SendGate::create_for(rd, _recvep, 0, CREDITS, nullptr, _mem.sel() + 1)) {
        assert(Math::is_aligned(size, DTU_PKG_SIZE));
        if(&rd != &VPE::self() && rd.is_cap_free(caps()))
            rd.delegate(CapRngDesc(CapRngDesc::OBJ, caps()));
        // we assume here that either both have been delegated or none since this does basically
        // only occur if we delegate all our caps to the VPE
        if(&wr != &VPE::self() && wr.is_cap_free(caps()))
            wr.delegate(CapRngDesc(CapRngDesc::OBJ, caps(), 2));
    }
    Pipe(const Pipe&) = delete;
    Pipe &operator=(const Pipe&) = delete;
    ~Pipe() {
        _rd.free_ep(_recvep);
    }

    /**
     * @return the capabilities (memory and gate)
     */
    capsel_t caps() const {
        return _mem.sel();
    }
    /**
     * @return the receive endpoint
     */
    size_t receive_ep() const {
        return _recvep;
    }
    /**
     * @return the size of the shared memory area
     */
    size_t size() const {
        return _size;
    }

    /**
     * Constructs a filepath with given prefix so that one can use VFS::open to open the pipe.
     * This is intended for cases where an application can work with any kind of file and is started
     * via VPE::exec. The application should mount the pipe-fs at <prefix>. You can then simply
     * pass the filepath via command line argument to the application.
     *
     * @param type the type ('r' for reading and 'w' for writing)
     * @param prefix the prefix to put in front of the constructed path
     * @return the filepath for this pipe-end
     */
    String get_path(char type, const char *prefix) const {
        assert(type == 'r' || type == 'w');
        OStringStream os;
        os << prefix << type << '_' << caps() << '_' << receive_ep() << '_' << size();
        return os.str();
    }

private:
    VPE &_rd;
    size_t _recvep;
    size_t _size;
    MemGate _mem;
    SendGate _sgate;
};

}
