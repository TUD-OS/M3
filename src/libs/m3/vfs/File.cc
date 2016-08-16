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

bool File::Buffer::putback(off_t, char c) {
    if(cur > 0 && pos > 0) {
        buffer[--pos] = c;
        return true;
    }
    return false;
}

ssize_t File::Buffer::read(File *file, off_t, void *dst, size_t amount) {
    if(pos < static_cast<off_t>(cur)) {
        size_t count = Math::min(amount, static_cast<size_t>(cur - pos));
        memcpy(dst, buffer + pos, count);
        pos += count;
        return count;
    }

    ssize_t res = file->read(buffer, size);
    if(res <= 0)
        return res;
    cur = res;

    size_t copyamnt = Math::min(static_cast<size_t>(res), amount);
    memcpy(dst, buffer, copyamnt);
    pos = copyamnt;
    return copyamnt;
}

ssize_t File::Buffer::write(File *file, off_t, const void *src, size_t amount) {
    if(cur == size)
        flush(file);

    size_t count = Math::min(size - cur, amount);
    memcpy(buffer + cur, src, count);
    cur += count;
    return count;
}

int File::Buffer::seek(off_t, int, off_t &) {
    // not supported
    return -1;
}

ssize_t File::Buffer::flush(File *file) {
    if(cur > 0) {
        file->write(buffer, cur);
        cur = 0;
    }
    return 1;
}

}
