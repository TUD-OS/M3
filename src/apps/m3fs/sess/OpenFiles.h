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

#include <base/col/SList.h>
#include <base/col/Treap.h>

#include <fs/internal.h>

#include "FileSession.h"

class FSHandle;

class OpenFiles {
public:
    struct OpenFile : public m3::TreapNode<OpenFile, m3::inodeno_t> {
        explicit OpenFile(m3::inodeno_t ino)
            : m3::TreapNode<OpenFile, m3::inodeno_t>(ino), deleted(false) {
        }

        bool deleted;
        m3::SList<M3FSFileSession> sessions;
    };

    explicit OpenFiles(FSHandle &hdl)
        : _hdl(hdl),
          _files() {
    }

    OpenFile *get_file(m3::inodeno_t ino) {
        return _files.find(ino);
    }

    void delete_file(m3::inodeno_t ino);

    void add_sess(M3FSFileSession *sess);
    void rem_sess(M3FSFileSession *sess);

private:
    FSHandle &_hdl;
    m3::Treap<OpenFile> _files;
};
