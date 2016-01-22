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

#include <m3/Types.h>

namespace m3 {

static constexpr size_t _getnextlog2(size_t size, int shift) {
    return size > (static_cast<size_t>(1) << shift)
            ? shift + 1
            : (shift == 0 ? 0 : _getnextlog2(size, shift - 1));
}
/**
 * Converts <size> to x with 2^x >= <size>. It may be executed at compiletime or runtime,
 */
static constexpr size_t getnextlog2(size_t size) {
    return _getnextlog2(size, sizeof(size_t) * 8 - 2);
}

/**
 * Converts <size> to x with 2^x >= <size>. It is always executed at compiletime.
 */
template<size_t SIZE>
struct nextlog2 {
    static constexpr int val = getnextlog2(SIZE);
};

static_assert(nextlog2<0>::val == 0, "failed");
static_assert(nextlog2<1>::val == 0, "failed");
static_assert(nextlog2<8>::val == 3, "failed");
static_assert(nextlog2<10>::val == 4, "failed");
static_assert(nextlog2<100>::val == 7, "failed");
static_assert(nextlog2<1UL << 31>::val == 31, "failed");
static_assert(nextlog2<(1UL << 30) + 1>::val == 31, "failed");
static_assert(nextlog2<(1UL << (sizeof(size_t) * 8 - 1)) + 1>::val == (sizeof(size_t) * 8 - 1), "failed");

template<typename T>
struct remove_reference {
    using type = T;
};
template<typename T>
struct remove_reference<T&> {
    using type = T;
};
template<typename T>
struct remove_reference<T&&> {
    using type = T;
};

class Util {
public:
    template<typename T>
    static typename remove_reference<T>::type && move(T &&t) {
        return static_cast<typename remove_reference<T>::type&&>(t);
    }
    template<typename T>
    static constexpr T&& forward(typename remove_reference<T>::type& a) {
        return static_cast<T&&>(a);
    }
};

}
