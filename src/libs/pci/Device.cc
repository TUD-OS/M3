/*
 * Copyright (C) 2017, Georg Kotheimer <georg.kotheimer@mailbox.tu-dresden.de>
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

#include <base/Panic.h>

#include <m3/stream/Standard.h>
#include <m3/Syscalls.h>

#include <pci/Device.h>

using namespace m3;

namespace net {

ProxiedPciDevice::ProxiedPciDevice(PEISA isa)
    : _vpe("nic", PEDesc(PEType::COMP_IMEM, isa)),
      // TODO: validate that a suitable pe has been found
      _intgate(RecvGate::create(nextlog2<256>::val, nextlog2<32>::val)),
      // TODO: Specify receive gate, grant it to nic dtu, send replies to give credits back
      _sintgate(SendGate::create(&_intgate, 0, SendGate::UNLIMITED)) {
    _intgate.activate();
    _intgate.start(std::bind(receiveInterrupt, this, std::placeholders::_1));

    _vpe.delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _sintgate.sel(), 1));
    if(_sintgate.activate_for(_vpe, EP_INT) != Errors::NONE)
        PANIC("Unable to activate interrupt EP for proxied pci device: " << Errors::last);

    _vpe.start();
}

uint32_t ProxiedPciDevice::readReg(size_t offset) {
    uint32_t result;
    _vpe.mem().read(&result, sizeof(uint32_t), REG_ADDR + offset);
    return result;
}

void ProxiedPciDevice::writeReg(size_t offset, uint32_t value) {
    _vpe.mem().write(&value, sizeof(uint32_t), REG_ADDR + offset);
}

void ProxiedPciDevice::setDmaEp(m3::MemGate &memgate) {
    _vpe.delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, memgate.sel(), 1));
    if(memgate.activate_for(_vpe, EP_DMA) != Errors::NONE)
        PANIC("Unable to activate DMA EP for proxied pci device: " << Errors::last);
}

void ProxiedPciDevice::setInterruptCallback(std::function<void()> callback) {
    _callback = callback;
}

void ProxiedPciDevice::receiveInterrupt(ProxiedPciDevice *nic, m3::GateIStream &) {
    if(nic->_callback)
        nic->_callback();
    else
        cout << "received interrupt, but no callback is registered.\n";
}

}
