use alloc::boxed::FnBox;
use boxed::Box;
use cap::{CapFlags, Capability, Selector};
use cell::RefCell;
use cfg;
use com::{MemGate, RBufSpace};
use collections::Vec;
use core::{fmt, iter, intrinsics};
use dtu::{EP_COUNT, FIRST_FREE_EP, EpId};
use env;
use elf;
use errors::Error;
use heap;
use kif;
use kif::{CapType, CapRngDesc, INVALID_SEL, PEDesc};
use rc::Rc;
use session::M3FS;
use session::{Pager, Map};
use vfs::{BufReader, FileSystem, OpenFlags, Read, RegularFile, Seek, SeekMode};
use libc;
use syscalls;
use util;

pub struct VPE {
    cap: Capability,
    pe: PEDesc,
    mem: MemGate,
    next_sel: Selector,
    eps: u64,
    rbufs: RBufSpace,
    pager: Option<Pager>,
}

pub struct VPEArgs<'n, 'p> {
    name: &'n str,
    pager: Option<&'p str>,
    pe: PEDesc,
    muxable: bool,
}

pub trait Activity {
    fn vpe(&self) -> &VPE;

    fn start(&self) -> Result<(), Error> {
        syscalls::vpe_ctrl(self.vpe().sel(), kif::syscalls::VPEOp::Start, 0).map(|_| ())
    }

    fn stop(&self) -> Result<(), Error> {
        syscalls::vpe_ctrl(self.vpe().sel(), kif::syscalls::VPEOp::Stop, 0).map(|_| ())
    }

    fn wait(&self) -> Result<i32, Error> {
        syscalls::vpe_ctrl(self.vpe().sel(), kif::syscalls::VPEOp::Wait, 0)
    }
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
    fn vpe(&self) -> &VPE {
        self.vpe
    }
}

impl<'v> Drop for ClosureActivity<'v> {
    fn drop(&mut self) {
        self.stop().ok();
        if let Some(ref mut pg) = self.vpe.pager {
            pg.deactivate();
        }
    }
}

pub struct ExecActivity<'v> {
    vpe: &'v mut VPE,
    _file: BufReader<RegularFile>,
}

impl<'v> ExecActivity<'v> {
    pub fn new<'vo : 'v>(vpe: &'vo mut VPE, file: BufReader<RegularFile>) -> ExecActivity<'v> {
        ExecActivity {
            vpe: vpe,
            _file: file,
        }
    }
}

impl<'v> Activity for ExecActivity<'v> {
    fn vpe(&self) -> &VPE {
        self.vpe
    }
}

impl<'v> Drop for ExecActivity<'v> {
    fn drop(&mut self) {
        self.stop().ok();
        if let Some(ref mut pg) = self.vpe.pager {
            pg.deactivate();
        }
    }
}

impl<'n, 'p> VPEArgs<'n, 'p> {
    pub fn new(name: &'n str) -> VPEArgs<'n, 'p> {
        VPEArgs {
            name: name,
            pager: None,
            pe: VPE::cur().pe(),
            muxable: false,
        }
    }

    pub fn pe(mut self, pe: PEDesc) -> Self {
        self.pe = pe;
        self
    }

    pub fn pager(mut self, pager: &'p str) -> Self {
        self.pager = Some(pager);
        self
    }

    pub fn muxable(mut self, muxable: bool) -> Self {
        self.muxable = muxable;
        self
    }
}

const VMA_RBUF_SIZE: usize  = 64;

static mut CUR: Option<VPE> = None;

impl VPE {
    fn new_cur() -> Self {
        let env = env::data();
        VPE {
            cap: Capability::new(0, CapFlags::KEEP_CAP),
            pe: env.pedesc.clone(),
            mem: MemGate::new_bind(1),
            // 0 and 1 are reserved for VPE cap and mem cap
            // it's initially 0. make sure it's at least the first usable selector
            next_sel: util::max(2, env.caps as Selector),
            eps: env.eps,
            rbufs: RBufSpace::new(env.rbuf_cur as usize, env.rbuf_end as usize),
            pager: match env.pager_sess {
                0 => None,
                s => Some(Pager::new_bind(s, env.pager_sgate, env.pager_rgate)),
            },
        }
    }

    pub fn cur() -> &'static mut VPE {
        unsafe {
            match CUR {
                Some(ref mut v) => v,
                None            => intrinsics::transmute(env::data().fds)
            }
        }
    }

    pub fn new(name: &str) -> Result<Self, Error> {
        Self::new_with(VPEArgs::new(name))
    }

