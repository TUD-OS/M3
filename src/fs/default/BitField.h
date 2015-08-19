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

#pragma once

#include <m3/Common.h>
#include <m3/stream/OStream.h>

namespace m3 {

template<uint BITS>
class BitField;
template<uint BITS>
static OStream &operator<<(OStream &os, const BitField<BITS> &bf);

/**
 * A field of <BITS> bits that is managed in an array of words.
 */
template<uint BITS>
class BitField {
    template<uint N>
    friend OStream & operator<<(OStream &os, const BitField<N> &bf);

    static size_t idx(uint bit) {
        return bit / (sizeof(word_t) * 8);
    }
    static size_t bitpos(uint bit) {
        return 1UL << (bit % (sizeof(word_t) * 8));
    }

public:
    /**
     * Constructor
     */
    explicit BitField() : _first_clear(0), _words() {
    }

    /**
     * @param bit the bit
     * @return true if the bit <bit> is set
     */
    bool is_set(uint bit) const {
        return (_words[idx(bit)] & bitpos(bit)) != 0;
    }

    /**
     * @return the first clear bit in the bitfield (BITS if there is none)
     */
    uint first_clear() const {
        return _first_clear;
    }

    /**
     * Sets bit <bit> to 1
     */
    void set(uint bit) {
        _words[idx(bit)] |= bitpos(bit);
        if(bit == _first_clear) {
            for(_first_clear++; is_set(_first_clear) && _first_clear < BITS; ++_first_clear)
                ;
        }
    }
    /**
     * Sets bit <bit> to 0
     */
    void clear(uint bit) {
        _words[idx(bit)] &= ~bitpos(bit);
        if(bit < _first_clear)
            _first_clear = bit;
    }
    /**
     * Sets bit <bit> to <value>
     */
    void set(uint bit, bool value) {
        if(value)
            set(bit);
        else
            clear(bit);
    }

private:
    uint _first_clear;
    word_t _words[(BITS + sizeof(word_t) * 8 - 1) / (sizeof(word_t) * 8)];
};

template<uint BITS>
static inline OStream &operator<<(OStream &os, const BitField<BITS> &bf) {
    os << "Bitfield[";
    for(size_t i = 0; i < ARRAY_SIZE(bf._words); ++i) {
        os << fmt(bf._words[i], "0x", sizeof(bf._words[i]) * 2);
        if(i + 1 < ARRAY_SIZE(bf._words))
            os << ' ';
    }
    os << ']';
    return os;
}

}
