use cap;
use com::epmux::EpMux;
// TODO use Option instead of INVALID_EP
use com::gate::{Gate, EpId, INVALID_EP};
use core::ops;
use dtu;
use env;
use errors::Error;
use kif::INVALID_SEL;
use syscalls;
use util;
use vpe;

bitflags! {
    struct FreeFlags : u8 {
        const FREE_BUF  = 0x1;
        const FREE_EP   = 0x2;
    }
}

pub struct RecvGate<'v> {
    gate: Gate,
    buf: usize,
    order: i32,
    free: FreeFlags,
    vpe: Option<&'v mut vpe::VPE>,
}

const RECVBUF_SPACE: usize       = 0x3FC00000;
const RECVBUF_SIZE: usize        = 4 * dtu::PAGE_SIZE;
const RECVBUF_SIZE_SPM: usize    = 16384;

const SYSC_RBUF_SIZE: usize      = 1 << 9;
const UPCALL_RBUF_SIZE: usize    = 1 << 8;
const DEF_RBUF_SIZE: usize       = 1 << 8;

const DEF_MSG_ORD: i32           = 6;

// TODO move that into VPE
static mut RBUFS_CUR: usize      = 0;
static mut RBUFS_END: usize      = 0;

static mut SYS_RGATE: RecvGate = RecvGate::new_def(dtu::SYSC_REP);
static mut UPC_RGATE: RecvGate = RecvGate::new_def(dtu::UPCALL_REP);
static mut DEF_RGATE: RecvGate = RecvGate::new_def(dtu::DEF_REP);

pub fn init() {
    let get_buf = |off| {
        let pe = &env::data().pedesc;
        if pe.has_virtmem() {
            RECVBUF_SPACE + off
        }
        else {
            (pe.mem_size() - RECVBUF_SIZE_SPM) + off
        }
    };

    RecvGate::syscall().buf = get_buf(0);
    RecvGate::syscall().order = util::next_log2(SYSC_RBUF_SIZE);
    RecvGate::syscall().vpe = Some(vpe::VPE::cur());

    RecvGate::upcall().buf = get_buf(SYSC_RBUF_SIZE);
    RecvGate::upcall().order = util::next_log2(UPCALL_RBUF_SIZE);
    RecvGate::upcall().vpe = Some(vpe::VPE::cur());

    RecvGate::def().buf = get_buf(SYSC_RBUF_SIZE + UPCALL_RBUF_SIZE);
    RecvGate::def().order = util::next_log2(DEF_RBUF_SIZE);
    RecvGate::def().vpe = Some(vpe::VPE::cur());
}

pub struct RGateArgs<'v> {
    order: i32,
    msg_order: i32,
    sel: cap::Selector,
    flags: cap::Flags,
    vpe: &'v mut vpe::VPE,
}

impl<'v> RGateArgs<'v> {
    pub fn new() -> Self {
        RGateArgs {
            order: DEF_MSG_ORD,
            msg_order: DEF_MSG_ORD,
            sel: INVALID_SEL,
            flags: cap::Flags::empty(),
            vpe: vpe::VPE::cur(),
        }
    }

    pub fn order(mut self, order: i32) -> Self {
        self.order = order;
        self
    }

    pub fn msg_order(mut self, msg_order: i32) -> Self {
        self.msg_order = msg_order;
        self
    }

    pub fn sel(mut self, sel: cap::Selector) -> Self {
        self.sel = sel;
        self
    }
}

impl<'v> RecvGate<'v> {
    pub fn syscall() -> &'static mut RecvGate<'v> {
        unsafe { &mut SYS_RGATE }
    }
    pub fn upcall() -> &'static mut RecvGate<'v> {
        unsafe { &mut UPC_RGATE }
    }
    pub fn def() -> &'static mut RecvGate<'v> {
        unsafe { &mut DEF_RGATE }
    }

    const fn new_def(ep: EpId) -> Self {
        RecvGate {
            gate: Gate::new_with_ep(INVALID_SEL, cap::Flags::const_empty(), ep),
            buf: 0,
            order: 0,
            free: FreeFlags { bits: 0 },
            vpe: None,
        }
    }

