use cap;
use com::epmux::EpMux;
use com::gate::{Gate, EpId};
use com::{GateIStream, SendGate};
use core::ops;
use core::fmt;
use dtu;
use env;
use errors::Error;
use kif::INVALID_SEL;
use syscalls;
use util;
use vpe;

// TODO move that elsewhere
const RECVBUF_SPACE: usize       = 0x3FC00000;
const RECVBUF_SIZE: usize        = 4 * dtu::PAGE_SIZE;
const RECVBUF_SIZE_SPM: usize    = 16384;

const SYSC_RBUF_SIZE: usize      = 1 << 9;
const UPCALL_RBUF_SIZE: usize    = 1 << 9;
const DEF_RBUF_SIZE: usize       = 1 << 8;

const DEF_MSG_ORD: i32           = 6;

static mut SYS_RGATE: RecvGate = RecvGate::new_def(dtu::SYSC_REP);
static mut UPC_RGATE: RecvGate = RecvGate::new_def(dtu::UPCALL_REP);
static mut DEF_RGATE: RecvGate = RecvGate::new_def(dtu::DEF_REP);

#[repr(C, packed)]
#[derive(Debug)]
pub struct RBufSpace {
    pub cur: usize,
    pub end: usize,
}

impl RBufSpace {
    pub fn new(cur: usize, end: usize) -> Self {
        RBufSpace {
            cur: cur,
            end: end,
        }
    }
}

bitflags! {
    struct FreeFlags : u8 {
        const FREE_BUF  = 0x1;
        const FREE_EP   = 0x2;
    }
}

pub struct RecvGate {
    gate: Gate,
    buf: usize,
    order: i32,
    free: FreeFlags,
}

impl fmt::Debug for RecvGate {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(f, "RecvGate[sel: {}, buf: {:#0x}, size: {:#0x}]",
            self.sel(), self.buf, 1 << self.order)
    }
}

pub struct RGateArgs {
    order: i32,
    msg_order: i32,
    sel: cap::Selector,
    flags: cap::Flags,
}

impl RGateArgs {
    pub fn new() -> Self {
        RGateArgs {
            order: DEF_MSG_ORD,
            msg_order: DEF_MSG_ORD,
            sel: INVALID_SEL,
            flags: cap::Flags::empty(),
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

impl RecvGate {
    pub fn syscall() -> &'static mut RecvGate {
        unsafe { &mut SYS_RGATE }
    }
    pub fn upcall() -> &'static mut RecvGate {
        unsafe { &mut UPC_RGATE }
    }
    pub fn def() -> &'static mut RecvGate {
        unsafe { &mut DEF_RGATE }
    }

    const fn new_def(ep: EpId) -> Self {
        RecvGate {
            gate: Gate::new_with_ep(INVALID_SEL, cap::Flags::const_empty(), Some(ep)),
            buf: 0,
            order: 0,
            free: FreeFlags { bits: 0 },
        }
    }

    pub fn new(order: i32, msg_order: i32) -> Result<Self, Error> {
        Self::new_with(RGateArgs::new().order(order).msg_order(msg_order))
    }

    pub fn new_with(args: RGateArgs) -> Result<Self, Error> {
        let sel = if args.sel == INVALID_SEL {
            vpe::VPE::cur().alloc_cap()
        }
        else {
            args.sel
        };

        syscalls::create_rgate(sel, args.order, args.msg_order)?;
        Ok(RecvGate {
            gate: Gate::new(sel, args.flags),
            buf: 0,
            order: args.order,
            free: FreeFlags::empty(),
        })
    }

    pub fn sel(&self) -> cap::Selector {
        self.gate.sel()
    }
    pub fn ep(&self) -> Option<EpId> {
        self.gate.ep()
    }

    pub fn activate(&mut self) -> Result<(), Error> {
        if self.ep().is_none() {
            let vpe = vpe::VPE::cur();
            let ep = vpe.alloc_ep()?;
            self.free |= FreeFlags::FREE_EP;
            self.activate_for(vpe.rbufs(), ep)?;
            EpMux::get().reserve(ep);
        }
        Ok(())
    }

