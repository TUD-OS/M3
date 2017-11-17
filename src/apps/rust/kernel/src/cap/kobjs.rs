use base::dtu::{EpId, PEId, Label};
use base::cell::RefCell;
use base::kif;
use base::rc::Rc;
use core::fmt;

use pes::{INVALID_VPE, VPEId};
use pes::vpemng;

pub struct RGateObject {
    pub vpe: VPEId,
    pub ep: Option<EpId>,
    pub addr: usize,
    pub order: i32,
    pub msg_order: i32,
    pub header: usize,
}

impl RGateObject {
    pub fn new(order: i32, msg_order: i32) -> Rc<RefCell<Self>> {
        Rc::new(RefCell::new(RGateObject {
            vpe: INVALID_VPE,
            ep: None,
            addr: 0,
            order: order,
            msg_order: msg_order,
            header: 0,
        }))
    }

    pub fn activated(&self) -> bool {
        self.addr != 0
    }
    pub fn size(&self) -> usize {
        1 << self.order
    }
    pub fn msg_size(&self) -> usize {
        1 << self.msg_order
    }

    fn print_dest(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "VPE{}:", self.vpe)?;
        match vpemng::get().pe_of(self.vpe) {
            Some(pe) => write!(f, "PE{}:", pe)?,
            None     => write!(f, "PE??:")?,
        };
        match self.ep {
            Some(ep) => write!(f, "EP{}", ep),
            None     => write!(f, "EP??"),
        }
    }
}

impl fmt::Debug for RGateObject {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "RGateObject[loc=")?;
        self.print_dest(f)?;
        write!(f, ", addr={:#x}, size={:#x}, msgsize={:#x}, header={:#x}]",
            self.addr, self.size(), self.msg_size(), self.header)
    }
}

pub struct SGateObject {
    pub rgate: Rc<RefCell<RGateObject>>,
    pub label: Label,
    pub credits: u64,
}

impl SGateObject {
    pub fn new(rgate: &Rc<RefCell<RGateObject>>, label: Label, credits: u64) -> Rc<RefCell<Self>> {
        Rc::new(RefCell::new(SGateObject {
            rgate: rgate.clone(),
            label: label,
            credits: credits,
        }))
    }
}

impl fmt::Debug for SGateObject {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "SGateObject[rgate=")?;
        self.rgate.borrow().print_dest(f)?;
        write!(f, ", label={:#}, credits={}]", self.label, self.credits)
    }
}

pub struct MGateObject {
    pub pe: PEId,
    pub vpe: VPEId,
    pub addr: usize,
    pub size: usize,
    pub perms: kif::Perm,
    pub derived: bool,
}

impl MGateObject {
    pub fn new(pe: PEId, vpe: VPEId, addr: usize, size: usize, perms: kif::Perm) -> Rc<RefCell<Self>> {
        Rc::new(RefCell::new(MGateObject {
            pe: pe,
            vpe: vpe,
            addr: addr,
            size: size,
            perms: perms,
            derived: false,
        }))
    }
}

impl fmt::Debug for MGateObject {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "MGateObject[target=VPE{}:PE{}, addr={:#x}, size={:#x}, perms={:?}, derived={}]",
            self.pe, self.vpe, self.addr, self.size, self.perms, self.derived)
    }
}
