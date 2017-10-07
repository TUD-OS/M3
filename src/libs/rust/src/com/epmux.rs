use cap::Flags;
use com::gate::{Gate, INVALID_EP};
use dtu::EpId;
use dtu;
use errors::Error;
use errors;
use kif;
use syscalls;
use vpe;

pub struct EpMux {
    gates: [Option<*mut Gate>; dtu::EP_COUNT],
    next_victim: usize,
}

static mut EP_MUX: EpMux = EpMux::new();

impl EpMux {
    const fn new() -> Self {
        EpMux {
            gates: [None; dtu::EP_COUNT],
            next_victim: 1,
        }
    }

    pub fn get() -> &'static mut EpMux {
        unsafe {
            &mut EP_MUX
        }
    }

    pub fn reserve(&mut self, ep: EpId) {
        // take care that some non-fixed gate could already use that endpoint
        if let Some(g) = self.gate_at_ep(ep) {
            syscalls::activate(0, kif::INVALID_SEL, g.ep, 0).ok();
            g.ep = INVALID_EP;
        }
        self.gates[ep] = None;
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
        g.ep = INVALID_EP;
        // only necessary if we won't revoke the gate anyway
        if !(g.cap.flags() & Flags::KEEP_CAP).is_empty() {
            syscalls::activate(0, kif::INVALID_SEL, g.ep, 0).ok();
        }
    }

    fn select_victim(&mut self) -> Result<EpId, Error> {
        let mut victim = self.next_victim;
        for _ in 0..dtu::EP_COUNT {
            if vpe::VPE::cur().is_ep_free(victim) {
                break;
            }

            victim = (victim + 1) % dtu::EP_COUNT;
        }

        if !vpe::VPE::cur().is_ep_free(victim) {
            Err(errors::Error::NoSpace)
        }
        else {
            if let Some(g) = self.gate_at_ep(victim) {
                g.ep = INVALID_EP;
            }
            self.next_victim = (victim + 1) % dtu::EP_COUNT;
            Ok(victim)
        }
    }

    fn gate_at_ep(&self, ep: EpId) -> Option<&mut Gate> {
        if let Some(g) = self.gates[ep] {
            Some(unsafe { &mut *g })
        }
        else {
            None
        }
    }
}
