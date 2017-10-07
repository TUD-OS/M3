use cap::{Flags, Selector};
use com::gate::Gate;
use com::RecvGate;
use dtu;
use errors::Error;
use kif::INVALID_SEL;
use syscalls;
use vpe;

pub struct SendGate<'a> {
    gate: Gate,
    reply_gate: &'a RecvGate<'a>,
}

pub struct SGateArgs<'a> {
    rgate: &'a RecvGate<'a>,
    reply_gate: &'a RecvGate<'a>,
    label: dtu::Label,
    credits: u64,
    sel: Selector,
    flags: Flags,
}

impl<'a> SGateArgs<'a> {
    pub fn new(rgate: &'a RecvGate<'a>, reply_gate: &'a RecvGate<'a>) -> Self {
        SGateArgs {
            rgate: rgate,
            label: 0,
            credits: 0,
            reply_gate: reply_gate,
            sel: INVALID_SEL,
            flags: Flags::empty(),
        }
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

impl<'a> SendGate<'a> {
    pub fn new(rgate: &'a RecvGate, reply_gate: &'a RecvGate<'a>) -> Result<Self, Error> {
        Self::new_with(SGateArgs::new(rgate, reply_gate))
    }

    pub fn new_with(args: SGateArgs<'a>) -> Result<Self, Error> {
        let sel = if args.sel == INVALID_SEL {
            vpe::VPE::cur().alloc_cap()
        }
        else {
            args.sel
        };

        try!(syscalls::create_sgate(sel, args.rgate.sel(), args.label, args.credits));
        Ok(SendGate {
            gate: Gate::new(sel, args.flags),
            reply_gate: args.reply_gate,
        })
    }

    pub fn new_bind(sel: Selector, reply_gate: &'a RecvGate<'a>) -> Result<Self, Error> {
        Ok(SendGate {
            gate: Gate::new(sel, Flags::KEEP_CAP),
            reply_gate: reply_gate,
        })
    }

    pub fn reply_gate(&self) -> &RecvGate<'a> {
        self.reply_gate
    }

    pub fn send<T>(&mut self, msg: &[T]) -> Result<(), Error> {
        try!(self.gate.activate());
        dtu::DTU::send(self.gate.ep, msg, 0, self.reply_gate.ep())
    }
}
