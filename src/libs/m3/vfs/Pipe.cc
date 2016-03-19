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

#include <m3/vfs/Pipe.h>

namespace m3 {

int PipeFile::stat(FileInfo &info) const {
    // TODO
    info.devno = 0;
    info.inode = 0;
    info.lastaccess = info.lastmod = 0;
    info.links = 1;
    info.mode = S_IFPIP | S_IRUSR | S_IWUSR;
    info.size = 0;
    return 0;
}

}