    pub fn activate_for(&mut self, rbufs: &mut RBufSpace, ep: EpId) -> Result<(), Error> {
        if self.ep().is_none() {
            let buf = if self.buf == 0 {
                let size = 1 << self.order;
                Self::alloc_buf(rbufs, size)?
            }
            else {
                self.buf
            };

            self.activate_buf(ep, buf)?;
            if self.buf == 0 {
                self.buf = buf;
                self.free |= FreeFlags::FREE_BUF;
            }
        }

        Ok(())
    }

    pub fn activate_buf(&mut self, ep: EpId, addr: usize) -> Result<(), Error> {
        assert!(self.ep().is_none());

        self.gate.set_ep(ep);

        if self.sel() != INVALID_SEL {
            syscalls::activate(0, self.sel(), ep, addr)
        }
        else {
            Ok(())
        }
    }

    pub fn deactivate(&mut self) {
        if !(self.free & FreeFlags::FREE_EP).is_empty() {
            let ep = self.ep().unwrap();
            vpe::VPE::cur().free_ep(ep);
        }
        self.gate.unset_ep();
    }

    pub fn fetch(&self) -> Option<GateIStream> {
        let rep = self.ep().unwrap();
        let msg = dtu::DTU::fetch_msg(rep);
        if let Some(m) = msg {
            Some(GateIStream::new(m, self))
        }
        else {
            None
        }
    }

    pub fn reply<T>(&self, reply: &[T], msg: &'static dtu::Message) -> Result<(), Error> {
        self.reply_bytes(reply.as_ptr() as *const u8, reply.len() * util::size_of::<T>(), msg)
    }
    pub fn reply_bytes(&self, reply: *const u8, size: usize, msg: &'static dtu::Message) -> Result<(), Error> {
        dtu::DTU::reply(self.ep().unwrap(), reply, size, msg)
    }

    pub fn wait(&self, sgate: Option<&SendGate>) -> Result<&'static dtu::Message, Error> {
        assert!(self.ep().is_some());

        let rep = self.ep().unwrap();
        let idle = match sgate {
            Some(sg) => sg.ep().unwrap() != dtu::SYSC_SEP,
            None     => true,
        };

        loop {
            let msg = dtu::DTU::fetch_msg(rep);
            if let Some(m) = msg {
                return Ok(m)
            }

            if let Some(sg) = sgate {
                if !dtu::DTU::is_valid(sg.ep().unwrap()) {
                    return Err(Error::InvEP)
                }
            }

            dtu::DTU::try_sleep(idle, 0)?;
        }
    }

    fn alloc_buf(rbufs: &mut RBufSpace, size: usize) -> Result<usize, Error> {
        if rbufs.end == 0 {
            let pe = &env::data().pedesc;
            let buf_sizes = SYSC_RBUF_SIZE + UPCALL_RBUF_SIZE + DEF_RBUF_SIZE;
            if pe.has_virtmem() {
                rbufs.cur = RECVBUF_SPACE + buf_sizes;
                rbufs.end = RECVBUF_SPACE + RECVBUF_SIZE;
            }
            else {
                rbufs.cur = pe.mem_size() - RECVBUF_SIZE_SPM + buf_sizes;
                rbufs.end = pe.mem_size();
            }
        }

        // TODO atm, the kernel allocates the complete receive buffer space
        let left = rbufs.end - rbufs.cur;
        if size > left {
            Err(Error::NoSpace)
        }
        else {
            let res = rbufs.cur;
            rbufs.cur += size;
            Ok(res)
        }
    }

    fn free_buf(_addr: usize) {
        // TODO implement me
    }
}

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

    RecvGate::upcall().buf = get_buf(SYSC_RBUF_SIZE);
    RecvGate::upcall().order = util::next_log2(UPCALL_RBUF_SIZE);

    RecvGate::def().buf = get_buf(SYSC_RBUF_SIZE + UPCALL_RBUF_SIZE);
    RecvGate::def().order = util::next_log2(DEF_RBUF_SIZE);
}

impl ops::Drop for RecvGate {
    fn drop(&mut self) {
        if !(self.free & FreeFlags::FREE_BUF).is_empty() {
            RecvGate::free_buf(self.buf);
        }
        self.deactivate();
    }
}
