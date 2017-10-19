use cap::{Capability, Flags, Selector};
use com::{MemGate, RBufSpace};
use dtu::{EP_COUNT, FIRST_FREE_EP, EpId};
use env;
use errors::Error;
use kif::{cap, CapRngDesc, PEDesc};
use syscalls;
use util;

static mut CUR: Option<VPE> = None;

#[derive(Debug)]
pub struct VPE {
    cap: Capability,
    pe: PEDesc,
    mem: MemGate,
    next_sel: Selector,
    eps: u64,
    rbufs: RBufSpace,
}

impl VPE {
    fn new_cur() -> Self {
        VPE {
            cap: Capability::new(0, Flags::KEEP_CAP),
            pe: env::data().pedesc.clone(),
            mem: MemGate::new_bind(1),
            // 0 and 1 are reserved for VPE cap and mem cap
            next_sel: 2,
            eps: 0,
            rbufs: RBufSpace::new(env::data().rbuf_cur as usize, env::data().rbuf_end as usize),
        }
    }

    pub fn cur() -> &'static mut VPE {
        unsafe {
            CUR.as_mut().unwrap()
        }
    }

    pub fn sel(&self) -> Selector {
        self.cap.sel()
    }
    pub fn pe(&self) -> PEDesc {
        self.pe
    }
    pub fn mem(&self) -> &MemGate {
        &self.mem
    }

    pub fn alloc_cap(&mut self) -> Selector {
        self.alloc_caps(1)
    }
    pub fn alloc_caps(&mut self, count: u32) -> Selector {
        self.next_sel += count;
        self.next_sel - count
    }

    pub fn alloc_ep(&mut self) -> Result<EpId, Error> {
        for ep in 0..EP_COUNT {
            if self.is_ep_free(ep) {
                self.eps |= 1 << ep;
                return Ok(ep)
            }
        }
        Err(Error::NoSpace)
    }

    pub fn is_ep_free(&self, ep: EpId) -> bool {
        (self.eps & (1 << ep)) == 0
    }

    pub fn free_ep(&mut self, ep: EpId) {
        self.eps &= !(1 << ep);
    }

    pub fn rbufs(&mut self) -> &mut RBufSpace {
        &mut self.rbufs
    }

    pub fn delegate_obj(&mut self, sel: Selector) -> Result<(), Error> {
        self.delegate(CapRngDesc::new_from(cap::Type::OBJECT, sel, 1))
    }
    pub fn delegate(&mut self, crd: CapRngDesc) -> Result<(), Error> {
        let start = crd.start();
        self.delegate_to(crd, start)
    }
    pub fn delegate_to(&mut self, crd: CapRngDesc, dst: Selector) -> Result<(), Error> {
        try!(syscalls::exchange(self.sel(), crd, dst, false));
        self.next_sel = util::max(self.next_sel, dst + crd.count());
        Ok(())
    }

    pub fn obtain(&mut self, crd: CapRngDesc) -> Result<(), Error> {
        let count = crd.count();
        let start = self.alloc_caps(count);
        self.obtain_to(crd, start)
    }

    pub fn obtain_to(&self, crd: CapRngDesc, dst: Selector) -> Result<(), Error> {
        let own = CapRngDesc::new_from(crd.cap_type(), dst, crd.count());
        syscalls::exchange(self.sel(), own, crd.start(), true)
    }

    pub fn revoke(&self, crd: CapRngDesc, del_only: bool) -> Result<(), Error> {
        syscalls::revoke(self.sel(), crd, !del_only)
    }

    pub fn start(&self) -> Result<(), Error> {
        unimplemented!();
    }
    pub fn stop(&self) -> Result<(), Error> {
        unimplemented!();
    }
    pub fn wait(&self) -> Result<i32, Error> {
        unimplemented!();
    }
}

pub fn init() {
    unsafe {
        CUR = Some(VPE::new_cur());
    }

    for _ in 0..FIRST_FREE_EP {
        VPE::cur().alloc_ep().unwrap();
    }
}
