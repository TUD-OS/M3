use base::col::{String, ToString, Vec};
use base::cell::{Ref, RefCell, RefMut};
use base::dtu::{EpId, PEId, HEADER_COUNT};
use base::errors::{Code, Error};
use base::kif::PEDesc;
use base::rc::{Rc, Weak};

use arch::kdtu;
use cap::{SGateObject, RGateObject, MGateObject};
use pes::vpemng;
use platform;

pub type VPEId = usize;

bitflags! {
    pub struct VPEFlags : u32 {
        const BOOTMOD     = 0b00000001;
        const DAEMON      = 0b00000010;
        const IDLE        = 0b00000100;
        const INIT        = 0b00001000;
        const HASAPP      = 0b00010000;
        const MUXABLE     = 0b00100000; // TODO temporary
        const READY       = 0b01000000;
        const WAITING     = 0b10000000;
    }
}

#[derive(Clone, Copy, Eq, PartialEq)]
pub enum State {
    RUNNING,
    SUSPENDED,
    DEAD
}

pub const INVALID_VPE: VPEId = 0xFFFF;

pub struct VPEDesc {
    pe: PEId,
    vpe: VPEId,
}

impl VPEDesc {
    pub fn new(pe: PEId, vpe: VPEId) -> Self {
        VPEDesc {
            pe: pe,
            vpe: vpe,
        }
    }

    pub fn pe(&self) -> PEId {
        self.pe
    }
    pub fn vpe(&self) -> VPEId {
        self.vpe
    }
}

pub struct VPE {
    self_weak: Weak<RefCell<VPE>>,
    desc: VPEDesc,
    pid: i32,
    state: State,
    name: String,
    flags: VPEFlags,
    eps_addr: usize,
    args: Vec<String>,
    req: Vec<String>,
    dtu_state: kdtu::State,
    rbufs_size: usize,
    headers: usize,
}

impl VPE {
    pub fn new(name: &str, id: VPEId, pe: PEId, flags: VPEFlags) -> Rc<RefCell<Self>> {
        let vpe = Rc::new(RefCell::new(VPE {
            self_weak: Weak::new(),
            desc: VPEDesc::new(pe, id),
            pid: 0,
            state: State::DEAD,
            name: name.to_string(),
            flags: flags,
            eps_addr: 0,
            args: Vec::new(),
            req: Vec::new(),
            dtu_state: kdtu::State::new(),
            rbufs_size: 0,
            headers: 0,
        }));
        vpe.borrow_mut().self_weak = Rc::downgrade(&vpe);
        vpe.borrow_mut().init();
        vpe
    }

    #[cfg(target_os = "linux")]
    fn init(&mut self) {
    }

    #[cfg(target_os = "none")]
    fn init(&mut self) {
        use base::dtu;
        use base::cfg;

        let rgate = RGateObject::new(cfg::SYSC_RBUF_ORD, cfg::SYSC_RBUF_ORD);

        // attach syscall receive endpoint
        {
            let mut rgate = rgate.borrow_mut();
            rgate.vpe = vpemng::KERNEL_VPE;
            rgate.order = cfg::SYSC_RBUF_ORD;
            rgate.msg_order = cfg::SYSC_RBUF_ORD;
            rgate.addr = platform::default_rcvbuf(self.pe_id());
            rgate.ep = Some(dtu::SYSC_SEP);
        }
        self.config_rcv_ep(dtu::SYSC_REP, &mut rgate.borrow_mut()).unwrap();

        // attach syscall endpoint
        let sgate = SGateObject::new(&rgate, self.id() as dtu::Label, cfg::SYSC_RBUF_SIZE as u64);
        self.config_snd_ep(dtu::SYSC_SEP, &sgate.borrow());

        // attach upcall receive endpoint
        {
            let mut rgate = rgate.borrow_mut();
            rgate.order = cfg::UPCALL_RBUF_ORD;
            rgate.msg_order = cfg::UPCALL_RBUF_ORD;
            rgate.addr += cfg::SYSC_RBUF_SIZE;
        }
        self.config_rcv_ep(dtu::UPCALL_REP, &mut rgate.borrow_mut()).unwrap();

        // attach default receive endpoint
        {
            let mut rgate = rgate.borrow_mut();
            rgate.order = cfg::DEF_RBUF_ORD;
            rgate.msg_order = cfg::DEF_RBUF_ORD;
            rgate.addr += cfg::DEF_RBUF_SIZE;
        }
        self.config_rcv_ep(dtu::DEF_REP, &mut rgate.borrow_mut()).unwrap();

        self.rbufs_size = rgate.borrow().addr + (1 << rgate.borrow().order);
        self.rbufs_size -= platform::default_rcvbuf(self.pe_id());
    }

