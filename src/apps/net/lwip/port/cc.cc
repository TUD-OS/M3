/*
 * Copyright (C) 2017, Georg Kotheimer <georg.kotheimer@mailbox.tu-dresden.de>
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

#include "arch/cc.h"

#include <base/Heap.h>
#include <base/stream/Serial.h>

#include <cstdarg>

#include <mini_printf.h>

// adapted from https://stackoverflow.com/a/69911
int printf_adapter(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    // Allocate a buffer on the stack that's big enough for us almost
    // all the time.
    size_t size = 1024;
    char stackbuf[size];
    char * dynamicbuf = nullptr;
    char * buf = &stackbuf[0];

    va_list apcopy;
    while(1) {
        va_copy(apcopy, ap);
        int needed = mini_vsnprintf(buf, size, fmt, ap);
        va_end(apcopy);
        if(needed != -1) {
            // It fit fine the first time, we're done.
            m3::Serial::get() << buf;
            if(dynamicbuf)
                m3::Heap::free(dynamicbuf);
            return needed;
        }

        // vsnprintf reported that it wanted to write more characters
        // than we allotted.  So try again using a dynamic buffer.  This
        // doesn't happen very often if we chose our initial size well.
        size = size * 2;
        dynamicbuf = static_cast<char*>(m3::Heap::realloc(dynamicbuf, size));
        buf = dynamicbuf;
    }
}

