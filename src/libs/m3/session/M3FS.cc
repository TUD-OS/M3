/*
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <m3/com/GateStream.h>
#include <m3/session/M3FS.h>
#include <m3/vfs/GenericFile.h>

namespace m3 {

File *M3FS::open(const char *path, int perms) {
    capsel_t ep;
    if((perms & FILE_NOSESS) && (ep = alloc_ep()) != ObjCap::INVALID) {
        GateIStream reply = send_receive_vmsg(_gate, OPEN_PRIV, path, perms, ep - _eps);
        reply >> Errors::last;
        if(Errors::last != Errors::NONE)
            return nullptr;
        size_t id;
        reply >> id;
        return new GenericFile(perms, sel(), id, VPE::self().sel_to_ep(ep), this);
    }
    else {
        perms &= ~FILE_NOSESS;
        KIF::ExchangeArgs args;
        args.count = 1;
        args.svals[0] = static_cast<xfer_t>(perms);
        strncpy(args.str, path, sizeof(args.str));
        KIF::CapRngDesc crd = obtain(2, &args);
        if(Errors::last != Errors::NONE)
            return nullptr;
        return new GenericFile(perms, crd.start());
    }
}

Errors::Code M3FS::stat(const char *path, FileInfo &info) {
    GateIStream reply = send_receive_vmsg(_gate, STAT, path);
    reply >> Errors::last;
    if(Errors::last != Errors::NONE)
        return Errors::last;
    reply >> info;
    return Errors::NONE;
}

Errors::Code M3FS::mkdir(const char *path, mode_t mode) {
    GateIStream reply = send_receive_vmsg(_gate, MKDIR, path, mode);
    reply >> Errors::last;
    return Errors::last;
}

Errors::Code M3FS::rmdir(const char *path) {
    GateIStream reply = send_receive_vmsg(_gate, RMDIR, path);
    reply >> Errors::last;
    return Errors::last;
}

Errors::Code M3FS::link(const char *oldpath, const char *newpath) {
    GateIStream reply = send_receive_vmsg(_gate, LINK, oldpath, newpath);
    reply >> Errors::last;
    return Errors::last;
}

Errors::Code M3FS::unlink(const char *path) {
    GateIStream reply = send_receive_vmsg(_gate, UNLINK, path);
    reply >> Errors::last;
    return Errors::last;
}

Errors::Code M3FS::delegate(VPE &vpe) {
    if(vpe.delegate_obj(sel()) != Errors::NONE)
        return Errors::last;
    // TODO what if it fails?
    return obtain_for(vpe, KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sel() + 1, 1));
}

void M3FS::serialize(Marshaller &m) {
    m << sel();
}

FileSystem *M3FS::unserialize(Unmarshaller &um) {
    capsel_t sel;
    um >> sel;
    return new M3FS(sel);
}

}
