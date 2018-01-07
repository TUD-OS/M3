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

#include <base/stream/OStream.h>

#include <m3/com/GateStream.h>

namespace m3 {

template<size_t N>
class LocList {
public:
    explicit LocList() : _sel(ObjCap::INVALID), _count(), _lengths() {
    }

    void append(size_t length) {
        assert(_count < N);
        _lengths[_count++] = length;
    }
    void clear() {
        _count = 0;
        memset(_lengths, 0, sizeof(_lengths));
    }

    size_t total_length() const {
        size_t len = 0;
        for(size_t i = 0; i < _count; ++i)
            len += _lengths[i];
        return len;
    }

    size_t count() const {
        return _count;
    }
    size_t get_len(size_t i) const {
        return i < _count ? _lengths[i] : 0;
    }

    capsel_t get_sel(size_t i) const {
        return _sel + i;
    }
    void set_sel(capsel_t sel) {
        _sel = sel;
    }

    friend OStream &operator <<(OStream &os, const LocList &l) {
        os << "LocList[";
        for(size_t i = 0; i < l.count(); ++i) {
            os << l.get(i);
            if(i != l.count() - 1)
                os << ", ";
        }
        os << "]";
        return os;
    }

private:
    capsel_t _sel;
    size_t _count;
    size_t _lengths[N];
};

}
