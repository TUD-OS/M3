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

#include <base/util/Math.h>
#include <base/util/Sync.h>
#include <base/DTU.h>
#include <base/Init.h>
#include <base/KIF.h>

namespace m3 {

INIT_PRIO_DTU DTU DTU::inst;

Errors::Code DTU::send(int ep, const void *msg, size_t size, label_t replylbl, int reply_ep) {
    static_assert(KIF::Perm::R == DTU::R, "DTU::R does not match KIF::Perm::R");
    static_assert(KIF::Perm::W == DTU::W, "DTU::W does not match KIF::Perm::W");

    static_assert(KIF::Perm::R == DTU::PTE_R, "DTU::PTE_R does not match KIF::Perm::R");
    static_assert(KIF::Perm::W == DTU::PTE_W, "DTU::PTE_W does not match KIF::Perm::W");
    static_assert(KIF::Perm::X == DTU::PTE_X, "DTU::PTE_X does not match KIF::Perm::X");

    write_reg(CmdRegs::DATA_ADDR, reinterpret_cast<uintptr_t>(msg));
    write_reg(CmdRegs::DATA_SIZE, size);
    write_reg(CmdRegs::REPLY_LABEL, replylbl);
    write_reg(CmdRegs::REPLY_EP, reply_ep);
    Sync::compiler_barrier();
    write_reg(CmdRegs::COMMAND, buildCommand(ep, CmdOpCode::SEND));

    return get_error();
}

Errors::Code DTU::reply(int ep, const void *msg, size_t size, size_t) {
    write_reg(CmdRegs::DATA_ADDR, reinterpret_cast<uintptr_t>(msg));
    write_reg(CmdRegs::DATA_SIZE, size);
    Sync::compiler_barrier();
    write_reg(CmdRegs::COMMAND, buildCommand(ep, CmdOpCode::REPLY));

    return get_error();
}

Errors::Code DTU::transfer(reg_t cmd, uintptr_t data, size_t size, size_t off) {
    size_t left = size;
    while(left > 0) {
        size_t amount = Math::min<size_t>(left, MAX_PKT_SIZE);
        write_reg(CmdRegs::DATA_ADDR, data);
        write_reg(CmdRegs::DATA_SIZE, amount);
        write_reg(CmdRegs::OFFSET, off);
        Sync::compiler_barrier();
        write_reg(CmdRegs::COMMAND, cmd);

        left -= amount;
        data += amount;
        off += amount;

        Errors::Code res = get_error();
        if(EXPECT_FALSE(res != Errors::NO_ERROR))
            return res;
    }
    return Errors::NO_ERROR;
}

Errors::Code DTU::read(int ep, void *data, size_t size, size_t off, uint flags) {
    uintptr_t dataaddr = reinterpret_cast<uintptr_t>(data);
    reg_t cmd = buildCommand(ep, CmdOpCode::READ, flags);
    return transfer(cmd, dataaddr, size, off);
}

Errors::Code DTU::write(int ep, const void *data, size_t size, size_t off, uint flags) {
    uintptr_t dataaddr = reinterpret_cast<uintptr_t>(data);
    reg_t cmd = buildCommand(ep, CmdOpCode::WRITE, flags);
    return transfer(cmd, dataaddr, size, off);
}

}
