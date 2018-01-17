use cap::{CapFlags, Capability, Selector};
use core::fmt;
use errors::Error;
use kif;
use syscalls;
use vpe;

pub struct Session {
    cap: Capability,
}

impl Session {
    pub fn new(name: &str, arg: u64) -> Result<Self, Error> {
        let sel = vpe::VPE::cur().alloc_sel();
        syscalls::create_sess(sel, name, arg)?;
        Ok(Session {
            cap: Capability::new(sel, CapFlags::empty()),
        })
    }

    pub fn new_bind(sel: Selector) -> Self {
        Session {
            cap: Capability::new(sel, CapFlags::KEEP_CAP),
        }
    }
    pub fn new_owned_bind(sel: Selector) -> Self {
        Session {
            cap: Capability::new(sel, CapFlags::empty()),
        }
    }

    pub fn sel(&self) -> Selector {
        self.cap.sel()
    }

    pub fn delegate_obj(&self, sel: Selector) -> Result<(), Error> {
        let crd = kif::CapRngDesc::new(kif::CapType::OBJECT, sel, 1);
        let sargs = [];
        let mut rargs = [];
        self.delegate(crd, &sargs, &mut rargs).map(|_| ())
    }

    pub fn delegate(&self, crd: kif::CapRngDesc, sargs: &[u64], rargs: &mut [u64]) -> Result<usize, Error> {
        syscalls::delegate(self.sel(), crd, sargs, rargs)
    }

    pub fn obtain_obj(&self) -> Result<Selector, Error> {
        self.obtain(1, &[], &mut []).map(|res| res.1.start())
    }

    pub fn obtain(&self, count: u32, sargs: &[u64], rargs: &mut [u64]) -> Result<(usize, kif::CapRngDesc), Error> {
        let caps = vpe::VPE::cur().alloc_sels(count);
        let crd = kif::CapRngDesc::new(kif::CapType::OBJECT, caps, count);
        let num = syscalls::obtain(self.sel(), crd, sargs, rargs)?;
        Ok((num, crd))
    }
}

impl fmt::Debug for Session {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(f, "Session[sel: {}]", self.sel())
    }
}
