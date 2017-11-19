use base::dtu::{EpId, PEId, Label};
use base::cell::RefCell;
use base::kif;
use base::rc::Rc;
use core::fmt;

use pes::{INVALID_VPE, VPE, VPEId};
use pes::vpemng;

#[derive(Clone)]
pub enum KObject {
    RGate(Rc<RefCell<RGateObject>>),
    SGate(Rc<RefCell<SGateObject>>),
    MGate(Rc<RefCell<MGateObject>>),
    VPE(Rc<RefCell<VPE>>),
}

impl fmt::Debug for KObject {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            KObject::SGate(ref s)   => write!(f, "{:?}", s.borrow()),
            KObject::RGate(ref r)   => write!(f, "{:?}", r.borrow()),
            KObject::MGate(ref m)   => write!(f, "{:?}", m.borrow()),
            KObject::VPE(ref v)     => write!(f, "{:?}", v.borrow()),
        }
    }
}

macro_rules! map_enum {
    ($v:expr, $e:ident) => (match $v {
        &KObject::$e(ref r)  => unsafe { Some(&*r.as_ptr()) },
        _                    => None,
    })
}
macro_rules! map_enum_mut {
    ($v:expr, $e:ident) => (match $v {
        &mut KObject::$e(ref mut r) => unsafe { Some(&mut *r.as_ptr()) },
        _                           => None,
    })
}

impl KObject {
    pub fn as_rgate<'r>(&'r self) -> Option<&'r RGateObject> {
        map_enum!(self, RGate)
    }
    pub fn as_rgate_mut<'r>(&'r mut self) -> Option<&'r mut RGateObject> {
        map_enum_mut!(self, RGate)
    }

    pub fn as_sgate<'r>(&'r self) -> Option<&'r SGateObject> {
        map_enum!(self, SGate)
    }
    pub fn as_sgate_mut<'r>(&'r mut self) -> Option<&'r mut SGateObject> {
        map_enum_mut!(self, SGate)
    }

    pub fn as_mgate<'r>(&'r self) -> Option<&'r MGateObject> {
        map_enum!(self, MGate)
    }
    pub fn as_mgate_mut<'r>(&'r mut self) -> Option<&'r mut MGateObject> {
        map_enum_mut!(self, MGate)
    }

    pub fn as_vpe<'r>(&'r self) -> Option<&'r VPE> {
        map_enum!(self, VPE)
    }
    pub fn as_vpe_mut<'r>(&'r mut self) -> Option<&'r mut VPE> {
        map_enum_mut!(self, VPE)
    }
}

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
        write!(f, "RGate[loc=")?;
        self.print_dest(f)?;
        write!(f, ", addr={:#x}, sz={:#x}, msz={:#x}, hd={:#x}]",
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
        write!(f, "SGate[rgate=")?;
        self.rgate.borrow().print_dest(f)?;
        write!(f, ", lbl={:#}, crd={}]", self.label, self.credits)
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
        write!(f, "MGate[tgt=VPE{}:PE{}, addr={:#x}, sz={:#x}, perm={:?}, der={}]",
            self.vpe, self.pe, self.addr, self.size, self.perms, self.derived)
    }
}
