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

#include <base/Common.h>
#include <base/Config.h>

#if defined(__x86_64__)
#   include <base/arch/x86_64/ExceptionState.h>
#elif defined(__arm__)
#   include <base/arch/arm/ExceptionState.h>
#else
#   error "Unsupported ISA"
#endif

namespace m3 {

class Exceptions {
public:
    typedef ExceptionState State;

    typedef void *(*isr_func)(State *state);

    static void init();

private:
    static void *handler(State *state);
};

}
