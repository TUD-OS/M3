use cap::{Flags, SelSpace, Selector};
use com::gate::Gate;
use com::RecvGate;
use dtu;
use errors::Error;
use kif::INVALID_SEL;
use syscalls;

pub struct SendGate<'rpl> {
    gate: Gate,
    reply_gate: &'rpl mut RecvGate,
}

pub struct SGateArgs<'recv, 'rpl> {
    rgate: &'recv RecvGate,
    reply_gate: &'rpl mut RecvGate,
    label: dtu::Label,
    credits: u64,
    sel: Selector,
    flags: Flags,
}

impl<'recv, 'rpl> SGateArgs<'recv, 'rpl> {
    pub fn new(rgate: &'recv RecvGate, reply_gate: &'rpl mut RecvGate) -> SGateArgs<'recv, 'rpl> {
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
        self.flags |= Flags::KEEP_SEL;
        self
    }
}

impl<'r> SendGate<'r> {
    pub fn new<'recv, 'rpl>(rgate: &'recv RecvGate,
                            reply_gate: &'rpl mut RecvGate) -> Result<SendGate<'rpl>, Error> {
        Self::new_with(SGateArgs::new(rgate, reply_gate))
    }

    pub fn new_with<'recv, 'rpl>(args: SGateArgs<'recv, 'rpl>) -> Result<SendGate<'rpl>, Error> {
        let sel = if args.sel == INVALID_SEL {
            SelSpace::get().alloc()
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

    pub fn new_bind<'rpl>(sel: Selector,
                          reply_gate: &'rpl mut RecvGate) -> Result<SendGate<'rpl>, Error> {
        Ok(SendGate {
            gate: Gate::new(sel, Flags::KEEP_SEL | Flags::KEEP_CAP),
            reply_gate: reply_gate,
        })
    }

    pub fn reply_gate(&mut self) -> &mut RecvGate {
        self.reply_gate
    }

    pub fn send<T>(&mut self, msg: &[T]) -> Result<(), Error> {
        try!(self.gate.activate());
        dtu::DTU::send(self.gate.ep, msg, 0, 0)
    }
}
