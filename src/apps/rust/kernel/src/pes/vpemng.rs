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

use base::cell::{StaticCell, RefCell};
use base::col::{DList, String, ToString, Vec};
use base::dtu::PEId;
use base::env;
use base::errors::{Code, Error};
use base::kif;
use base::rc::Rc;

use arch::kdtu::KDTU;
use arch::vm;
use com::ServiceList;
use pes::{VPE, VPEId, VPEFlags};
use pes::pemng;
use platform;

pub const MAX_VPES: usize   = 1024;
pub const KERNEL_VPE: usize = MAX_VPES;

pub struct VPEMng {
    vpes: Vec<Option<Rc<RefCell<VPE>>>>,
    pending: DList<VPEId>,
    count: usize,
    daemons: usize,
    next_id: usize,
}

static INST: StaticCell<Option<VPEMng>> = StaticCell::new(None);

pub fn get() -> &'static mut VPEMng {
    INST.get_mut().as_mut().unwrap()
}

pub fn init() {
    INST.set(Some(VPEMng {
        vpes: vec![None; MAX_VPES],
        pending: DList::new(),
        count: 0,
        daemons: 0,
        next_id: 0,
    }));
}

pub fn deinit() {
    INST.set(None);
}

impl VPEMng {
    pub fn count(&self) -> usize {
        self.count
    }
    pub fn daemons(&self) -> usize {
        self.daemons
    }

    pub fn vpe(&self, id: VPEId) -> Option<Rc<RefCell<VPE>>> {
        self.vpes[id].as_ref().map(|v| v.clone())
    }

    pub fn pe_of(&self, id: VPEId) -> Option<PEId> {
        if id == KERNEL_VPE {
            Some(platform::kernel_pe())
        }
        else {
            self.vpe(id).map(|v| v.borrow().pe_id())
        }
    }

    pub fn get_id(&mut self) -> Result<usize, Error> {
        for id in self.next_id..MAX_VPES {
            if self.vpes[id].is_none() {
                self.next_id = id + 1;
                return Ok(id)
            }
        }

        for id in 0..self.next_id {
            if self.vpes[id].is_none() {
                self.next_id = id + 1;
                return Ok(id)
            }
        }

        Err(Error::new(Code::NoSpace))
    }

    pub fn create(&mut self, name: &str, pedesc: &kif::PEDesc,
                  addr_space: Option<vm::AddrSpace>, muxable: bool) -> Result<Rc<RefCell<VPE>>, Error> {
        let id: VPEId = self.get_id()?;
        let pe_id: PEId = pemng::get().alloc_pe(pedesc, None, muxable).ok_or(Error::new(Code::NoFreePE))?;
        let vpe: Rc<RefCell<VPE>> = VPE::new(name, id, pe_id, VPEFlags::empty(), addr_space);

        klog!(VPES, "Created VPE {} [id={}, pe={}]", name, id, pe_id);

        let res = vpe.clone();
        self.vpes[id] = Some(vpe);
        self.count += 1;
        Ok(res)
    }

