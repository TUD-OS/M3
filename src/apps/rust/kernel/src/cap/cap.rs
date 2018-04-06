use base::cell::StaticCell;
use base::cfg;
use base::col::{KeyOrd, Treap};
use base::goff;
use base::kif::{CapRngDesc, CapSel};
use core::cmp;
use core::fmt;
use core::ptr::{NonNull, Unique};

use cap::KObject;
use com::ServiceList;
use pes::{vpemng, VPE};

#[derive(Copy, Clone)]
pub struct SelRange {
    start: CapSel,
    count: CapSel,
}

impl SelRange {
    pub fn new(sel: CapSel) -> Self {
        Self::new_range(sel, 1)
    }

    pub fn new_range(sel: CapSel, count: CapSel) -> Self {
        SelRange {
            start: sel,
            count: count,
        }
    }
}

impl fmt::Debug for SelRange {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.start)
    }
}

impl KeyOrd for SelRange {
    fn compare(&self, other: &Self) -> cmp::Ordering {
        if self.start >= other.start && self.start < other.start + other.count {
            cmp::Ordering::Equal
        }
        else if self.start < other.start {
            cmp::Ordering::Less
        }
        else {
            cmp::Ordering::Greater
        }
    }

    fn smaller(&self, other: &Self) -> bool {
        self.start < other.start
    }
}

pub struct CapTable {
    caps: Treap<SelRange, Capability>,
    vpe: Option<NonNull<VPE>>,
}

unsafe fn as_shared<T>(obj: &mut T) -> NonNull<T> {
    NonNull::from(Unique::new_unchecked(obj as *mut T))
}

impl CapTable {
    pub fn new() -> Self {
        CapTable {
            caps: Treap::new(),
            vpe: None,
        }
    }

    pub unsafe fn set_vpe(&mut self, vpe: *mut VPE) {
        self.vpe = Some(NonNull::from(Unique::new_unchecked(vpe)));
    }

    pub fn unused(&self, sel: CapSel) -> bool {
        self.get(sel).is_none()
    }
    pub fn range_unused(&self, crd: &CapRngDesc) -> bool {
        for s in crd.start()..crd.start() + crd.count() {
            if self.get(s).is_some() {
                return false;
            }
        }
        return true;
    }

    pub fn get(&self, sel: CapSel) -> Option<&Capability> {
        self.caps.get(&SelRange::new(sel))
    }
    pub fn get_mut(&mut self, sel: CapSel) -> Option<&mut Capability> {
        self.caps.get_mut(&SelRange::new(sel))
    }

    pub fn insert(&mut self, mut cap: Capability) -> &mut Capability {
        unsafe {
            cap.table = Some(as_shared(self));
        }
        self.caps.insert(cap.sel_range().clone(), cap)
    }
    pub fn insert_as_child(&mut self, cap: Capability, parent_sel: CapSel) {
        unsafe {
            let parent: Option<NonNull<Capability>> = self.get_shared(parent_sel);
            self.do_insert(cap, parent);
        }
    }
    pub fn insert_as_child_from(&mut self, cap: Capability, par_tbl: &mut CapTable, par_sel: CapSel) {
        unsafe {
            let parent: Option<NonNull<Capability>> = par_tbl.get_shared(par_sel);
            self.do_insert(cap, parent);
        }
    }

    unsafe fn get_shared(&mut self, sel: CapSel) -> Option<NonNull<Capability>> {
        self.caps.get_mut(&SelRange::new(sel)).map(|cap| NonNull::new_unchecked(cap))
    }
    unsafe fn do_insert(&mut self, child: Capability, parent: Option<NonNull<Capability>>) {
        let mut child_cap = self.insert(child);
        if let Some(parent_cap) = parent {
            (*parent_cap.as_ptr()).inherit(&mut child_cap);
        }
    }

    pub fn obtain(&mut self, sel: CapSel, cap: &mut Capability, child: bool) {
        let mut nc: Capability = (*cap).clone();
        nc.sels = SelRange::new(sel);
        if child {
            cap.inherit(self.insert(nc));
        }
        else {
            self.insert(nc).inherit(cap);
        }
    }

    pub fn revoke(&mut self, crd: CapRngDesc, own: bool) {
        for sel in crd.start()..crd.start() + crd.count() {
            self.get_mut(sel).map(|cap| {
                if own {
                    cap.revoke(false);
                }
                else {
                    unsafe {
                        cap.child.map(|child| (*child.as_ptr()).revoke(true));
                    }
                }
            });
        }
    }

    pub fn revoke_all(&mut self) {
        while let Some(cap) = self.caps.get_root_mut() {
            cap.revoke(false);
        }
    }
}

impl fmt::Debug for CapTable {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "CapTable[\n{:?}]", self.caps)
    }
}

#[derive(Clone)]
pub struct Capability {
    sels: SelRange,
    obj: KObject,
    table: Option<NonNull<CapTable>>,
    child: Option<NonNull<Capability>>,
    parent: Option<NonNull<Capability>>,
    next: Option<NonNull<Capability>>,
    prev: Option<NonNull<Capability>>,
}

