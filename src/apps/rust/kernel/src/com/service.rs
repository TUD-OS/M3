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
use base::col::{DList, String};
use base::kif::CapSel;
use base::rc::Rc;

use cap::{KObject, ServObject};
use pes::VPE;

pub struct Service {
    vpe: Rc<RefCell<VPE>>,
    sel: CapSel,
    name: String,
}

impl Service {
    fn new(name: String, vpe: &Rc<RefCell<VPE>>, sel: CapSel) -> Self {
        Service {
            vpe: vpe.clone(),
            sel: sel,
            name: name,
        }
    }

    pub fn vpe(&self) -> &Rc<RefCell<VPE>> {
        &self.vpe
    }

    pub fn get_kobj(&self) -> Rc<RefCell<ServObject>> {
        self.vpe.borrow().obj_caps().get(self.sel).and_then(|cap| match cap.get() {
            &KObject::Serv(ref s) => Some(s.clone()),
            _                     => None,
        }).unwrap()
    }
}

pub struct ServiceList {
    list: DList<Service>,
}

static SERVICES: StaticCell<Option<ServiceList>> = StaticCell::new(None);

pub fn init() {
    SERVICES.set(Some(ServiceList::new()));
}

impl ServiceList {
    fn new() -> Self {
        ServiceList {
            list: DList::new(),
        }
    }

    pub fn get() -> &'static mut Self {
        SERVICES.get_mut().as_mut().unwrap()
    }

    pub fn add(&mut self, name: String, vpe: &Rc<RefCell<VPE>>, sel: CapSel) {
        klog!(SERV, "Added service {} @ [VPE={}, sel={}]", name, vpe.borrow().id(), sel);
        self.list.push_back(Service::new(name, vpe, sel));
    }

    pub fn find(&self, name: &str) -> Option<&Service> {
        self.list.iter().find(|s| s.name == *name)
    }

    pub fn remove(&mut self, vpe: &mut VPE, sel: CapSel) {
        let mut it = self.list.iter_mut();
        while let Some(s) = it.next() {
            if s.vpe.as_ptr() == vpe && s.sel == sel {
                klog!(SERV, "Removed service {} @ [VPE={}, sel={}]", s.name, vpe.id(), sel);
                it.remove();
                break;
            }
        }
    }
}
