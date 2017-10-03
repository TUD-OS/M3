use cap::SelSpace;
use core::ops;
use kif;
use syscalls;

pub type Selector = kif::cap::CapSel;

bitflags! {
    pub struct Flags : u32 {
        const KEEP_SEL   = 0x1;
        const KEEP_CAP   = 0x2;
    }
}

pub struct Capability {
    sel: Selector,
    flags: Flags,
}

impl Capability {
    pub fn new(sel: Selector, flags: Flags) -> Capability {
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
}

impl ops::Drop for Capability {
    fn drop(&mut self) {
        if (self.flags & Flags::KEEP_SEL).is_empty() {
            SelSpace::get().free(self.sel);
        }

        if (self.flags & Flags::KEEP_CAP).is_empty() {
            let crd = kif::cap::CapRngDesc::new_from(kif::cap::Type::Object, self.sel, 1);
            syscalls::revoke(0, crd, true).ok();
        }
    }
}
