use core::ops;
use cap;
use cap::Capability;
use com::EpMux;
use dtu;
use errors::Error;

pub type EpId = dtu::EpId;

#[derive(Debug)]
pub struct Gate {
    pub cap: Capability,
    pub ep: Option<EpId>,
}

impl Gate {
    pub fn new(sel: cap::Selector, flags: cap::Flags) -> Self {
        Self::new_with_ep(sel, flags, None)
    }

    pub const fn new_with_ep(sel: cap::Selector, flags: cap::Flags, ep: Option<EpId>) -> Self {
        Gate {
            cap: Capability::new(sel, flags),
            ep: ep,
        }
    }

    pub fn rebind(&mut self, sel: cap::Selector) -> Result<(), Error> {
        try!(EpMux::get().switch_cap(self, sel));
        self.cap.rebind(sel);
        Ok(())
    }

    pub fn activate(&mut self) -> Result<EpId, Error> {
        match self.ep {
            Some(ep) => Ok(ep),
            None     => EpMux::get().switch_to(self),
        }
    }
}

impl ops::Drop for Gate {
    fn drop(&mut self) {
        if self.ep.is_some() {
            EpMux::get().remove(self);
        }
    }
}