    pub fn new_with(args: VPEArgs) -> Result<Self, Error> {
        let sels = VPE::cur().alloc_caps(2);

        let mut vpe = VPE {
            cap: Capability::new(sels + 0, CapFlags::empty()),
            pe: args.pe,
            mem: MemGate::new_bind(sels + 1),
            next_sel: 2,
            eps: 0,
            rbufs: RBufSpace::new(0, 0),
            pager: None,
        };

        let rbuf = if args.pe.has_mmu() {
            vpe.alloc_rbuf(util::next_log2(VMA_RBUF_SIZE) as usize)?
        }
        else {
            0
        };

        let pager = if args.pe.has_virtmem() {
            if let Some(p) = args.pager {
                Some(Pager::new(&mut vpe, rbuf, p)?)
            }
            else if let Some(p) = Self::cur().pager() {
                Some(p.new_clone(&mut vpe, rbuf)?)
            }
            else {
                None
            }
        }
        else {
            None
        };

        vpe.pager = if let Some(mut pg) = pager {
            let sgate_sel = pg.sgate().sel();
            let rgate_sel = match pg.rgate() {
                Some(rg) => rg.sel(),
                None     => INVALID_SEL,
            };

            // now create VPE, which implicitly obtains the gate cap from us
            vpe.pe = syscalls::create_vpe(
                vpe.sel(), vpe.mem().sel(), sgate_sel, rgate_sel, args.name,
                args.pe, vpe.alloc_ep()?, pg.rep(), args.muxable
            )?;

            // after the VPE creation, we can activate the receive gate
            // note that we do that here in case neither run nor exec is used
            pg.activate(vpe.sel())?;

            // mark the pager caps allocated
            vpe.next_sel = util::max(sgate_sel + 1, vpe.next_sel);
            if rgate_sel != INVALID_SEL {
                vpe.next_sel = util::max(rgate_sel + 1, vpe.next_sel);
            }
            // now delegate our VPE cap and memory cap to the pager
            pg.delegate_caps(&vpe)?;
            // and delegate the pager cap to the VPE
            vpe.delegate_obj(pg.sel())?;
            Some(pg)
        }
        else {
            vpe.pe = syscalls::create_vpe(
                sels + 0, sels + 1, INVALID_SEL, INVALID_SEL, args.name,
                args.pe, 0, 0, args.muxable
            )?;
            None
        };

        Ok(vpe)
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

    pub fn pager(&self) -> Option<&Pager> {
        self.pager.as_ref()
    }

    pub fn alloc_cap(&mut self) -> Selector {
        self.alloc_caps(1)
    }
    pub fn alloc_caps(&mut self, count: u32) -> Selector {
        self.next_sel += count;
        self.next_sel - count
    }

    pub fn alloc_ep(&mut self) -> Result<EpId, Error> {
        for ep in FIRST_FREE_EP..EP_COUNT {
            if self.is_ep_free(ep) {
                self.eps |= 1 << ep;
                return Ok(ep)
            }
        }
        Err(Error::NoSpace)
    }

    pub fn is_ep_free(&self, ep: EpId) -> bool {
        ep >= FIRST_FREE_EP && (self.eps & (1 << ep)) == 0
    }

    pub fn free_ep(&mut self, ep: EpId) {
        self.eps &= !(1 << ep);
    }

    pub fn alloc_rbuf(&mut self, size: usize) -> Result<usize, Error> {
        self.rbufs.alloc(&self.pe, size)
    }
    pub fn free_rbuf(&mut self, addr: usize) {
        self.rbufs.free(addr)
    }

    pub fn delegate_obj(&mut self, sel: Selector) -> Result<(), Error> {
        self.delegate(CapRngDesc::new(CapType::OBJECT, sel, 1))
    }
    pub fn delegate(&mut self, crd: CapRngDesc) -> Result<(), Error> {
        let start = crd.start();
        self.delegate_to(crd, start)
    }
    pub fn delegate_to(&mut self, crd: CapRngDesc, dst: Selector) -> Result<(), Error> {
        syscalls::exchange(self.sel(), crd, dst, false)?;
        self.next_sel = util::max(self.next_sel, dst + crd.count());
        Ok(())
    }

    pub fn obtain(&mut self, crd: CapRngDesc) -> Result<(), Error> {
        let count = crd.count();
        let start = self.alloc_caps(count);
        self.obtain_to(crd, start)
    }

    pub fn obtain_to(&mut self, crd: CapRngDesc, dst: Selector) -> Result<(), Error> {
        let own = CapRngDesc::new(crd.cap_type(), dst, crd.count());
        syscalls::exchange(self.sel(), own, crd.start(), true)
    }

    pub fn revoke(&mut self, crd: CapRngDesc, del_only: bool) -> Result<(), Error> {
        syscalls::revoke(self.sel(), crd, !del_only)
    }

    pub fn run<F>(&mut self, func: Box<F>) -> Result<ClosureActivity, Error>
                  where F: FnBox() -> i32, F: Send + 'static {
        let get_sp = || {
            let res: usize;
            unsafe {
                asm!("mov %rsp, $0" : "=r"(res));
            }
            res
        };

        let sel = self.sel();
        if let Some(ref mut pg) = self.pager {
            pg.activate(sel)?;
        }

        let env = env::data();
        let mut senv = env::EnvData::default();

        senv.sp = get_sp() as u64;
        senv.entry = self.copy_regions(senv.sp as usize)? as u64;
        senv.lambda = 1;
        senv.exit_addr = 0;

        senv.heap_size = env.heap_size;

        senv.fds_len = 0;
        senv.fds = self as *const VPE as u64;

        // TODO
        // senv.mounts_len = 0;
        // senv.mounts = reinterpret_cast<uintptr_t>(_ms);
        // senv.fds_len = 0;
        // senv.fds = reinterpret_cast<uintptr_t>(_fds);
        // senv._backend = env()->_backend;

        // env goes first
        let mut off = cfg::RT_START + util::size_of_val(&senv);

        // create and write closure
        let closure = env::Closure::new(func);
        self.mem.write_obj(&closure, off)?;
        off += util::size_of_val(&closure);

        // write args
        senv.argc = env.argc;
        senv.argv = self.write_arguments(off, env::args())? as u64;

        // write start env to PE
        self.mem.write_obj(&senv, cfg::RT_START)?;

        // go!
        let act = ClosureActivity::new(self, closure);
        act.start().map(|_| act)
    }

