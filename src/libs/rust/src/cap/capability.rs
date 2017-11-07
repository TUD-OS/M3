use core::ops;
use kif;
use syscalls;

pub type Selector = kif::CapSel;

bitflags! {
    pub struct CapFlags : u32 {
        const KEEP_CAP   = 0x1;
    }
}

// TODO isn't there a better way?
impl CapFlags {
    pub const fn const_empty() -> Self {
        CapFlags {
            bits: 0
        }
    }
}

#[derive(Debug)]
pub struct Capability {
    sel: Selector,
    flags: CapFlags,
}

impl Capability {
    pub const fn new(sel: Selector, flags: CapFlags) -> Self {
        Capability {
            sel: sel,
            flags: flags,
        }
    }

    pub fn sel(&self) -> Selector {
        self.sel
    }
    pub fn flags(&self) -> CapFlags {
        self.flags
    }
    pub fn set_flags(&mut self, flags: CapFlags) {
        self.flags = flags;
    }

    pub fn rebind(&mut self, sel: Selector) {
        self.release();
        self.sel = sel;
    }

    fn release(&mut self) {
        if (self.flags & CapFlags::KEEP_CAP).is_empty() {
            let crd = kif::CapRngDesc::new(kif::CapType::OBJECT, self.sel, 1);
            syscalls::revoke(0, crd, true).ok();
        }
    }
}

impl ops::Drop for Capability {
    fn drop(&mut self) {
        self.release();
    }
}
