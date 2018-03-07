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

#include <base/util/Time.h>

#include <m3/com/GateStream.h>
#include <m3/session/M3FS.h>
#include <m3/vfs/GenericFile.h>
#include <m3/Syscalls.h>

namespace m3 {

GenericFile::~GenericFile() {
    submit();
    if(_mg.ep() != MemGate::UNBOUND) {
        capsel_t sel = VPE::self().ep_sel(_mg.ep());
        VPE::self().revoke(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sel), true);
        VPE::self().free_ep(_mg.ep());
    }
}

Errors::Code GenericFile::stat(FileInfo &info) {
    GateIStream reply = send_receive_vmsg(_sg, STAT);
    reply >> Errors::last;
    if(Errors::last == Errors::NONE)
        reply >> info;
    return Errors::last;
}

ssize_t GenericFile::seek(size_t offset, int whence) {
    if(submit() != Errors::NONE)
        return -1;

    if(whence == SEEK_CUR) {
        offset = _goff + _pos + offset;
        whence = SEEK_SET;
    }

    if(whence != SEEK_END && _pos < _len) {
        if(offset > _goff && offset < _goff + _len) {
            _pos = offset - _goff;
            return static_cast<ssize_t>(offset);
        }
    }

    size_t off;
    GateIStream reply = send_receive_vmsg(_sg, SEEK, offset, whence);
    reply >> Errors::last >> _goff >> off;
    _pos = _len = 0;
    if(Errors::last != Errors::NONE)
        return -1;
    return static_cast<ssize_t>(_goff + off);
}

ssize_t GenericFile::read(void *buffer, size_t count) {
    if(delegate_ep() != Errors::NONE || submit() != Errors::NONE)
        return -1;

    if(_pos == _len) {
        Time::start(0xbbbb);
        GateIStream reply = send_receive_vmsg(_sg, READ);
        reply >> Errors::last;
        Time::stop(0xbbbb);
        if(Errors::last != Errors::NONE)
            return -1;

        _goff += _len;
        reply >> _off >> _len;
        _pos = 0;
    }

    size_t amount = Math::min(count, _len - _pos);
    if(amount > 0) {
        Time::start(0xaaaa);
        _mg.read(buffer, amount, _off + _pos);
        Time::stop(0xaaaa);
        _pos += amount;
    }
    _writing = false;
    return static_cast<ssize_t>(amount);
}

ssize_t GenericFile::write(const void *buffer, size_t count) {
    if(delegate_ep() != Errors::NONE)
        return -1;

    if(_pos == _len) {
        Time::start(0xbbbb);
        GateIStream reply = send_receive_vmsg(_sg, WRITE, static_cast<size_t>(0));
        reply >> Errors::last;
        Time::stop(0xbbbb);
        if(Errors::last != Errors::NONE)
            return -1;

        _goff += _len;
        reply >> _off >> _len;
        _pos = 0;
    }

    size_t amount = Math::min(count, _len - _pos);
    if(amount > 0) {
        Time::start(0xaaaa);
        _mg.write(buffer, amount, _off + _pos);
        Time::stop(0xaaaa);
        _pos += amount;
    }
    _writing = true;
    return static_cast<ssize_t>(amount);
}

Errors::Code GenericFile::submit() {
    if(_writing && _pos > 0) {
        GateIStream reply = send_receive_vmsg(_sg, WRITE, _off + _pos);
        reply >> Errors::last;
        if(Errors::last != Errors::NONE)
            return Errors::last;
        // if we append, the file was truncated
        size_t filesize;
        reply >> filesize;
        if(_goff + _len > filesize)
            _len = filesize - _goff;
    }
    return Errors::NONE;
}

Errors::Code GenericFile::delegate_ep() {
    if(_mg.ep() == MemGate::UNBOUND) {
        epid_t ep = VPE::self().alloc_ep();
        _sess.delegate_obj(VPE::self().ep_sel(ep));
        if(Errors::last != Errors::NONE)
            return Errors::last;
        _mg.ep(ep);
    }
    return Errors::NONE;
}

}
