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

namespace m3 {

/**
 * The base-class of IStream and OStream that holds the state of the stream.
 */
class IOSBase {
public:
    enum {
        FL_EOF      = 1 << 0,
        FL_ERROR    = 1 << 1,
    };

    explicit IOSBase() : _state() {
    }
    virtual ~IOSBase() {
    }

    /**
     * Resets all flags
     */
    void clearerr() {
        _state = 0;
    }

    /**
     * @return true if everything is ok
     */
    bool good() const {
        return _state == 0;
    }
    /**
     * @return true if EOF has been reached or an error occurred
     */
    bool bad() const {
        return _state != 0;
    }
    /**
     * @return true if an error has occurred
     */
    bool error() const {
        return _state & FL_ERROR;
    }
    /**
     * @return true if EOF has been reached
     */
    bool eof() const {
        return _state & FL_EOF;
    }

    bool operator!() const {
        return bad();
    }
    operator bool() const {
        return good();
    }

protected:
    uint _state;
};

}
