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

#pragma once

#include <base/PEDesc.h>
#include <m3/VPE.h>
#include <m3/com/RecvGate.h>
#include <m3/com/SendGate.h>
#include <m3/com/MemGate.h>

namespace net {

class ProxiedPciDevice {
public:
    static const uint EP_INT        = 7;
    static const uint EP_DMA        = 8;

    // Hardcoded for now
    static const size_t REG_SIZE    = 128 * 1024;
    static const size_t REG_ADDR    = 0x4000;

    explicit ProxiedPciDevice(m3::PEISA isa);

    uint32_t readReg(size_t offset);
    void writeReg(size_t offset, uint32_t value);

    void setDmaEp(m3::MemGate &memgate);

    void setInterruptCallback(std::function<void()> callback);

    /**
     * @return the VPE for the proxied pci device
     */
    m3::VPE &vpe() {
        return _vpe;
    }

private:
    static void receiveInterrupt(ProxiedPciDevice *nic, m3::GateIStream &is);

    m3::VPE _vpe;
    m3::RecvGate _intgate;  // receives interrupts from the proxied pci device
    m3::SendGate _sintgate; // used by the proxied pci device to signal interrupts to its driver
    std::function<void()> _callback;
};

}
