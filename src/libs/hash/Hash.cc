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

#include <m3/com/GateStream.h>
#include <m3/session/Pager.h>
#include <m3/stream/Standard.h>
#include <m3/Syscalls.h>

#include <hash/Hash.h>

using namespace m3;

namespace hash {

Hash::AccelIMem::AccelIMem()
    : _vpe("acc", PEDesc(PEType::COMP_IMEM, PEISA::ACCEL_HASH)) {
}

Hash::AccelEMem::AccelEMem()
    : _vpe("acc", PEDesc(PEType::COMP_EMEM, PEISA::ACCEL_HASH), "pager") {
    if(_vpe.pager()) {
        uintptr_t virt = BUF_ADDR;
        _vpe.pager()->map_anon(&virt, BUF_SIZE, Pager::Prot::READ | Pager::Prot::WRITE, 0);
    }
}

uintptr_t Hash::AccelIMem::getRBAddr() {
    return _vpe.pe().mem_size() - RECVBUF_SIZE_SPM + DEF_RCVBUF_SIZE + UPCALL_RBUF_SIZE;
}

uintptr_t Hash::AccelEMem::getRBAddr() {
    return RECVBUF_SPACE + DEF_RCVBUF_SIZE + UPCALL_RBUF_SIZE;
}

Hash::Hash()
    : _accel(get_accel()),
      _rbuf(RecvBuf::create(VPE::self().alloc_ep(), nextlog2<256>::val, 0)),
      _rgate(RecvGate::create(&_rbuf)),
      _send(SendGate::create_for(_accel->get(), EPID, 0, 256, &_rgate)) {
    _accel->get().start();

    Errors::Code res = Syscalls::get().attachrb(_accel->get().sel(), EPID,
        _accel->getRBAddr(), getnextlog2(RB_SIZE), getnextlog2(RB_SIZE), 0);
    if(res != Errors::NO_ERROR)
        exitmsg("Unable to attach receive buffer on accelerator");
}

Hash::~Hash() {
    delete _accel;
}

Hash::Accel *Hash::get_accel() {
    Accel *acc = new AccelIMem();
    if(Errors::last != Errors::NO_ERROR) {
        delete acc;
        acc = new AccelEMem();
        if(Errors::last != Errors::NO_ERROR)
            exitmsg("Unable to find accelerator");
    }
    return acc;
}

size_t Hash::get(Algorithm algo, const void *data, size_t len, void *res, size_t max) {
    assert(len <= BUF_SIZE);
    _accel->get().mem().write_sync(data, len, BUF_ADDR);

    uint64_t count;
    GateIStream is = send_receive_vmsg(_send,
        static_cast<uint64_t>(algo),
        static_cast<uint64_t>(len)
    );
    is >> count;

    if(count == 0)
        return 0;
    memcpy(res, is.buffer() + sizeof(uint64_t), Math::min(max, static_cast<size_t>(count)));
    return count;
}

}
