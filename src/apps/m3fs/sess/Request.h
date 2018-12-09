/*
 * Copyright (C) 2018, Sebastian Reimers <sebastian.reimers@mailbox.tu-dresden.de>
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

class FSHandle;
class MetaBufferHead;

class Request {
    static constexpr size_t MAX_USED_BLOCKS = 16;

public:
    explicit Request(FSHandle &handle)
        : _handle(handle),
          _used(0) {
    }
    ~Request();

    FSHandle &hdl() {
        return _handle;
    }

    size_t used_meta() const {
        return _used;
    }
    void push_meta(MetaBufferHead *b);
    void pop_meta();
    void pop_meta(size_t n);

private:
    FSHandle &_handle;
    size_t _used;
    MetaBufferHead *_blocks[MAX_USED_BLOCKS];
};
