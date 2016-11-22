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
#include <c/div.h>

namespace m3 {

/**
 * Helper class to count the number of digits in a number
 */
class Digits {
public:
    /**
     * @param n the unsigned number
     * @param base the base to use for digit-counting
     * @return the number of digits the number has when represented in base <base>
     */
    template<typename T>
    static uint count_unsigned(T n, uint base) {
        uint width = 1;
        while(n >= base) {
            T rem;
            n = divide(n, base, &rem);
            width++;
        }
        return width;
    }

    /**
     * @param n the signed number
     * @param base the base to use for digit-counting
     * @return the number of digits the number has when represented in base <base>
     */
    template<typename T>
    static uint count_signed(T n, int base) {
        // we have at least one char
        uint width = 1;
        if(n < 0) {
            width++;
            n = -n;
        }
        while(n >= base) {
            T rem;
            n = divide(n, base, &rem);
            width++;
        }
        return width;
    }

private:
    Digits();
};

}
