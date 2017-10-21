use boxed::Box;
use alloc::boxed::FnBox;
use cap::{Capability, Flags, Selector};
use com::{MemGate, RBufSpace};
use collections::Vec;
use core::iter;
use dtu::{EP_COUNT, FIRST_FREE_EP, EpId};
use env;
use errors::Error;
use heap;
use kif;
use kif::{cap, CapRngDesc, INVALID_SEL, PEDesc};
use libc;
use syscalls;
use util;

static mut CUR: Option<VPE> = None;

#[derive(Debug)]
pub struct VPE {
    cap: Capability,
    pe: PEDesc,
    mem: MemGate,
    next_sel: Selector,
    eps: u64,
    rbufs: RBufSpace,
}

pub struct VPEArgs<'n> {
    name: &'n str,
    pe: PEDesc,
    muxable: bool,
}

pub trait Activity {
    fn start(&self) -> Result<(), Error>;
    fn stop(&self) -> Result<(), Error>;
    fn wait(&self) -> Result<i32, Error>;
}

pub struct ClosureActivity<'v> {
    vpe: &'v mut VPE,
    _closure: env::Closure,
}

impl<'v> ClosureActivity<'v> {
    pub fn new<'vo : 'v>(vpe: &'vo mut VPE, closure: env::Closure) -> ClosureActivity<'v> {
        ClosureActivity {
            vpe: vpe,
            _closure: closure,
        }
    }
}

impl<'v> Activity for ClosureActivity<'v> {
    fn start(&self) -> Result<(), Error> {
        syscalls::vpe_ctrl(self.vpe.sel(), kif::syscalls::VPEOp::Start, 0).map(|_| ())
    }

    fn stop(&self) -> Result<(), Error> {
        syscalls::vpe_ctrl(self.vpe.sel(), kif::syscalls::VPEOp::Stop, 0).map(|_| ())
    }

    fn wait(&self) -> Result<i32, Error> {
        syscalls::vpe_ctrl(self.vpe.sel(), kif::syscalls::VPEOp::Wait, 0)
    }
}

impl<'v> Drop for ClosureActivity<'v> {
    fn drop(&mut self) {
        self.stop().ok();
    }
}

impl<'n> VPEArgs<'n> {
    pub fn new(name: &'n str) -> VPEArgs<'n> {
        VPEArgs {
            name: name,
            pe: VPE::cur().pe(),
            muxable: false,
        }
    }

    pub fn pe(mut self, pe: PEDesc) -> Self {
        self.pe = pe;
        self
    }

    pub fn muxable(mut self, muxable: bool) -> Self {
        self.muxable = muxable;
        self
    }
}

impl VPE {
    fn new_cur() -> Self {
        VPE {
            cap: Capability::new(0, Flags::KEEP_CAP),
            pe: env::data().pedesc.clone(),
            mem: MemGate::new_bind(1),
            // 0 and 1 are reserved for VPE cap and mem cap
            next_sel: 2,
            eps: 0,
            rbufs: RBufSpace::new(env::data().rbuf_cur as usize, env::data().rbuf_end as usize),
        }
    }

    pub fn cur() -> &'static mut VPE {
        unsafe {
            CUR.as_mut().unwrap()
        }
    }

    pub fn new(name: &str) -> Result<Self, Error> {
        Self::new_with(VPEArgs::new(name))
    }

    pub fn new_with(builder: VPEArgs) -> Result<Self, Error> {
        let sels = VPE::cur().alloc_caps(2);
        let pe = try!(syscalls::create_vpe(
            sels + 0, sels + 1, INVALID_SEL, INVALID_SEL, builder.name,
            builder.pe, 0, 0, builder.muxable
        ));

        Ok(VPE {
            cap: Capability::new(sels + 0, Flags::empty()),
            pe: pe,
            mem: MemGate::new_bind(sels + 1),
            next_sel: 2,
            eps: 0,
            rbufs: RBufSpace::new(0, 0),
        })
    }

    pub fn sel(&self) -> Selector {
        self.cap.sel()
    }
    pub fn pe(&self) -> PEDesc {
        self.pe
    }
    pub fn mem(&self) -> &MemGate {
        &self.mem
    }

    pub fn alloc_cap(&mut self) -> Selector {
        self.alloc_caps(1)
    }
    pub fn alloc_caps(&mut self, count: u32) -> Selector {
        self.next_sel += count;
        self.next_sel - count
    }

    pub fn alloc_ep(&mut self) -> Result<EpId, Error> {
        for ep in 0..EP_COUNT {
            if self.is_ep_free(ep) {
                self.eps |= 1 << ep;
                return Ok(ep)
            }
        }
        Err(Error::NoSpace)
    }

    pub fn is_ep_free(&self, ep: EpId) -> bool {
        (self.eps & (1 << ep)) == 0
    }

    pub fn free_ep(&mut self, ep: EpId) {
        self.eps &= !(1 << ep);
    }

    pub fn rbufs(&mut self) -> &mut RBufSpace {
        &mut self.rbufs
    }

    pub fn delegate_obj(&mut self, sel: Selector) -> Result<(), Error> {
        self.delegate(CapRngDesc::new_from(cap::Type::OBJECT, sel, 1))
    }
    pub fn delegate(&mut self, crd: CapRngDesc) -> Result<(), Error> {
        let start = crd.start();
        self.delegate_to(crd, start)
    }
    pub fn delegate_to(&mut self, crd: CapRngDesc, dst: Selector) -> Result<(), Error> {
        try!(syscalls::exchange(self.sel(), crd, dst, false));
        self.next_sel = util::max(self.next_sel, dst + crd.count());
        Ok(())
    }

    pub fn obtain(&mut self, crd: CapRngDesc) -> Result<(), Error> {
        let count = crd.count();
        let start = self.alloc_caps(count);
        self.obtain_to(crd, start)
    }

