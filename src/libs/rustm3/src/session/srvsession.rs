use cap::{CapFlags, Capability, Selector};
use core::fmt;
use errors::Error;
use syscalls;
use vpe;

pub struct ServerSession {
    cap: Capability,
}

impl ServerSession {
    pub fn new(srv: Selector, ident: u64) -> Result<Self, Error> {
        let sel = vpe::VPE::cur().alloc_sel();
        syscalls::create_sess(sel, srv, ident)?;
        Ok(ServerSession {
            cap: Capability::new(sel, CapFlags::empty()),
        })
    }

    pub fn sel(&self) -> Selector {
        self.cap.sel()
    }
}

impl fmt::Debug for ServerSession {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(f, "ServerSession[sel: {}]", self.sel())
    }
}
