use base::cell::RefCell;
use base::col::String;
use base::dtu::{self, EpId, PEId, Label};
use base::errors::{Code, Error};
use base::GlobAddr;
use base::kif;
use base::rc::Rc;
use base::util;
use core::fmt;
use thread;

use com::SendQueue;
use mem;
use pes::{INVALID_VPE, VPE, VPEId, vpemng};

#[derive(Clone)]
pub enum KObject {
    RGate(Rc<RefCell<RGateObject>>),
    SGate(Rc<RefCell<SGateObject>>),
    MGate(Rc<RefCell<MGateObject>>),
    Map(Rc<RefCell<MapObject>>),
    Serv(Rc<RefCell<ServObject>>),
    Sess(Rc<RefCell<SessObject>>),
    VPE(Rc<RefCell<VPE>>),
    EP(Rc<RefCell<EPObject>>),
}

impl fmt::Debug for KObject {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            KObject::SGate(ref s)   => write!(f, "{:?}", s.borrow()),
            KObject::RGate(ref r)   => write!(f, "{:?}", r.borrow()),
            KObject::MGate(ref m)   => write!(f, "{:?}", m.borrow()),
            KObject::Map(ref m)     => write!(f, "{:?}", m.borrow()),
            KObject::Serv(ref s)    => write!(f, "{:?}", s.borrow()),
            KObject::Sess(ref s)    => write!(f, "{:?}", s.borrow()),
            KObject::VPE(ref v)     => write!(f, "{:?}", v.borrow()),
            KObject::EP(ref e)      => write!(f, "{:?}", e.borrow()),
        }
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

    pub fn get_event(&self) -> thread::Event {
        self as *const Self as thread::Event
    }

    fn print_dest(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "VPE{}:", self.vpe)?;
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
        write!(f, ", lbl={:#x}, crd={}]", self.label, self.credits)
    }
}

pub struct MGateObject {
    pub vpe: VPEId,
    pub mem: mem::Allocation,
    pub perms: kif::Perm,
    pub derived: bool,
}

impl MGateObject {
    pub fn new(vpe: VPEId, mem: mem::Allocation, perms: kif::Perm, derived: bool) -> Rc<RefCell<Self>> {
        Rc::new(RefCell::new(MGateObject {
            vpe: vpe,
            mem: mem,
            perms: perms,
            derived: derived,
        }))
    }

    pub fn pe_id(&self) -> Option<PEId> {
        if self.vpe == INVALID_VPE {
            Some(self.mem.global().pe())
        }
        else {
            vpemng::get().pe_of(self.vpe)
        }
    }

    pub fn addr(&self) -> usize {
        self.mem.global().offset()
    }
    pub fn size(&self) -> usize {
        self.mem.size()
    }
}

impl Drop for MGateObject {
    fn drop(&mut self) {
        // if it's not derived, it's always memory from mem-PEs
        if self.derived {
            self.mem.claim();
        }
    }
}

impl fmt::Debug for MGateObject {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "MGate[vpe={}, mem={:?}, perm={:?}, der={}]",
            self.vpe, self.mem, self.perms, self.derived)
    }
}

pub struct ServObject {
    pub name: String,
    pub rgate: Rc<RefCell<RGateObject>>,
    pub queue: SendQueue,
}

impl ServObject {
    pub fn new(vpe: &Rc<RefCell<VPE>>, name: String, rgate: Rc<RefCell<RGateObject>>) -> Rc<RefCell<Self>> {
        Rc::new(RefCell::new(ServObject {
            name: name,
            rgate: rgate.clone(),
            queue: SendQueue::new(vpe),
        }))
    }

    pub fn vpe(&self) -> &Rc<RefCell<VPE>> {
        self.queue.vpe()
    }

    pub fn send(&mut self, msg: &[u8]) -> Result<thread::Event, Error> {
        let rep = self.rgate.borrow().ep.unwrap();
        self.queue.send(rep, msg, msg.len())
    }