    pub fn new(order: i32, msg_order: i32) -> Result<Self, Error> {
        Self::new_with(RGateArgs::new().order(order).msg_order(msg_order))
    }

    pub fn new_with<'a>(args: RGateArgs<'a>) -> Result<RecvGate<'a>, Error> {
        let sel = if args.sel == INVALID_SEL {
            vpe::VPE::cur().alloc_cap()
        }
        else {
            args.sel
        };

        try!(syscalls::create_rgate(sel, args.order, args.msg_order));
        Ok(RecvGate {
            gate: Gate::new(sel, args.flags),
            buf: 0,
            order: args.order,
            free: FreeFlags::empty(),
            vpe: Some(args.vpe),
        })
    }

    pub fn sel(&self) -> cap::Selector {
        self.gate.cap.sel()
    }
    pub fn ep(&self) -> EpId {
        self.gate.ep
    }

    pub fn activate(&mut self) -> Result<(), Error> {
        if self.gate.ep == INVALID_EP {
            let ep = try!(self.vpe().alloc_ep());
            self.free |= FreeFlags::FREE_EP;
            self.activate_ep(ep)
        }
        else {
            Ok(())
        }
    }

    pub fn activate_ep(&mut self, ep: EpId) -> Result<(), Error> {
        if self.gate.ep == INVALID_EP {
            let buf = if self.buf == 0 {
                try!(Self::alloc_buf(1 << self.order))
            }
            else {
                self.buf
            };

            try!(self.activate_for(ep, buf));
            if self.buf == 0 {
                self.buf = buf;
                self.free |= FreeFlags::FREE_BUF;
            }
        }

        Ok(())
    }

    pub fn activate_for(&mut self, ep: EpId, addr: usize) -> Result<(), Error> {
        assert!(self.gate.ep == INVALID_EP);

        self.gate.ep = ep;

        if self.vpe().sel() == vpe::VPE::cur().sel() {
            EpMux::get().reserve(ep);
        }

        if self.gate.cap.sel() != INVALID_SEL {
            syscalls::activate(0, self.gate.cap.sel(), ep, addr)
        }
        else {
            Ok(())
        }
    }

    pub fn deactivate(&mut self) {
        if !(self.free & FreeFlags::FREE_EP).is_empty() {
            let ep = self.gate.ep;
            self.vpe().free_ep(ep);
        }
        self.gate.ep = INVALID_EP;

        // TODO stop
    }

    fn vpe(&mut self) -> &mut vpe::VPE {
        self.vpe.as_mut().unwrap()
    }

    fn alloc_buf(size: usize) -> Result<usize, Error> {
        // TODO get rid of that as soon as we don't need the global vars anymore
        unsafe {
            if RBUFS_END == 0 {
                let pe = &env::data().pedesc;
                let buf_sizes = SYSC_RBUF_SIZE + UPCALL_RBUF_SIZE + DEF_RBUF_SIZE;
                if pe.has_virtmem() {
                    RBUFS_CUR = RECVBUF_SPACE + buf_sizes;
                    RBUFS_END = RECVBUF_SPACE + RECVBUF_SIZE;
                }
                else {
                    RBUFS_CUR = pe.mem_size() - RECVBUF_SIZE_SPM + buf_sizes;
                    RBUFS_END = pe.mem_size();
                }
            }

            // TODO atm, the kernel allocates the complete receive buffer space
            let left = RBUFS_END - RBUFS_CUR;
            if size > left {
                Err(Error::NoSpace)
            }
            else {
                let res = RBUFS_CUR;
                RBUFS_CUR += size;
                Ok(res)
            }
        }
    }

    fn free_buf(_addr: usize) {
        // TODO implement me
    }
}

impl<'v> ops::Drop for RecvGate<'v> {
    fn drop(&mut self) {
        if !(self.free & FreeFlags::FREE_BUF).is_empty() {
            RecvGate::free_buf(self.buf);
        }
        self.deactivate();
    }
}
