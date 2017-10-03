use cap::Flags;
use com::gate::{Gate, INVALID_EP};
use dtu::EpId;
use dtu;
use errors::Error;
use errors;
use kif;
use syscalls;

pub struct EpMux {
    gates: [Option<*mut Gate>; dtu::EP_COUNT],
    next_victim: usize,
    free: u64,
}

static mut EP_MUX: EpMux = EpMux::new();

impl EpMux {
    const fn new() -> EpMux {
        EpMux {
            gates: [None; dtu::EP_COUNT],
            next_victim: 1,
            free: !0,
        }
    }

    pub fn get() -> &'static mut EpMux {
        unsafe {
            &mut EP_MUX
        }
    }

    pub fn reserve(&mut self, ep: EpId) {
        self.free &= !(1 << ep);
    }

    pub fn switch_to(&mut self, g: &mut Gate) -> Result<EpId, Error> {
        let idx = try!(self.select_victim());
        try!(syscalls::activate(0, g.cap.sel(), idx, 0));
        self.gates[idx] = Some(g);
        g.ep = idx;
        Ok(idx)
    }

    pub fn remove(&mut self, g: &mut Gate) {
        self.gates[g.ep] = None;
        self.free |= 1 << g.ep;
        g.ep = INVALID_EP;
        // only necessary if we won't revoke the gate anyway
        if !(g.cap.flags() & Flags::KEEP_CAP).is_empty() {
            syscalls::activate(0, kif::INVALID_SEL, g.ep, 0).ok();
        }
    }

    fn select_victim(&mut self) -> Result<EpId, Error> {
        let mut victim = self.next_victim;
        for _ in 0..dtu::EP_COUNT {
            if (self.free & (1 << victim)) != 0 {
                break;
            }

            victim = (victim + 1) % dtu::EP_COUNT;
        }

        if (self.free & (1 << victim)) == 0 {
            Err(errors::Error::NoSpace)
        }
        else {
            if let Some(v) = self.gates[victim] {
                unsafe { (*v).ep = INVALID_EP };
            }
            self.next_victim = (victim + 1) % dtu::EP_COUNT;
            Ok(victim)
        }
    }
}