    pub fn obtain_to(&mut self, crd: CapRngDesc, dst: Selector) -> Result<(), Error> {
        let own = CapRngDesc::new_from(crd.cap_type(), dst, crd.count());
        syscalls::exchange(self.sel(), own, crd.start(), true)
    }

    pub fn revoke(&mut self, crd: CapRngDesc, del_only: bool) -> Result<(), Error> {
        syscalls::revoke(self.sel(), crd, !del_only)
    }

    pub fn run<F>(&mut self, func: Box<F>) -> Result<ClosureActivity, Error>
                  where F: FnBox() -> i32, F: Send + 'static {
        extern {
            static _text_start: u8;
            static _text_end: u8;
            static _data_start: u8;
            static _bss_end: u8;
            static heap_end: usize;
        }

        const RT_START: usize   = 0x6000;
        const STACK_TOP: usize  = 0xC000;

        let addr = |sym: &u8| {
            (sym as *const u8) as usize
        };
        let get_sp = || {
            let res: usize;
            unsafe {
                asm!("mov %rsp, $0" : "=r"(res));
            }
            res
        };

        let sp = get_sp();

        unsafe {
            // copy text
            let text_start = addr(&_text_start);
            let text_end = addr(&_text_end);
            try!(self.mem.write_bytes(&_text_start, text_end - text_start, text_start));

            // copy data and heap
            let data_start = addr(&_data_start);
            try!(self.mem.write_bytes(&_data_start, libc::heap_used_end() - data_start, data_start));

            // copy end-area of heap
            let heap_area_size = util::size_of::<heap::HeapArea>();
            try!(self.mem.write_bytes(heap_end as *const u8, heap_area_size, heap_end));

            // copy stack
            try!(self.mem.write_bytes(sp as *const u8, STACK_TOP - sp, sp));
        }

        let env = env::data();
        let mut senv = env::EnvData::new();

        senv.sp = get_sp() as u64;
        senv.entry = unsafe { addr(&_text_start) } as u64;
        senv.lambda = 1;
        senv.exit_addr = 0;

        senv.pedesc = self.pe();
        senv.heap_size = env.heap_size;

        senv.rbuf_cur = self.rbufs.cur as u64;
        senv.rbuf_end = self.rbufs.end as u64;
        senv.caps = self.next_sel as u64;

        // TODO
        // senv.mounts_len = 0;
        // senv.mounts = reinterpret_cast<uintptr_t>(_ms);
        // senv.fds_len = 0;
        // senv.fds = reinterpret_cast<uintptr_t>(_fds);
        // senv.eps = reinterpret_cast<uintptr_t>(_eps);
        // senv.pager_sgate = 0;
        // senv.pager_rgate = 0;
        // senv.pager_sess = 0;
        // senv._backend = env()->_backend;

        let mut off = RT_START;
        // env goes first
        off += util::size_of_val(&senv);

        // create and write closure
        let closure = env::Closure::new(func);
        try!(self.mem.write_obj(&closure, off));
        off += util::size_of_val(&closure);

        // write args
        senv.argc = env.argc;
        senv.argv = try!(self.write_arguments(off, env::args())) as u64;

        // write start env to PE
        try!(self.mem.write_obj(&senv, RT_START));

        // go!
        let act = ClosureActivity::new(self, closure);
        act.start().map(|_| act)
    }

    fn write_arguments<I, S>(&self, off: usize, args: I) -> Result<usize, Error>
                             where I: iter::IntoIterator<Item = S>, S: AsRef<str> {
        let mut argptr = Vec::new();
        let mut argbuf = Vec::new();

        let mut argoff = off;
        for s in args {
            // push argv entry
            argptr.push(argoff);

            // push string
            let arg = s.as_ref().as_bytes();
            argbuf.extend_from_slice(arg);

            // 0-terminate it
            argbuf.push('\0' as u8);

            argoff += arg.len() + 1;
        }

        try!(self.mem.write(&argbuf, off));
        try!(self.mem.write(&argptr, argoff));
        Ok(argoff)
    }
}

pub fn init() {
    unsafe {
        CUR = Some(VPE::new_cur());
    }

    for _ in 0..FIRST_FREE_EP {
        VPE::cur().alloc_ep().unwrap();
    }
}

pub mod tests {
    use super::*;
    use com::*;

    pub fn run(t: &mut ::test::Tester) {
        run_test!(t, run_arguments);
        run_test!(t, run_send_receive);
    }

    fn run_arguments() {
        let mut vpe = assert_ok!(VPE::new_with(VPEArgs::new("test")));

        let act = assert_ok!(vpe.run(Box::new(|| {
            assert_eq!(env::args().count(), 1);
            assert_eq!(env::args().nth(0), Some("rustunittests"));
            0
        })));

        assert_eq!(act.wait(), Ok(0));
    }

    fn run_send_receive() {
        let mut vpe = assert_ok!(VPE::new_with(VPEArgs::new("test")));

        let mut rgate = assert_ok!(RecvGate::new(util::next_log2(256), util::next_log2(256)));
        let sgate = assert_ok!(SendGate::new_with(SGateArgs::new(&rgate).credits(256)));

        assert_ok!(vpe.delegate_obj(rgate.sel()));

        let act = assert_ok!(vpe.run(Box::new(move || {
            assert_ok!(rgate.activate());
            let (i1, i2) = assert_ok!(recv_vmsg!(&rgate, u32, u32));
            assert_eq!((i1, i2), (42, 23));
            (i1 + i2) as i32
        })));

        assert_ok!(send_vmsg!(&sgate, RecvGate::def(), 42, 23));

        assert_eq!(act.wait(), Ok(42 + 23));
    }
}
