use base::col::Treap;
use base::kif::{CapRngDesc, CapSel};
use core::fmt;
use core::ptr::{Shared, Unique};

use pes::VPE;
use cap::KObject;

pub struct CapTable {
    caps: Treap<CapSel, Capability>,
    vpe: Option<Shared<VPE>>,
}

unsafe fn as_shared<T>(obj: &mut T) -> Shared<T> {
    Shared::from(Unique::new_unchecked(obj as *mut T))
}

impl CapTable {
    pub fn new() -> Self {
        CapTable {
            caps: Treap::new(),
            vpe: None,
        }
    }

    pub unsafe fn set_vpe(&mut self, vpe: *mut VPE) {
        self.vpe = Some(Shared::from(Unique::new_unchecked(vpe)));
    }

    pub fn unused(&self, sel: CapSel) -> bool {
        self.get(sel).is_none()
    }

    pub fn get(&self, sel: CapSel) -> Option<&Capability> {
        self.caps.get(|k| sel.cmp(k))
    }
    pub fn get_mut(&mut self, sel: CapSel) -> Option<&mut Capability> {
        self.caps.get_mut(|k| sel.cmp(k))
    }
    pub unsafe fn get_shared(&mut self, sel: CapSel) -> Option<Shared<Capability>> {
        self.caps.get_mut(|k| sel.cmp(k)).map(|cap| Shared::new_unchecked(cap))
    }

    pub fn insert(&mut self, mut cap: Capability) -> &mut Capability {
        unsafe {
            cap.table = Some(as_shared(self));
        }
        self.caps.insert(cap.sel(), cap)
    }

    pub unsafe fn insert_as_child(&mut self, child: Capability, parent: Option<Shared<Capability>>) {
        let mut child_cap = self.insert(child);
        if let Some(parent_cap) = parent {
            (*parent_cap.as_ptr()).inherit(&mut child_cap);
        }
    }

    pub fn obtain(&mut self, sel: CapSel, cap: &mut Capability) {
        let mut nc: Capability = (*cap).clone();
        nc.sel = sel;
        cap.inherit(self.insert(nc));
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

    fn remove(&mut self, sel: CapSel) -> Option<Capability> {
        self.caps.remove(|k| sel.cmp(k))
    }
}

impl fmt::Debug for CapTable {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "CapTable[\n{:?}]", self.caps)
    }
}

#[derive(Clone)]
pub struct Capability {
    sel: CapSel,
    obj: KObject,
    table: Option<Shared<CapTable>>,
    child: Option<Shared<Capability>>,
    parent: Option<Shared<Capability>>,
    next: Option<Shared<Capability>>,
    prev: Option<Shared<Capability>>,
}

impl Capability {
    pub fn new(sel: CapSel, obj: KObject) -> Self {
        Capability {
            sel: sel,
            obj: obj,
            table: None,
            child: None,
            parent: None,
            next: None,
            prev: None,
        }
    }

    pub fn sel(&self) -> CapSel {
        self.sel
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
                let child = &mut (*p.as_ptr()).child;
                if child.unwrap().as_ptr() == self {
                    *child = self.next;
                }
            }
            self.revoke_rec(rev_next);
        }
    }

    fn revoke_rec(&mut self, rev_next: bool) {
        self.release();

        unsafe {
            let cap = (*self.table.unwrap().as_ptr()).remove(self.sel()).unwrap();

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

    fn release(&mut self) {
    }
}

fn print_childs(cap: Shared<Capability>, f: &mut fmt::Formatter, layer: u32) -> fmt::Result {
    use core::fmt::Write;
    let mut next = Some(cap);
    loop {
        match next {
            None    => return Ok(()),
            Some(n) => unsafe {
                f.write_char('\n')?;
                for _ in 0..layer {
                    f.write_char(' ')?;
                }
                write!(f, "=> {:?}", *n.as_ptr())?;
                if let Some(c) = (*n.as_ptr()).child {
                    print_childs(c, f, layer + 1)?;
                }
                next = (*n.as_ptr()).next;
            }
        }
    }
}

impl fmt::Debug for Capability {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Cap[sel={}, obj={:?}]", self.sel(), self.obj)?;
        if let Some(c) = self.child {
            print_childs(c, f, 5)?;
        }
        Ok(())
    }
}
