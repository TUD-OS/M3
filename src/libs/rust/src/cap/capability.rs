use core::ops;
use kif;
use syscalls;

pub type Selector = kif::cap::CapSel;

bitflags! {
    pub struct Flags : u32 {
        const KEEP_CAP   = 0x1;
    }
}

// TODO isn't there a better way?
impl Flags {
    pub const fn const_empty() -> Flags {
        Flags {
            bits: 0
        }
    }
}

#[derive(Debug)]
pub struct Capability {
    sel: Selector,
    flags: Flags,
}

impl Capability {
    pub const fn new(sel: Selector, flags: Flags) -> Capability {
        Capability {
            sel: sel,
            flags: flags,
        }
    }

    pub fn sel(&self) -> Selector {
        self.sel
    }
    pub fn flags(&self) -> Flags {
        self.flags
    }

    pub fn rebind(&mut self, sel: Selector) {
        self.release();
        self.sel = sel;
    }

    fn release(&mut self) {
        if (self.flags & Flags::KEEP_CAP).is_empty() {
            let crd = kif::cap::CapRngDesc::new_from(kif::cap::Type::OBJECT, self.sel, 1);
            syscalls::revoke(0, crd, true).ok();
        }
    }
}

impl ops::Drop for Capability {
    fn drop(&mut self) {
        self.release();
    }
}
