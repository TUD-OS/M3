use cap;
use com::{SendGate, RecvGate, RGateArgs};
use core::fmt;
use errors::Error;
use dtu::EpId;
use kif;
use session::Fd;
use session::Session;
use vpe::VPE;

pub struct Pager {
    sess: Session,
    rep: EpId,
    rbuf: usize,
    rgate: Option<RecvGate>,
    sgate: SendGate,
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
        let sess = Session::new(pager, 0)?;
        Self::create(vpe, rbuf, sess)
    }

    pub fn new_bind(sess: cap::Selector, sgate: cap::Selector, rgate: cap::Selector) -> Self {
        Pager {
            sess: Session::new_bind(sess),
            rep: 0,
            rbuf: 0,
            rgate: Some(RecvGate::new_bind(rgate, 6)),
            sgate: SendGate::new_bind(sgate),
        }
    }

    pub fn new_clone(&self, vpe: &mut VPE, rbuf: usize) -> Result<Self, Error> {
        let sess = self.sess.obtain_obj()?;
        Self::create(vpe, rbuf, Session::new_owned_bind(sess))
    }

    fn create(vpe: &mut VPE, rbuf: usize, sess: Session) -> Result<Self, Error> {
        let sgate = SendGate::new_bind(sess.obtain_obj()?);
        let rep = vpe.alloc_ep()?;
        let rgate = match vpe.pe().has_mmu() {
            true    => {
                let mut rgate = RecvGate::new_with(RGateArgs::new().order(6).msg_order(6))?;
                rgate.activate_for(vpe.sel(), rbuf, rep)?;
                Some(rgate)
            },
            false   => None,
        };

        Ok(Pager {
            sess: sess,
            rep: rep,
            rbuf: rbuf,
            rgate: rgate,
            sgate: sgate,
        })
    }

    pub fn sel(&self) -> cap::Selector {
        self.sess.sel()
    }

    pub fn rep(&self) -> EpId {
        self.rep
    }
    pub fn sgate(&self) -> &SendGate {
        &self.sgate
    }
    pub fn rgate(&self) -> Option<&RecvGate> {
        self.rgate.as_ref()
    }

    pub fn delegate_caps(&mut self, vpe: &VPE) -> Result<(), Error> {
        let crd = kif::CapRngDesc::new(kif::CapType::OBJECT, vpe.sel(), 2);
        self.sess.delegate(crd, &[], &mut []).map(|_| ())
    }

    pub fn deactivate(&mut self) {
        if let Some(ref mut rg) = self.rgate {
            rg.deactivate();
        }
    }
    pub fn activate(&mut self, vpe: cap::Selector) -> Result<(), Error> {
        if let Some(ref mut rg) = self.rgate {
            assert!(self.rbuf != 0);
            rg.activate_for(vpe, self.rbuf, self.rep)
        }
        else {
            Ok(())
        }
    }

    pub fn clone(&self) -> Result<(), Error> {
        send_recv_res!(
            &self.sgate, RecvGate::def(),
            Operation::CLONE
        ).map(|_| ())
    }

    pub fn pagefault(&self, addr: usize, access: u32) -> Result<(), Error> {
        send_recv_res!(
            &self.sgate, RecvGate::def(),
            Operation::PAGEFAULT, addr, access
        ).map(|_| ())
    }

    pub fn map_anon(&self, addr: usize, len: usize, prot: kif::Perm) -> Result<usize, Error> {
        let mut reply = send_recv_res!(
            &self.sgate, RecvGate::def(),
            Operation::MAP_ANON, addr, len, prot.bits(), 0
        )?;
        Ok(reply.pop())
    }

    pub fn map_ds(&self, addr: usize, len: usize, off: usize, prot: kif::Perm,
                  sess: &Session, fd: Fd) -> Result<usize, Error> {
        let mut rargs = [0u64; 1];
        let sargs: [u64; 5] = [
            addr as u64,
            len as u64,
            prot.bits() as u64,
            fd as u64,
            off as u64,
        ];
        let crd = kif::CapRngDesc::new(kif::CapType::OBJECT, sess.sel(), 1);
        self.sess.delegate(crd, &sargs, &mut rargs)?;
        Ok(rargs[0] as usize)
    }

    pub fn unmap(&self, addr: usize) -> Result<(), Error> {
        send_recv_res!(
            &self.sgate, RecvGate::def(),
            Operation::UNMAP, addr
        ).map(|_| ())
    }
}

impl fmt::Debug for Pager {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(f, "Pager[sel: {}, rep: {}, sep: {}]",
            self.sel(), self.rep, self.sgate.ep().unwrap())
    }
}

pub trait Map {
    fn map(&self, pager: &Pager, virt: usize,
           off: usize, len: usize, prot: kif::Perm) -> Result<(), Error>;
}
