use cap::{CapFlags, Capability, Selector};
use core::{intrinsics, fmt};
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
        self.delegate_crd(crd)
    }

    pub fn delegate_crd(&self, crd: kif::CapRngDesc) -> Result<(), Error> {
        let mut args = kif::syscalls::ExchangeArgs {
            count: 0,
            vals: unsafe { intrinsics::uninit() },
        };
        self.delegate(crd, &mut args)
    }

    pub fn delegate(&self, crd: kif::CapRngDesc,
                    args: &mut kif::syscalls::ExchangeArgs) -> Result<(), Error> {
        self.delegate_for(vpe::VPE::cur(), crd, args)
    }

    pub fn delegate_for(&self, vpe: &vpe::VPE, crd: kif::CapRngDesc,
                        args: &mut kif::syscalls::ExchangeArgs) -> Result<(), Error> {
        syscalls::delegate(vpe.sel(), self.sel(), crd, args)
    }

    pub fn obtain_obj(&self) -> Result<Selector, Error> {
        self.obtain_crd(1).map(|res| res.start())
    }

    pub fn obtain_crd(&self, count: u32) -> Result<kif::CapRngDesc, Error> {
        let mut args = kif::syscalls::ExchangeArgs {
            count: 0,
            vals: unsafe { intrinsics::uninit() },
        };
        self.obtain(count, &mut args)
    }

    pub fn obtain(&self, count: u32,
                  args: &mut kif::syscalls::ExchangeArgs) -> Result<kif::CapRngDesc, Error> {
        self.obtain_for(vpe::VPE::cur(), count, args)
    }

    pub fn obtain_for(&self, vpe: &vpe::VPE, count: u32,
                      args: &mut kif::syscalls::ExchangeArgs) -> Result<kif::CapRngDesc, Error> {
        let caps = vpe::VPE::cur().alloc_sels(count);
        let crd = kif::CapRngDesc::new(kif::CapType::OBJECT, caps, count);
        syscalls::obtain(vpe.sel(), self.sel(), crd, args)?;
        Ok(crd)
    }
}

impl fmt::Debug for Session {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(f, "Session[sel: {}]", self.sel())
    }
}
