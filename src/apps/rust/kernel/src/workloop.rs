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

use base::dtu;
use core::intrinsics;
use thread;

use arch::kdtu;
use com;
use pes;
use syscalls;

pub fn workloop() {
    let thmng = thread::ThreadManager::get();
    let vpemng = pes::vpemng::get();

    while vpemng.count() > vpemng.daemons() {
        dtu::DTU::try_sleep(false, 0).unwrap();

        if let Some(msg) = dtu::DTU::fetch_msg(kdtu::KSYS_EP) {
            syscalls::handle(msg);
        }

        if let Some(msg) = dtu::DTU::fetch_msg(kdtu::KSRV_EP) {
            unsafe {
                let squeue: *mut com::SendQueue = intrinsics::transmute(msg.header.label as usize);
                (*squeue).received_reply(msg);
            }
        }

        if thmng.ready_count() > 0 {
            thmng.try_yield();
        }

        #[cfg(target_os = "linux")]
        ::arch::loader::check_childs();
    }
}
