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

#include <m3/Common.h>
#include <m3/vfs/FileSystem.h>

namespace m3 {

/**
 * A pipe-filesystem, which parses all required information to construct a PipeReader or PipeWriter
 * object from the filename.
 *
 * @see Pipe{Read,Write}End::get_path().
 */
class PipeFS : public FileSystem {
public:
    virtual char type() const override {
        return 'P';
    }
    virtual File *open(const char *path, int perms) override;
    virtual Errors::Code stat(const char *, FileInfo &) override {
        return Errors::NOT_SUP;
    }
    virtual Errors::Code mkdir(const char *, mode_t) override {
        return Errors::NOT_SUP;
    }
    virtual Errors::Code rmdir(const char *) override {
        return Errors::NOT_SUP;
    }
    virtual Errors::Code link(const char *, const char *) override {
        return Errors::NOT_SUP;
    }
    virtual Errors::Code unlink(const char *) override {
        return Errors::NOT_SUP;
    }
};

}
