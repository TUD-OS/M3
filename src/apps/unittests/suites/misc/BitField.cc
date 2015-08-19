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

#include <m3/Common.h>
#include <m3/BitField.h>
#include <m3/Log.h>
#include "BitField.h"

using namespace m3;

void BitFieldTestSuite::FirstClearTestCase::run() {
    {
        BitField<16> bf;
        assert_int(bf.first_clear(), 0);

        bf.set(0);
        assert_int(bf.first_clear(), 1);

        bf.set(1);
        assert_int(bf.first_clear(), 2);

        bf.set(3);
        assert_int(bf.first_clear(), 2);

        for(int i = 0; i < 16; ++i)
            bf.set(i);
        assert_int(bf.first_clear(), 16);
    }

    {
        BitField<65> bf;

        bf.set(33);
        assert_int(bf.first_clear(), 0);

        for(int i = 0; i < 65; ++i)
            bf.set(i);
        assert_int(bf.first_clear(), 65);
    }

    {
        BitField<10> bf;
        for(int i = 0; i < 10; ++i)
            bf.set(i);
        assert_int(bf.first_clear(), 10);

        bf.clear(9);
        assert_int(bf.first_clear(), 9);

        bf.clear(3);
        assert_int(bf.first_clear(), 3);

        bf.set(3);
        assert_int(bf.first_clear(), 9);

        bf.clear(6);
        bf.clear(7);
        assert_int(bf.first_clear(), 6);

        bf.set(6);
        assert_int(bf.first_clear(), 7);

        bf.set(9);
        assert_int(bf.first_clear(), 7);

        bf.set(7);
        assert_int(bf.first_clear(), 10);
    }
}
