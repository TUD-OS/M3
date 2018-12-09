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

use cap::Selector;
use cell::RefCell;
use com::MemGate;
use errors::Error;
use kif;
use rc::Rc;
use session::ClientSession;
use vfs::{FileHandle, GenericFile};

pub struct Pipe {
    sess: ClientSession,
}

impl Pipe {
    pub fn new(name: &str, mem: &MemGate, mem_size: usize) -> Result<Self, Error> {
        let sess = ClientSession::new(name, mem_size as u64)?;
        sess.delegate_obj(mem.sel())?;
        Ok(Pipe {
            sess: sess,
        })
    }

    pub fn sel(&self) -> Selector {
        self.sess.sel()
    }

    pub fn create_chan(&self, read: bool) -> Result<FileHandle, Error> {
        let mut args = kif::syscalls::ExchangeArgs {
            count: 1,
            vals: kif::syscalls::ExchangeUnion {
                i: [read as u64, 0, 0, 0, 0, 0, 0, 0]
            },
        };
        let crd = self.sess.obtain(2, &mut args)?;
        Ok(Rc::new(RefCell::new(GenericFile::new(crd.start()))))
    }
}
