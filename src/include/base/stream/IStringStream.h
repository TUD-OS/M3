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
#include <base/util/String.h>
#include <cstring>

namespace m3 {

/**
 * Input-stream that reads from a string
 */
class IStringStream : public IStream {
public:
    /**
     * Reads a value of type <T> from the given string
     *
     * @param str the string
     * @return the read value
     */
    template<typename T>
    static T read_from(const String &str) {
        IStringStream is(const_cast<String&>(str));
        T t;
        is >> t;
        return t;
    }

    /**
     * Constructor
     *
     * @param str the string (not copied and not changed, but non-const to prevent that someone
     *  accidently passes in a temporary)
     */
    explicit IStringStream(String &str)
        : IStream(), _str(str), _pos() {
    }

    virtual char read() override {
        if(_pos < _str.length())
            return _str.c_str()[_pos++];
        _state |= FL_EOF;
        return '\0';
    }
    virtual bool putback(char c) override {
        if(_pos == 0 || _str[_pos - 1] != c)
            return false;
        _pos--;
        return true;
    }

private:
    const String &_str;
    size_t _pos;
};

}
