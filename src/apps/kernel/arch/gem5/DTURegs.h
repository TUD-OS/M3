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

#include <base/Common.h>
#include <base/DTU.h>

namespace kernel {

class DTURegs {
public:
    explicit DTURegs() : _dtu(), _cmd(), _eps(), _header() {
    }

    m3::DTU::reg_t get(m3::DTU::DtuRegs reg) const {
        return _dtu[static_cast<size_t>(reg)];
    }
    void set(m3::DTU::DtuRegs reg, m3::DTU::reg_t value) {
        _dtu[static_cast<size_t>(reg)] = value;
    }

    m3::DTU::reg_t _dtu[m3::DTU::DTU_REGS];
    m3::DTU::reg_t _cmd[m3::DTU::CMD_REGS];
    m3::DTU::reg_t _eps[m3::DTU::EP_REGS * EP_COUNT];
    m3::DTU::ReplyHeader _header[m3::DTU::HEADER_COUNT];
} PACKED;

}
