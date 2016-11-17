/**
 * Copyright (C) 2015-2016, René Küttner <rene.kuettner@.tu-dresden.de>
 * Economic rights: Technische Universität Dresden (Germany)
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

#include <base/util/Math.h>
#include <base/CPU.h>
#include <base/DTU.h>

#include <m3/com/MemGate.h>

// this is mostly taken from libm3 (arch/gem5/DTU.cc)

namespace m3 {

DTU m3::DTU::inst;

/* Re-implement the necessary DTU methods we need. */

Errors::Code DTU::transfer(reg_t cmd, uintptr_t data, size_t size, size_t off) {
    size_t left = size;
    while(left > 0) {
        size_t amount = Math::min<size_t>(left, MAX_PKT_SIZE);
        write_reg(CmdRegs::DATA_ADDR, data);
        write_reg(CmdRegs::DATA_SIZE, amount);
        write_reg(CmdRegs::OFFSET, off);
        CPU::compiler_barrier();
        write_reg(CmdRegs::COMMAND, cmd);

        left -= amount;
        data += amount;
        off += amount;

        Errors::Code res = get_error();
        if(EXPECT_FALSE(res != Errors::NONE))
            return res;
    }
    return Errors::NONE;
}

Errors::Code DTU::read(epid_t ep, void *data, size_t size, size_t off, uint flags) {
    uintptr_t dataaddr = reinterpret_cast<uintptr_t>(data);
    reg_t cmd = buildCommand(ep, CmdOpCode::READ, flags);
    return transfer(cmd, dataaddr, size, off);
}

Errors::Code DTU::write(epid_t ep, const void *data, size_t size, size_t off, uint flags) {
    uintptr_t dataaddr = reinterpret_cast<uintptr_t>(data);
    reg_t cmd = buildCommand(ep, CmdOpCode::WRITE, flags);
    return transfer(cmd, dataaddr, size, off);
}

}