    pub fn id(&self) -> VPEId {
        self.desc.vpe()
    }
    pub fn desc(&self) -> &VPEDesc {
        &self.desc
    }
    pub fn pe_id(&self) -> PEId {
        self.desc.pe()
    }
    pub fn pe_desc(&self) -> PEDesc {
        platform::pe_desc(self.pe_id())
    }
    pub fn name(&self) -> &str {
        &self.name
    }
    pub fn args(&self) -> &Vec<String> {
        &self.args
    }
    pub fn requirements(&self) -> &Vec<String> {
        &self.req
    }

    pub fn state(&self) -> State {
        self.state.clone()
    }
    pub fn set_state(&mut self, state: State) {
        self.state = state;
    }

    pub fn dtu_state(&mut self) -> &mut kdtu::State {
        &mut self.dtu_state
    }

    pub fn make_daemon(&mut self) {
        self.flags |= VPEFlags::DAEMON;
    }
    pub fn add_arg(&mut self, arg: &str) {
        self.args.push(arg.to_string());
    }
    pub fn add_requirement(&mut self, req: &str) {
        self.req.push(req.to_string());
    }

    pub fn eps_addr(&self) -> usize {
        self.eps_addr
    }
    pub fn set_eps_addr(&mut self, eps: usize) {
        self.eps_addr = eps;
    }

    pub fn set_pid(&mut self, pid: i32) {
        self.pid = pid;
    }

    pub fn config_snd_ep(&mut self, ep: EpId, obj: &Ref<SGateObject>) {
        let rgate = obj.rgate.borrow();
        assert!(rgate.activated());

        let pe = vpemng::get().pe_of(rgate.vpe).unwrap();

        klog!(EPS, "VPE{}:EP{} = {:?}", self.id(), ep, obj);

        self.dtu_state.config_send(
            ep, obj.label, pe, rgate.vpe, rgate.ep.unwrap(), rgate.msg_size(), obj.credits
        );
        self.update_ep(ep);
    }

    pub fn config_rcv_ep(&mut self, ep: EpId, obj: &mut RefMut<RGateObject>) -> Result<(), Error> {
        // it needs to be in the receive buffer space
        let addr = platform::default_rcvbuf(self.pe_id());
        let size = platform::rcvbufs_size(self.pe_id());

        // default_rcvbuf() == 0 means that we do not validate it
        if addr != 0 && (obj.addr < addr ||
                         obj.addr > addr + size ||
                         obj.addr + obj.size() > addr + size ||
                         obj.addr < addr + self.rbufs_size) {
            return Err(Error::new(Code::InvArgs));
        }

        // no free headers left?
        let msg_slots = 1 << (obj.order - obj.msg_order);
        if self.headers + msg_slots > HEADER_COUNT {
            return Err(Error::new(Code::OutOfMem));
        }

        // TODO really manage the header space and zero the headers first in case they are reused
        obj.header = self.headers;
        self.headers += msg_slots;

        klog!(EPS, "VPE{}:EP{} = {:?}", self.id(), ep, obj);

        self.dtu_state.config_recv(ep, obj.addr, obj.order, obj.msg_order, obj.header);
        self.update_ep(ep);

        Ok(())
    }

    pub fn config_mem_ep(&mut self, ep: EpId, obj: &Ref<MGateObject>, off: usize) -> Result<(), Error> {
        if off >= obj.size || obj.addr + off < off {
            return Err(Error::new(Code::InvArgs));
        }

        klog!(EPS, "VPE{}:EP{} = {:?}", self.id(), ep, obj);

        // TODO
        self.dtu_state.config_mem(ep, obj.pe, obj.vpe, obj.addr + off, obj.size - off, obj.perms);
        self.update_ep(ep);
        Ok(())
    }

    fn update_ep(&mut self, ep: EpId) {
        if self.state == State::RUNNING {
            kdtu::KDTU::get().write_ep_remote(self.desc(), ep, self.dtu_state.get_ep(ep)).unwrap();
        }
    }
}