    pub fn exec<S: AsRef<str>>(&mut self, fs: Rc<RefCell<M3FS>>,
                               args: &[S]) -> Result<ExecActivity, Error> {
        let file = fs.borrow_mut().open(args[0].as_ref(), OpenFlags::RX)?;
        let mut file = BufReader::new(file);

        let sel = self.sel();
        if let Some(ref mut pg) = self.pager {
            pg.activate(sel)?;
        }

        let mut senv = env::EnvData::default();

        senv.entry = self.load_program(&mut file)? as u64;
        senv.lambda = 0;
        senv.exit_addr = 0;
        senv.sp = cfg::STACK_TOP as u64;

        // write args
        let argoff = cfg::RT_START + util::size_of_val(&senv);
        senv.argc = args.len() as u32;
        senv.argv = self.write_arguments(argoff, args)? as u64;

        // TODO mounts, fds

        senv.rbuf_cur = self.rbufs.cur as u64;
        senv.rbuf_end = self.rbufs.end as u64;
        senv.caps = self.next_sel as u64;
        senv.eps = self.eps;
        senv.pedesc = self.pe();

        if let Some(ref pg) = self.pager {
            senv.pager_sess = pg.sel();
            senv.pager_sgate = pg.sgate().sel();
            senv.pager_rgate = match pg.rgate() {
                Some(rg) => rg.sel(),
                None     => INVALID_SEL,
            };
            senv.heap_size = cfg::APP_HEAP_SIZE as u64;
        }

        // write start env to PE
        self.mem.write_obj(&senv, cfg::RT_START)?;

        // go!
        let act = ExecActivity::new(self, file);
        act.start().map(|_| act)
    }

    fn copy_regions(&mut self, sp: usize) -> Result<usize, Error> {
        extern {
            static _text_start: u8;
            static _text_end: u8;
            static _data_start: u8;
            static _bss_end: u8;
            static heap_end: usize;
        }

        let addr = |sym: &u8| {
            (sym as *const u8) as usize
        };

        // use COW if both have a pager
        if let Some(ref pg) = self.pager {
            if Self::cur().pager().is_some() {
                return pg.clone().map(|_| unsafe { addr(&_text_start) })
            }
            // TODO handle that case
            unimplemented!();
        }

        unsafe {
            // copy text
            let text_start = addr(&_text_start);
            let text_end = addr(&_text_end);
            self.mem.write_bytes(&_text_start, text_end - text_start, text_start)?;

            // copy data and heap
            let data_start = addr(&_data_start);
            self.mem.write_bytes(&_data_start, libc::heap_used_end() - data_start, data_start)?;

            // copy end-area of heap
            let heap_area_size = util::size_of::<heap::HeapArea>();
            self.mem.write_bytes(heap_end as *const u8, heap_area_size, heap_end)?;

            // copy stack
            self.mem.write_bytes(sp as *const u8, cfg::STACK_TOP - sp, sp)?;

            Ok(text_start)
        }
    }

