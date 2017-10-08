use cap::{Capability, Flags, Selector};
use errors::Error;
use kif::{CapRngDesc, cap};
use syscalls;
use vpe;

pub struct Session {
    cap: Capability,
}

impl Session {
    pub fn new(name: &str, arg: u64) -> Result<Self, Error> {
        let sel = vpe::VPE::cur().alloc_cap();
        try!(syscalls::create_sess(sel, name, arg));
        Ok(Session {
            cap: Capability::new(sel, Flags::empty()),
        })
    }

    pub fn new_bind(sel: Selector) -> Self {
        Session {
            cap: Capability::new(sel, Flags::KEEP_CAP),
        }
    }

    pub fn sel(&self) -> Selector {
        self.cap.sel()
    }

    pub fn delegate_obj(&self, sel: Selector) -> Result<(), Error> {
        let crd = CapRngDesc::new_from(cap::Type::Object, sel, 1);
        let mut args = [];
        self.delegate(crd, &mut args)
    }

    pub fn delegate(&self, crd: CapRngDesc, args: &mut [u64]) -> Result<(), Error> {
        syscalls::delegate(self.sel(), crd, args)
    }

    pub fn obtain(&self, count: u32, args: &mut [u64]) -> Result<CapRngDesc, Error> {
        let caps = vpe::VPE::cur().alloc_caps(count);
        let crd = CapRngDesc::new_from(cap::Type::Object, caps, count);
        try!(syscalls::obtain(self.sel(), crd, args));
        Ok(crd)
    }
}
