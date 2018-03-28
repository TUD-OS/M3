use cap;
use com::{SendGate, RecvGate, RGateArgs};
use core::fmt;
use dtu::{EpId, FIRST_FREE_EP};
use errors::Error;
use kif;
use session::ClientSession;
use vpe::VPE;

pub struct Pager {
    sess: ClientSession,
    sep: EpId,
    rep: EpId,
    rbuf: usize,
    rgate: Option<RecvGate>,
    own_sgate: SendGate,
    child_sgate: SendGate,
}

int_enum! {
    struct DelOp : u32 {
        const DATASPACE = 0x0;
        const MEMGATE   = 0x1;
    }
}

int_enum! {
    struct Operation : u32 {
        const PAGEFAULT = 0x0;
        const CLONE     = 0x1;
        const MAP_ANON  = 0x2;
        const UNMAP     = 0x3;
    }
}

impl Pager {
    pub fn new(vpe: &mut VPE, rbuf: usize, pager: &str) -> Result<Self, Error> {
        let sess = ClientSession::new(pager, 0)?;
        Self::create(vpe, rbuf, sess)
    }

    pub fn new_bind(sess_sel: cap::Selector, rgate: cap::Selector) -> Result<Self, Error> {
        let sess = ClientSession::new_bind(sess_sel);
        let sgate = SendGate::new_bind(sess.obtain_obj()?);
        Ok(Pager {
            sess: sess,
            sep: 0,
            rep: 0,
            rbuf: 0,
            rgate: Some(RecvGate::new_bind(rgate, 6)),
            own_sgate: sgate,
            child_sgate: SendGate::new_bind(kif::INVALID_SEL),
        })
    }

    pub fn new_clone(&self, vpe: &mut VPE, rbuf: usize) -> Result<Self, Error> {
        let mut args = kif::syscalls::ExchangeArgs::default();
        // dummy arg to distinguish from the get_sgate operation
        args.count = 1;
        let sess = self.sess.obtain(1, &mut args)?;
        Self::create(vpe, rbuf, ClientSession::new_owned_bind(sess.start()))
    }

    fn create(vpe: &mut VPE, rbuf: usize, sess: ClientSession) -> Result<Self, Error> {
        let own_sgate = SendGate::new_bind(sess.obtain_obj()?);
        let child_sgate = SendGate::new_bind(sess.obtain_obj()?);
        let sep = vpe.alloc_ep()?;
        let rep = vpe.alloc_ep()?;
        let rgate = match vpe.pe().has_mmu() {
            true    => Some(RecvGate::new_with(RGateArgs::new().order(6).msg_order(6))?),
            false   => None,
        };

        Ok(Pager {
            sess: sess,
            sep: sep,
            rep: rep,
            rbuf: rbuf,
            rgate: rgate,
            own_sgate: own_sgate,
            child_sgate: child_sgate,
        })
    }

    pub fn sel(&self) -> cap::Selector {
        self.sess.sel()
    }

    pub fn sep(&self) -> EpId {
        self.sep
    }
    pub fn rep(&self) -> EpId {
        self.rep
    }

    pub fn own_sgate(&self) -> &SendGate {
        &self.own_sgate
    }
    pub fn child_sgate(&self) -> &SendGate {
        &self.child_sgate
    }
    pub fn rgate(&self) -> Option<&RecvGate> {
        self.rgate.as_ref()
    }

    pub fn delegate_caps(&mut self, vpe: &VPE) -> Result<(), Error> {
        let crd = kif::CapRngDesc::new(kif::CapType::OBJECT, vpe.sel(), 2);
        self.sess.delegate_crd(crd)
    }

    pub fn deactivate(&mut self) {
        if let Some(ref mut rg) = self.rgate {
            rg.deactivate();
        }
    }
    pub fn activate(&mut self, first_ep: cap::Selector) -> Result<(), Error> {
        self.child_sgate.activate_for(first_ep + (self.sep - FIRST_FREE_EP) as cap::Selector)?;

        if let Some(ref mut rg) = self.rgate {
            assert!(self.rbuf != 0);
            rg.activate_for(first_ep, self.rep, self.rbuf)
        }
        else {
            Ok(())
        }
    }

    pub fn clone(&self) -> Result<(), Error> {
        send_recv_res!(
            &self.own_sgate, RecvGate::def(),
            Operation::CLONE
        ).map(|_| ())
    }

    pub fn pagefault(&self, addr: usize, access: u32) -> Result<(), Error> {
        send_recv_res!(
            &self.own_sgate, RecvGate::def(),
            Operation::PAGEFAULT, addr, access
        ).map(|_| ())
    }

    pub fn map_anon(&self, addr: usize, len: usize, prot: kif::Perm) -> Result<usize, Error> {
        let mut reply = send_recv_res!(
            &self.own_sgate, RecvGate::def(),
            Operation::MAP_ANON, addr, len, prot.bits(), 0
        )?;
        Ok(reply.pop())
    }

    pub fn map_ds(&self, addr: usize, len: usize, off: usize, prot: kif::Perm,
                  sess: &ClientSession) -> Result<usize, Error> {
        let mut args = kif::syscalls::ExchangeArgs {
            count: 5,
            vals: kif::syscalls::ExchangeUnion {
                i: [
                    DelOp::DATASPACE.val as u64,
                    addr as u64,
                    len as u64,
                    prot.bits() as u64,
                    off as u64,
                    0, 0, 0
                ]
            },
        };

        let crd = kif::CapRngDesc::new(kif::CapType::OBJECT, sess.sel(), 1);
        self.sess.delegate(crd, &mut args)?;
        unsafe {
            Ok(args.vals.i[0] as usize)
        }
    }

    pub fn unmap(&self, addr: usize) -> Result<(), Error> {
        send_recv_res!(
            &self.own_sgate, RecvGate::def(),
            Operation::UNMAP, addr
        ).map(|_| ())
    }
}

impl fmt::Debug for Pager {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(f, "Pager[sel: {}, rep: {}, sep: {}]",
            self.sel(), self.rep, self.own_sgate.ep().unwrap())
    }
}