    fn clear_mem(&self, buf: &mut [u8], mut count: usize, mut dst: usize) -> Result<(), Error> {
        if count == 0 {
            return Ok(())
        }

        for i in 0..buf.len() {
            buf[i] = 0;
        }

        while count > 0 {
            let amount = util::min(count, buf.len());
            self.mem.write(&buf[0..amount], dst)?;
            count -= amount;
            dst += amount;
        }

        Ok(())
    }

    fn load_segment(&self, file: &mut BufReader<RegularFile>,
                    phdr: &elf::Phdr, buf: &mut [u8]) -> Result<(), Error> {
        file.seek(phdr.offset, SeekMode::SET)?;

        let mut count = phdr.filesz;
        let mut segoff = phdr.vaddr;
        while count > 0 {
            let amount = util::min(count, buf.len());
            let amount = file.read(&mut buf[0..amount])?;

            self.mem.write(&buf[0..amount], segoff)?;

            count -= amount;
            segoff += amount;
        }

        self.clear_mem(buf, phdr.memsz - phdr.filesz, segoff)
    }

    fn map_segment(&self, file: &mut BufReader<RegularFile>, pager: &Pager,
                   phdr: &elf::Phdr) -> Result<(), Error> {
        let mut prot = kif::Perm::empty();
        if (phdr.flags & elf::PF::R.bits()) != 0 {
            prot |= kif::Perm::R;
        }
        if (phdr.flags & elf::PF::W.bits()) != 0 {
            prot |= kif::Perm::W;
        }
        if (phdr.flags & elf::PF::X.bits()) != 0 {
            prot |= kif::Perm::X;
        }

        let size = util::round_up(phdr.memsz, cfg::PAGE_SIZE);
        if phdr.memsz == phdr.filesz {
            file.get_ref().map(pager, phdr.vaddr, phdr.offset, size, prot)
        }
        else {
            assert!(phdr.filesz == 0);
            pager.map_anon(phdr.vaddr, size, prot).map(|_| ())
        }
    }

    fn load_program(&self, file: &mut BufReader<RegularFile>) -> Result<usize, Error> {
        let mut buf = vec![0u8; 4096];
        let hdr: elf::Ehdr = file.read_object()?;

        if hdr.ident[0] != '\x7F' as u8 ||
           hdr.ident[1] != 'E' as u8 ||
           hdr.ident[2] != 'L' as u8 ||
           hdr.ident[3] != 'F' as u8 {
            return Err(Error::InvalidElf)
        }

        // copy load segments to destination PE
        let mut end = 0;
        let mut off = hdr.phoff;
        for _ in 0..hdr.phnum {
            // load program header
            file.seek(off, SeekMode::SET)?;
            let phdr: elf::Phdr = file.read_object()?;

            // we're only interested in non-empty load segments
            if phdr.ty != elf::PT::LOAD.val || phdr.memsz == 0 {
                continue;
            }

            if let Some(ref pg) = self.pager {
                self.map_segment(file, pg, &phdr)?;
            }
            else {
                self.load_segment(file, &phdr, &mut *buf)?;
            }
            off += hdr.phentsize as usize;

            end = phdr.vaddr + phdr.memsz;
        }

        if let Some(ref pg) = self.pager {
            // create area for boot/runtime stuff
            pg.map_anon(cfg::RT_START, cfg::RT_SIZE, kif::Perm::RW)?;

            // create area for stack
            pg.map_anon(cfg::STACK_BOTTOM, cfg::STACK_SIZE, kif::Perm::RW)?;

            // create heap
            pg.map_anon(util::round_up(end, cfg::PAGE_SIZE), cfg::APP_HEAP_SIZE, kif::Perm::RW)?;
        }

        Ok(hdr.entry)
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

        self.mem.write(&argbuf, off)?;
        self.mem.write(&argptr, argoff)?;
        Ok(argoff)
    }
}

impl fmt::Debug for VPE {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(f, "VPE[sel: {}, pe: {:?}]", self.sel(), self.pe())
    }
}

pub fn init() {
    unsafe {
        CUR = Some(VPE::new_cur());
    }
}

pub fn reinit() {
    unsafe {
        CUR = None;
        VPE::cur().cap = Capability::new(0, CapFlags::KEEP_CAP);
        VPE::cur().mem = MemGate::new_bind(1);
    }
}
