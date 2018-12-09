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

use cap::{CapFlags, Capability, Selector};
use core::fmt;
use errors::Error;
use kif;
use syscalls;
use vpe;

pub struct ClientSession {
    cap: Capability,
}

impl ClientSession {
    pub fn new(name: &str, arg: u64) -> Result<Self, Error> {
        Self::new_with_sel(name, arg, vpe::VPE::cur().alloc_sel())
    }
    pub fn new_with_sel(name: &str, arg: u64, sel: Selector) -> Result<Self, Error> {
        syscalls::open_sess(sel, name, arg)?;
        Ok(ClientSession {
            cap: Capability::new(sel, CapFlags::empty()),
        })
    }

    pub fn new_bind(sel: Selector) -> Self {
        ClientSession {
            cap: Capability::new(sel, CapFlags::KEEP_CAP),
        }
    }
    pub fn new_owned_bind(sel: Selector) -> Self {
        ClientSession {
            cap: Capability::new(sel, CapFlags::empty()),
        }
    }

    pub fn sel(&self) -> Selector {
        self.cap.sel()
    }

    pub fn delegate_obj(&self, sel: Selector) -> Result<(), Error> {
        let crd = kif::CapRngDesc::new(kif::CapType::OBJECT, sel, 1);
        self.delegate_crd(crd)
    }

    pub fn delegate_crd(&self, crd: kif::CapRngDesc) -> Result<(), Error> {
        let mut args = kif::syscalls::ExchangeArgs::default();
        self.delegate(crd, &mut args)
    }

    pub fn delegate(&self, crd: kif::CapRngDesc,
                    args: &mut kif::syscalls::ExchangeArgs) -> Result<(), Error> {
        self.delegate_for(vpe::VPE::cur().sel(), crd, args)
    }

    pub fn delegate_for(&self, vpe: Selector, crd: kif::CapRngDesc,
                        args: &mut kif::syscalls::ExchangeArgs) -> Result<(), Error> {
        syscalls::delegate(vpe, self.sel(), crd, args)
    }

    pub fn obtain_obj(&self) -> Result<Selector, Error> {
        self.obtain_crd(1).map(|res| res.start())
    }

    pub fn obtain_crd(&self, count: u32) -> Result<kif::CapRngDesc, Error> {
        let mut args = kif::syscalls::ExchangeArgs::default();
        self.obtain(count, &mut args)
    }

    pub fn obtain(&self, count: u32,
                  args: &mut kif::syscalls::ExchangeArgs) -> Result<kif::CapRngDesc, Error> {
        let caps = vpe::VPE::cur().alloc_sels(count);
        let crd = kif::CapRngDesc::new(kif::CapType::OBJECT, caps, count);
        self.obtain_for(vpe::VPE::cur().sel(), crd, args)?;
        Ok(crd)
    }

    pub fn obtain_for(&self, vpe: Selector, crd: kif::CapRngDesc,
                      args: &mut kif::syscalls::ExchangeArgs) -> Result<(), Error> {
        syscalls::obtain(vpe, self.sel(), crd, args)
    }
}

impl fmt::Debug for ClientSession {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(f, "ClientSession[sel: {}]", self.sel())
    }
}
