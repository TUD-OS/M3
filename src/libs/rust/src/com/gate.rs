use cell::Cell;
use core::ops;
use cap;
use cap::Capability;
use com::EpMux;
use dtu;
use errors::Error;

pub type EpId = dtu::EpId;

#[derive(Debug)]
pub struct Gate {
    cap: Capability,
    ep: Cell<Option<EpId>>,
}

impl Gate {
    pub fn new(sel: cap::Selector, flags: cap::Flags) -> Self {
        Self::new_with_ep(sel, flags, None)
    }

    pub const fn new_with_ep(sel: cap::Selector, flags: cap::Flags, ep: Option<EpId>) -> Self {
        Gate {
            cap: Capability::new(sel, flags),
            ep: Cell::new(ep),
        }
    }

    pub fn sel(&self) -> cap::Selector {
        self.cap.sel()
    }

    pub fn flags(&self) -> cap::Flags {
        self.cap.flags()
    }

    pub fn ep(&self) -> Option<EpId> {
        self.ep.get()
    }
    pub fn set_ep(&self, ep: EpId) {
        self.ep.set(Some(ep))
    }
    pub fn unset_ep(&self) {
        self.ep.set(None)
    }

    pub fn activate(&self) -> Result<EpId, Error> {
        match self.ep() {
            Some(ep) => Ok(ep),
            None     => EpMux::get().switch_to(self),
        }
    }

    pub fn rebind(&mut self, sel: cap::Selector) -> Result<(), Error> {
        EpMux::get().switch_cap(self, sel)?;
        self.cap.rebind(sel);
        Ok(())
    }
}

impl ops::Drop for Gate {
    fn drop(&mut self) {
        if self.ep().is_some() {
            EpMux::get().remove(self);
        }
    }
}
