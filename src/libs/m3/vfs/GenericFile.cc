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

#include <base/log/Lib.h>
#include <base/util/Time.h>

#include <m3/com/GateStream.h>
#include <m3/session/M3FS.h>
#include <m3/vfs/FileTable.h>
#include <m3/vfs/GenericFile.h>
#include <m3/Syscalls.h>

namespace m3 {

GenericFile::GenericFile(int flags, capsel_t caps, size_t id, epid_t mep, M3FS *sess_obj)
    : File(flags),
      _id(id),
      _sess_obj(sess_obj),
      _sess(caps + 0, sess_obj ? ObjCap::KEEP_CAP : 0),
      _sg(sess_obj ? &sess_obj->_gate : new SendGate(SendGate::bind(caps + 1))),
      _mg(MemGate::bind(ObjCap::INVALID)),
      _goff(),
      _off(),
      _pos(),
      _len(),
      _writing() {
    if(mep != EP_COUNT)
        _mg.ep(mep);
}

GenericFile::~GenericFile() {
    if(_writing)
        submit();

    if(flags() & FILE_NOSESS) {
        LLOG(FS, "GenFile[" << fd() << "," << _id << "]::close()");
        send_receive_vmsg(*_sg, M3FS::CLOSE_PRIV, _id);
        assert(_sess_obj);
        _sess_obj->free_ep(VPE::self().ep_to_sel(_mg.ep()));
    }
    else {
        if(_mg.ep() != MemGate::UNBOUND) {
            LLOG(FS, "GenFile[" << fd() << "," << _id << "]::revoke_ep(" << _mg.ep() << ")");
            capsel_t sel = VPE::self().ep_to_sel(_mg.ep());
            VPE::self().revoke(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sel), true);
            VPE::self().free_ep(_mg.ep());
        }
        delete _sg;
    }
}

Errors::Code GenericFile::stat(FileInfo &info) const {
    LLOG(FS, "GenFile[" << fd() << "," << _id << "]::stat()");

    GateIStream reply = send_receive_vmsg(*_sg, STAT, _id);
    reply >> Errors::last;
    if(Errors::last == Errors::NONE)
        reply >> info;
    return Errors::last;
}

ssize_t GenericFile::seek(size_t offset, int whence) {
    LLOG(FS, "GenFile[" << fd() << "," << _id << "]::seek(" << offset << ", " << whence << ")");

    // handle SEEK_CUR as SEEK_SET
    if(whence == M3FS_SEEK_CUR) {
        offset = _goff + _pos + offset;
        whence = M3FS_SEEK_SET;
    }

    // try to seek locally first
    if(whence == M3FS_SEEK_SET) {
        // no change?
        if(offset == _goff + _pos)
            return static_cast<ssize_t>(offset);

        // first submit the written data
        if(_writing && submit() != Errors::NONE)
            return -1;

        if(offset >= _goff && offset <= _goff + _len) {
            _pos = offset - _goff;
            return static_cast<ssize_t>(offset);
        }
    }
    else {
        // first submit the written data
        if(_writing && submit() != Errors::NONE)
            return -1;
    }

    // now seek on the server side
    size_t off;
    GateIStream reply = !have_sess() ? send_receive_vmsg(*_sg, SEEK, _id, offset, whence)
                                     : send_receive_vmsg(*_sg, SEEK, offset, whence);
    reply >> Errors::last;
    if(Errors::last != Errors::NONE)
        return -1;

    reply >> _goff >> off;
    _pos = _len = 0;
    return static_cast<ssize_t>(_goff + off);
}

ssize_t GenericFile::read(void *buffer, size_t count) {
    if(delegate_ep() != Errors::NONE)
        return -1;
    if(_writing && submit() != Errors::NONE)
        return -1;

    LLOG(FS, "GenFile[" << fd() << "," << _id << "]::read("
        << count << ", pos=" << (_goff + _pos) << ")");

    if(_pos == _len) {
        Time::start(0xbbbb);
        GateIStream reply = !have_sess() ? send_receive_vmsg(*_sg, READ, _id, static_cast<size_t>(0))
                                         : send_receive_vmsg(*_sg, READ, static_cast<size_t>(0));
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
        if(flags() & FILE_NODATA)
            CPU::compute(count / 2);
        else
            _mg.read(buffer, amount, _off + _pos);
        Time::stop(0xaaaa);
        _pos += amount;
    }
    return static_cast<ssize_t>(amount);
}

ssize_t GenericFile::write(const void *buffer, size_t count) {
    if(delegate_ep() != Errors::NONE)
        return -1;

    LLOG(FS, "GenFile[" << fd() << "," << _id << "]::write("
        << count << ", pos=" << (_goff + _pos) << ")");

    if(_pos == _len) {
        Time::start(0xbbbb);
        GateIStream reply = !have_sess() ? send_receive_vmsg(*_sg, WRITE, _id, static_cast<size_t>(0))
                                         : send_receive_vmsg(*_sg, WRITE, static_cast<size_t>(0));
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
        if(flags() & FILE_NODATA)
            CPU::compute(count / 4);
        else
            _mg.write(buffer, amount, _off + _pos);
        Time::stop(0xaaaa);
        _pos += amount;
    }
    _writing = true;
    return static_cast<ssize_t>(amount);
}

void GenericFile::evict() {
    assert(!(flags() & FILE_NOSESS));
    assert(_mg.ep() != MemGate::UNBOUND);
    LLOG(FS, "GenFile[" << fd() << "," << _id << "]::evict()");

    // submit read/written data
    submit();

    // revoke EP cap
    capsel_t ep_sel = VPE::self().ep_to_sel(_mg.ep());
    VPE::self().revoke(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, ep_sel), true);
    _mg.ep(MemGate::UNBOUND);
}

Errors::Code GenericFile::submit() {
    if(_pos > 0) {
        LLOG(FS, "GenFile[" << fd() << "," << _id << "]::submit("
            << (_writing ? "write" : "read") << ", " << _pos << ")");

        GateIStream reply = !have_sess() ? send_receive_vmsg(*_sg, _writing ? WRITE : READ, _id, _pos)
                                         : send_receive_vmsg(*_sg, _writing ? WRITE : READ, _pos);
        reply >> Errors::last;
        if(Errors::last != Errors::NONE)
            return Errors::last;

        // if we append, the file was truncated
        size_t filesize;
        reply >> filesize;
        if(_goff + _len > filesize)
            _len = filesize - _goff;
        _goff += _pos;
        _pos = _len = 0;
    }
    _writing = false;
    return Errors::NONE;
}

Errors::Code GenericFile::delegate_ep() {
    if(_mg.ep() == MemGate::UNBOUND) {
        assert(!(flags() & FILE_NOSESS));
        epid_t ep = VPE::self().fds()->request_ep(this);
        LLOG(FS, "GenFile[" << fd() << "," << _id << "]::delegate_ep(" << ep << ")");
        _sess.delegate_obj(VPE::self().ep_to_sel(ep));
        if(Errors::last != Errors::NONE)
            return Errors::last;
        _mg.ep(ep);
    }
    return Errors::NONE;
}

}