impl Capability {
    pub fn new(sel: CapSel, obj: KObject) -> Self {
        Self::new_range(SelRange::new(sel), obj)
    }
    pub fn new_range(sels: SelRange, obj: KObject) -> Self {
        Capability {
            sels: sels,
            obj: obj,
            table: None,
            child: None,
            parent: None,
            next: None,
            prev: None,
        }
    }

    pub fn sel_range(&self) -> &SelRange {
        &self.sels
    }
    pub fn sel(&self) -> CapSel {
        self.sels.start
    }
    pub fn len(&self) -> CapSel {
        self.sels.count
    }

    pub fn get(&self) -> &KObject {
        &self.obj
    }
    pub fn get_mut(&mut self) -> &mut KObject {
        &mut self.obj
    }

    pub fn inherit(&mut self, child: &mut Capability) {
        unsafe {
            child.parent = Some(as_shared(self));
            child.child = None;
            child.next = self.child;
            child.prev = None;
            if let Some(n) = child.next {
                (*n.as_ptr()).prev = Some(as_shared(child));
            }
            self.child = Some(as_shared(child));

            if let KObject::Sess(ref mut k) = self.obj {
                k.borrow_mut().users += 1;
            }
        }
    }

    fn revoke(&mut self, rev_next: bool) {
        unsafe {
            if let Some(n) = self.next {
                (*n.as_ptr()).prev = self.prev;
            }
            if let Some(p) = self.prev {
                (*p.as_ptr()).next = self.next;
            }
            if let Some(p) = self.parent {
                if self.prev.is_none() {
                    let child = &mut (*p.as_ptr()).child;
                    *child = self.next;
                }
            }
            self.revoke_rec(rev_next);
        }
    }

    fn revoke_rec(&mut self, rev_next: bool) {
        self.release();

        unsafe {
            // remove it from the table
            let sels = SelRange::new(self.sel());
            let cap = self.table_mut().caps.remove(&sels).unwrap();

            if let Some(c) = cap.child {
                (*c.as_ptr()).revoke_rec(true);
            }
            // on the first level, we don't want to revoke siblings
            if rev_next {
                if let Some(n) = cap.next {
                    (*n.as_ptr()).revoke_rec(true);
                }
            }
        }
    }

    fn table(&self) -> &CapTable {
        unsafe { &*self.table.unwrap().as_ptr() }
    }
    fn table_mut(&mut self) -> &mut CapTable {
        unsafe { &mut *self.table.unwrap().as_ptr() }
    }

    fn vpe(&self) -> &VPE {
        unsafe { &*(self.table().vpe.unwrap().as_ptr()) }
    }
    fn vpe_mut(&mut self) -> &mut VPE {
        unsafe { &mut *(self.table_mut().vpe.unwrap().as_ptr()) }
    }

    fn invalidate_ep(&mut self, sel: CapSel) {
        let vpe = self.vpe_mut();
        if let Some(ep) = vpe.ep_with_sel(sel) {
            vpe.set_ep_sel(ep, None);
            // if that fails, just ignore it
            vpe.invalidate_ep(ep, false).ok();
        }
    }

    fn release(&mut self) {
        match self.obj {
            KObject::VPE(ref v) => {
                // remove VPE if we revoked the root capability
                if self.parent.is_none() && !v.borrow().is_bootmod() {
                    let id = v.borrow().id();
                    vpemng::get().remove(id);
                }
            },

            KObject::SGate(_) | KObject::RGate(_) | KObject::MGate(_) => {
                let sel = self.sel();
                self.invalidate_ep(sel)
            },

            KObject::Sess(ref s) => unsafe {
                let mut sess = s.borrow_mut();
                if let KObject::Serv(_) = (*self.parent.unwrap().as_ptr()).obj {
                    sess.invalid = true;
                }
                else {
                    sess.users -= 1;
                    if !sess.invalid && sess.users == 0 {
                        sess.close();
                    }
                }
            },

            KObject::Serv(_) => {
                let sel = self.sel();
                let vpe = self.vpe_mut();
                ServiceList::get().remove(vpe, sel);
            },

            KObject::Map(ref m) => {
                let virt = (self.sel() as goff) << cfg::PAGE_BITS;
                m.borrow().unmap(self.vpe(), virt, self.len() as usize);
            },

            _ => {
            },
        }
    }
}

fn print_childs(cap: NonNull<Capability>, f: &mut fmt::Formatter) -> fmt::Result {
    static LAYER: StaticCell<u32> = StaticCell::new(5);
    use core::fmt::Write;
    let mut next = Some(cap);
    loop {
        match next {
            None    => return Ok(()),
            Some(n) => unsafe {
                f.write_char('\n')?;
                for _ in 0..*LAYER {
                    f.write_char(' ')?;
                }
                LAYER.set(*LAYER + 1);
                write!(f, "=> {:?}", *n.as_ptr())?;
                LAYER.set(*LAYER - 1);

                next = (*n.as_ptr()).next;
            }
        }
    }
}

impl fmt::Debug for Capability {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Cap[vpe={}, sel={}, len={}, obj={:?}]",
            self.vpe().id(), self.sel(), self.len(), self.obj)?;
        if let Some(c) = self.child {
            print_childs(c, f)?;
        }
        Ok(())
    }
}
