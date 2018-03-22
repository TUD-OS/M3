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

#include "../data/INodes.h"
#include "OpenFiles.h"

void OpenFiles::delete_file(m3::inodeno_t ino) {
    OpenFile *file = get_file(ino);
    if(file)
        file->deleted = true;
    else
        INodes::free(_hdl, ino);
}

void OpenFiles::add_sess(M3FSFileSession *sess) {
    OpenFile *file = get_file(sess->ino());
    if(!file) {
        file = new OpenFile(sess->ino());
        files.insert(file);
    }

    file->sessions.append(sess);
}

void OpenFiles::rem_sess(M3FSFileSession *sess) {
    OpenFile *file = get_file(sess->ino());
    assert(file != nullptr);

    file->sessions.remove(sess);

    if(file->sessions.length() == 0) {
        files.remove(file);
        if(file->deleted)
            INodes::free(_hdl, sess->ino());
        delete file;
    }
}
