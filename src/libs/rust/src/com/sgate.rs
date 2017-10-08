use cap::{Flags, Selector};
use com::gate::Gate;
use com::RecvGate;
use dtu;
use dtu::EpId;
use errors::Error;
use kif::INVALID_SEL;
use syscalls;
use vpe;

pub struct SendGate {
    gate: Gate,
    reply_ep: EpId,
}

pub struct SGateArgs {
    rgate_sel: Selector,
    reply_ep: EpId,
    label: dtu::Label,
    credits: u64,
    sel: Selector,
    flags: Flags,
}

impl SGateArgs {
    pub fn new(rgate: &RecvGate) -> Self {
        SGateArgs {
            rgate_sel: rgate.sel(),
            label: 0,
            credits: 0,
            reply_ep: RecvGate::def().ep(),
            sel: INVALID_SEL,
            flags: Flags::empty(),
        }
    }

    pub fn reply_gate(mut self, reply_gate: &RecvGate) -> Self {
        self.reply_ep = reply_gate.ep();
        self
    }

    pub fn credits(mut self, credits: u64) -> Self {
        self.credits = credits;
        self
    }

    pub fn label(mut self, label: dtu::Label) -> Self {
        self.label = label;
        self
    }

    pub fn sel(mut self, sel: Selector) -> Self {
        self.sel = sel;
        self
    }
}

impl SendGate {
    pub fn new(rgate: &RecvGate) -> Result<Self, Error> {
        Self::new_with(SGateArgs::new(rgate))
    }

    pub fn new_with(args: SGateArgs) -> Result<Self, Error> {
        let sel = if args.sel == INVALID_SEL {
            vpe::VPE::cur().alloc_cap()
        }
        else {
            args.sel
        };

        try!(syscalls::create_sgate(sel, args.rgate_sel, args.label, args.credits));
        Ok(SendGate {
            gate: Gate::new(sel, args.flags),
            reply_ep: args.reply_ep,
        })
    }

    pub fn new_bind(sel: Selector, reply_gate: &RecvGate) -> Self {
        SendGate {
            gate: Gate::new(sel, Flags::KEEP_CAP),
            reply_ep: reply_gate.ep(),
        }
    }

    pub fn ep(&self) -> dtu::EpId {
        self.gate.ep
    }

    pub fn activate(&mut self) -> Result<(), Error> {
        self.gate.activate()
    }

    pub fn send<T>(&mut self, msg: &[T]) -> Result<(), Error> {
        try!(self.activate());
        dtu::DTU::send(self.gate.ep, msg, 0, self.reply_ep)
    }
}
