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

Errors::Code DTU::send(epid_t ep, const void *msg, size_t size, label_t replylbl, epid_t reply_ep) {
    write_reg(CmdRegs::DATA, reinterpret_cast<reg_t>(msg) | (static_cast<reg_t>(size) << 48));
    if(replylbl)
        write_reg(CmdRegs::REPLY_LABEL, replylbl);
    CPU::compiler_barrier();
    write_reg(CmdRegs::COMMAND, buildCommand(ep, CmdOpCode::SEND, 0, reply_ep));

    return get_error();
}

Errors::Code DTU::read(epid_t ep, void *data, size_t size, goff_t off, uint flags) {
    write_reg(CmdRegs::DATA, reinterpret_cast<reg_t>(data) | (static_cast<reg_t>(size) << 48));
    CPU::compiler_barrier();
    write_reg(CmdRegs::COMMAND, buildCommand(ep, CmdOpCode::READ, flags, off));

    Errors::Code res = get_error();
    CPU::memory_barrier();
    return res;
}

Errors::Code DTU::write(epid_t ep, const void *data, size_t size, goff_t off, uint flags) {
    write_reg(CmdRegs::DATA, reinterpret_cast<reg_t>(data) | (static_cast<reg_t>(size) << 48));
    CPU::compiler_barrier();
    write_reg(CmdRegs::COMMAND, buildCommand(ep, CmdOpCode::WRITE, flags, off));

    return get_error();
}

}