    pub fn send_receive(serv: &Rc<RefCell<ServObject>>, msg: &[u8]) -> Result<&'static dtu::Message, Error> {
        let event = serv.borrow_mut().send(msg);

        event.and_then(|event| {
            thread::ThreadManager::get().wait_for(event);
            Ok(thread::ThreadManager::get().fetch_msg().unwrap())
        })
    }
}

impl fmt::Debug for ServObject {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Serv[name={}, rgate=", self.name)?;
        self.rgate.borrow().print_dest(f)?;
        write!(f, "]")
    }
}

pub struct SessObject {
    pub srv: Rc<RefCell<ServObject>>,
    pub ident: u64,
    pub srv_owned: bool,
}

impl SessObject {
    pub fn new(srv: &Rc<RefCell<ServObject>>, ident: u64, srv_owned: bool) -> Rc<RefCell<Self>> {
        Rc::new(RefCell::new(SessObject {
            srv: srv.clone(),
            ident: ident,
            srv_owned: srv_owned,
        }))
    }
}

impl Drop for SessObject {
    fn drop(&mut self) {
        let mut srv = self.srv.borrow_mut();
        if !self.srv_owned && srv.vpe().borrow().has_app() {
            let smsg = kif::service::Close {
                opcode: kif::service::Operation::CLOSE.val as u64,
                sess: self.ident,
            };

            klog!(SERV, "Sending CLOSE(sess={:#x}) to service {}", self.ident, srv.name);
            srv.send(util::object_to_bytes(&smsg)).ok();
        }
    }
}

impl fmt::Debug for SessObject {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Sess[service={}, ident={:#x}, srv_owned={}]",
            self.srv.borrow().name, self.ident, self.srv_owned)
    }
}

pub struct EPObject {
    pub vpe: VPEId,
    pub ep: EpId,
}

impl EPObject {
    pub fn new(vpe: VPEId, ep: EpId) -> Rc<RefCell<Self>> {
        Rc::new(RefCell::new(EPObject {
            vpe: vpe,
            ep: ep,
        }))
    }
}

impl fmt::Debug for EPObject {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "EPMask[vpe={}, ep={}]", self.vpe, self.ep)
    }
}

bitflags! {
    pub struct MapFlags : u64 {
        const R             = 0b000001;
        const W             = 0b000010;
        const X             = 0b000100;
        const I             = 0b001000;
        const UNCACHED      = 0b100000;
        const RW            = Self::R.bits | Self::W.bits;
        const RWX           = Self::R.bits | Self::W.bits | Self::X.bits;
        const IRWX          = Self::R.bits | Self::W.bits | Self::X.bits | Self::I.bits;
    }
}

impl From<kif::Perm> for MapFlags {
    fn from(perm: kif::Perm) -> Self {
        MapFlags::from_bits_truncate(perm.bits() as u64)
    }
}

pub struct MapObject {
    pub phys: GlobAddr,
    pub flags: MapFlags,
}

impl MapObject {
    pub fn new(phys: GlobAddr, flags: MapFlags) -> Rc<RefCell<Self>> {
        Rc::new(RefCell::new(MapObject {
            phys: phys,
            flags: flags,
        }))
    }

    pub fn remap(&mut self, vpe: &VPE, virt: usize, pages: usize,
                 phys: GlobAddr, flags: MapFlags) -> Result<(), Error> {
        self.phys = phys;
        self.flags = flags;
        self.map(vpe, virt, pages)
    }

    pub fn map(&self, vpe: &VPE, virt: usize, pages: usize) -> Result<(), Error> {
        match vpe.addr_space() {
            Some(space) => space.map_pages(&vpe.desc(), virt, self.phys, pages, self.flags),
            None        => Err(Error::new(Code::NotSup)),
        }
    }

    pub fn unmap(&self, vpe: &VPE, virt: usize, pages: usize) {
        vpe.addr_space().map(|space| space.unmap_pages(&vpe.desc(), virt, pages));
    }
}

impl fmt::Debug for MapObject {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Map[phys={:?}, flags={:#x}]", self.phys, self.flags)
    }
}
