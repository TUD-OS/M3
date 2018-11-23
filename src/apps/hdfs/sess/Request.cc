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

#include "../FSHandle.h"
#include "Request.h"

Request::~Request() {
    for(size_t i = 0; i < _used; i++)
        _handle.metabuffer().quit(_blocks[i]);
}

void Request::push_meta(MetaBufferHead *b) {
    _blocks[_used] = b;
    _used++;
}

void Request::pop_meta() {
    assert(_used > 0);
    _handle.metabuffer().quit(_blocks[--_used]);
}

void Request::pop_meta(size_t n) {
    assert(_used >= n);
    for(size_t i = 0; i < n; i++)
        _handle.metabuffer().quit(_blocks[--_used]);
}
