/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

use arch::env;
use cfg;
use com::RecvGate;
use dtu;
use kif;
use syscalls;
use vpe;

pub fn init() {
    {
        let (ep, lbl, crd) = env::get().syscall_params();
        dtu::DTU::configure(dtu::SYSC_SEP, lbl, 0, ep, crd, cfg::SYSC_RBUF_ORD);
    }

    let sysc = RecvGate::syscall();
    dtu::DTU::configure_recv(dtu::SYSC_REP, sysc.buffer(), cfg::SYSC_RBUF_ORD, cfg::SYSC_RBUF_ORD);

    let upc = RecvGate::upcall();
    dtu::DTU::configure_recv(dtu::UPCALL_REP, upc.buffer(), cfg::UPCALL_RBUF_ORD, cfg::UPCALL_RBUF_ORD);

    let def = RecvGate::def();
    dtu::DTU::configure_recv(dtu::DEF_REP, def.buffer(), cfg::DEF_RBUF_ORD, cfg::DEF_RBUF_ORD);

    dtu::init();

    let eps = dtu::ep_regs_addr();
    syscalls::vpe_ctrl(vpe::VPE::cur().sel(), kif::syscalls::VPEOp::INIT, eps as u64).unwrap();
}

pub fn deinit() {
    dtu::deinit();
}
