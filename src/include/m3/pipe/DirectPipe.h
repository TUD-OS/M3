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

#include <base/Common.h>
#include <base/stream/OStringStream.h>
#include <base/util/Math.h>

#include <m3/com/MemGate.h>
#include <m3/com/SendGate.h>
#include <m3/VPE.h>

#if !defined(SINGLE_ITEM_BUF)
#   define SINGLE_ITEM_BUF  0
#endif

#define DEBUG_PIPE  0
#if DEBUG_PIPE
#   include <base/stream/Serial.h>
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
 * To use the pipe, this class creates two file descriptors for the read-end and write-end. After
 * being done with reading/writing, you need to close the file descriptor to notify the other
 * end. This is also required for the part that you do not use.
 *
 * Caution: the current implementation does only support the communication between the two VPEs
 *          specified on construction.
 *
 * A usage example looks like the following:
 * <code>
 *   VPE reader("reader");
 *
 *   // construct the pipe for VPE::self -> reader
 *   Pipe pipe(reader, VPE::self(), 0x1000);
 *
 *   // bind the read-end to stdin of the child
 *   reader.fds()->set(STDIN_FD, VPE::self().fds()->get(pipe.reader_fd()));
 *   reader.obtain_fds();
 *
 *   reader.run([] {
 *       // read from cin
 *       return 0;
 *   });
 *
 *   // we are done with reading
 *   pipe.close_reader();
 *
 *   File *out = VPE::self().fds()->get(pipe.writer_fd());
 *   // write into out
 *
 *   // we are done with writing
 *   pipe.close_writer();
 *
 *   // wait until the reader exists before destroying the pipe
 *   reader.wait();
 * </code>
 */
class DirectPipe {
public:
    static const size_t MSG_SIZE        = 64;

#if defined(__t2__) || SINGLE_ITEM_BUF
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
    explicit DirectPipe(VPE &rd, VPE &wr, size_t size);
    DirectPipe(const DirectPipe&) = delete;
    DirectPipe &operator=(const DirectPipe&) = delete;
    ~DirectPipe();

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
     * @return the file descriptor for the reader
     */
    fd_t reader_fd() const {
        return _rdfd;
    }
    /**
     * Closes the read-end
     */
    void close_reader();

    /**
     * @return the file descriptor for the writer
     */
    fd_t writer_fd() const {
        return _wrfd;
    }
    /**
     * Closes the write-end
     */
    void close_writer();

private:
    VPE &_rd;
    VPE &_wr;
    size_t _recvep;
    size_t _size;
    MemGate _mem;
    SendGate _sgate;
    fd_t _rdfd;
    fd_t _wrfd;
};

}
