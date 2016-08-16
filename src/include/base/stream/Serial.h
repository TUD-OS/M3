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

#include <base/stream/IStream.h>
#include <base/stream/OStream.h>
#include <base/DTU.h>
#include <base/Machine.h>

namespace m3 {

/**
 * An input- and output stream that uses the "serial line" (what that exactly means depends on the
 * architecture). This can be used for logging. For example:
 * Serial::get() << "Hello, " << 123 << " World!\n";
 * Note that it is line-buffered.
 */
class Serial : public IStream, public OStream {
    static const size_t OUTBUF_SIZE = 160;
    static const size_t INBUF_SIZE  = 64;
    static const size_t SUFFIX_LEN  = sizeof("\e[0m") - 1;

public:
    /**
     * @return the instance
     */
    static Serial &get() {
        return *_inst;
    }

    /**
     * @return true if it is ready to print
     */
    static bool ready() {
        return _inst != nullptr;
    }

    /**
     * Initializes everything. Should only be called at the beginning.
     *
     * @param path the path of the program
     * @param core the core number
     */
    static void init(const char *path, int core);

    /**
     * Flushes the output
     */
    void flush();

private:
    explicit Serial() : IStream(), OStream(), _outpos(0), _inpos(0), _inlen(0) {
    }

    virtual char read() override;
    virtual bool putback(char c) override;
    virtual void write(char c) override;

    size_t _start;
    size_t _outpos;
    size_t _inpos;
    size_t _inlen;
    alignas(DTU_PKG_SIZE) char _outbuf[OUTBUF_SIZE];
    alignas(DTU_PKG_SIZE) char _inbuf[INBUF_SIZE];

    static const char *_colors[];
    static Serial *_inst;
};

}