    pub fn start(&mut self, args: env::Args) -> Result<(), Error> {
        // TODO temporary
        let isa = platform::pe_desc(platform::kernel_pe()).isa();
        let pe_imem = kif::PEDesc::new(kif::PEType::COMP_IMEM, isa, 0);
        let pe_emem = kif::PEDesc::new(kif::PEType::COMP_EMEM, isa, 0);
        let find_pe = || {
            if let Some(pe_id) = pemng::get().alloc_pe(&pe_emem, None, false) {
                return Ok(pe_id)
            }
            pemng::get().alloc_pe(&pe_imem, None, false).ok_or(Error::new(Code::NoFreePE))
        };

        let mut argv = Vec::new();
        for arg in args {
            argv.push(arg.to_string());
        }

        let mut i = 0;
        while i < argv.len() {
            let arg: &String = &argv[i];
            if arg == "--" {
                i += 1;
                continue;
            }

            let id: VPEId = self.get_id()?;
            let pe_id: PEId = find_pe()?;

            let addr_space = if platform::pe_desc(pe_id).has_virtmem() {
                Some(vm::AddrSpace::new(&platform::pe_desc(pe_id))?)
            }
            else {
                None
            };

            let vpe: Rc<RefCell<VPE>> = VPE::new(
                Self::path_to_name(&arg), id, pe_id, VPEFlags::BOOTMOD, addr_space
            );
            klog!(VPES, "Created VPE {} [id={}, pe={}]", &arg, id, pe_id);

            // find end of arguments
            let mut karg = false;
            vpe.borrow_mut().add_arg(&argv[i]);
            for j in i + 1..argv.len() {
                if argv[j] == "daemon" {
                    vpe.borrow_mut().make_daemon();
                    self.daemons += 1;
                    karg = true;
                }
                else if argv[j].starts_with("requires=") {
                    let req = &argv[j]["requires=".len()..];
                    if cfg!(target_os = "none") || req != "pager" {
                        vpe.borrow_mut().add_requirement(req);
                        karg = true;
                    }
                }
                else if argv[j] == "--" {
                    break;
                }
                else if karg {
                    panic!("Kernel argument before program argument");
                }
                else {
                    vpe.borrow_mut().add_arg(&argv[j]);
                }

                i += 1;
            }

            if vpe.borrow().requirements().len() > 0 {
                self.pending.push_back(id);
            }
            else {
                vpe.borrow_mut().start(0)?;
            }

            self.vpes[id] = Some(vpe);
            self.count += 1;

            i += 1;
        }

        Ok(())
    }

    pub fn start_pending(&mut self) {
        let mut it = self.pending.iter_mut();
        while let Some(id) = it.next() {
            let vpe: &Rc<RefCell<VPE>> = self.vpes[*id as usize].as_ref().unwrap();
            let mut fullfilled = true;
            for r in vpe.borrow().requirements() {
                if ServiceList::get().find(r).is_none() {
                    fullfilled = false;
                    break;
                }
            }

            if fullfilled {
                vpe.borrow_mut().start(0).unwrap();

                it.remove();
            }
        }
    }

    pub fn remove(&mut self, id: VPEId) {
        match self.vpes[id] {
            Some(ref v) => unsafe {
                let vpe: &mut VPE = &mut *v.as_ptr();
                Self::destroy_vpe(vpe);
                if !vpe.is_daemon() {
                    self.count -= 1;
                }
            },
            None        => panic!("Removing nonexisting VPE with id {}", id),
        };
        self.vpes[id] = None;
    }

    #[cfg(target_os = "linux")]
    pub fn pid_to_vpeid(&mut self, pid: i32) -> Option<VPEId> {
        for v in &self.vpes {
            if let Some(vpe) = v.as_ref() {
                if vpe.borrow().pid() == pid {
                    return Some(vpe.borrow().id());
                }
            }
        }
        None
    }

    fn destroy_vpe(vpe: &mut VPE) {
        vpe.destroy();
        // TODO temporary
        KDTU::get().reset(&vpe.desc()).unwrap();
        pemng::get().free(vpe.pe_id());
        klog!(
            VPES, "Removed VPE {} [id={}, pe={}]",
            vpe.name(), vpe.id(), vpe.pe_id()
        );
    }

    fn path_to_name(path: &str) -> &str {
        match path.bytes().rposition(|c| c == b'/') {
            Some(p) => &path[p + 1..],
            None    => path,
        }
    }
}

impl Drop for VPEMng {
    fn drop(&mut self) {
        for v in self.vpes.drain(0..) {
            if let Some(vpe) = v {
                unsafe {
                    let vpe: &mut VPE = &mut *vpe.as_ptr();

                    #[cfg(target_os = "linux")]
                    ::arch::loader::kill_child(vpe.pid());

                    Self::destroy_vpe(vpe);
                }
            }
        }
        // TODO workaround for compiler bug (?); without that, heap_free(0x40) gets called!??
        self.pending.push_back(1);
    }
}
