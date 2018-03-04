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

#include <m3/vfs/File.h>

namespace m3 {

bool File::Buffer::putback(char c) {
    if(cur > 0 && pos > 0) {
        buffer[--pos] = c;
        return true;
    }
    return false;
}

ssize_t File::Buffer::read(File *file, void *dst, size_t amount) {
    if(pos < cur) {
        size_t count = Math::min(amount, cur - pos);
        memcpy(dst, buffer + pos, count);
        pos += count;
        return static_cast<ssize_t>(count);
    }

    ssize_t res = file->read(buffer, size);
    if(res <= 0)
        return res;
    cur = static_cast<size_t>(res);

    size_t copyamnt = Math::min(static_cast<size_t>(res), amount);
    memcpy(dst, buffer, copyamnt);
    pos = copyamnt;
    return static_cast<ssize_t>(copyamnt);
}

ssize_t File::Buffer::write(File *file, const void *src, size_t amount) {
    if(cur == size)
        flush(file);

    size_t count = Math::min(size - cur, amount);
    memcpy(buffer + cur, src, count);
    cur += count;
    return static_cast<ssize_t>(count);
}

ssize_t File::Buffer::flush(File *file) {
    size_t off = 0;
    while(off < cur) {
        ssize_t res = file->write(buffer + off, cur - off);
        if(res < 0)
            return res;
        off += static_cast<size_t>(res);
    }
    cur = 0;
    return 1;
}

}
