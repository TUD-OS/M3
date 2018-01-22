use arch;
use boxed::{Box, FnBox};
use cap::{CapFlags, Capability, Selector};
use cell::StaticCell;
use col::Vec;
use com::MemGate;
use core::fmt;
use dtu::{EP_COUNT, FIRST_FREE_EP, EpId};
use env;
use errors::{Code, Error};
use kif::{CapType, CapRngDesc, INVALID_SEL, PEDesc};
use kif;
use session::Pager;
use syscalls;
use util;
use vfs::{BufReader, FileRef, OpenFlags, VFS};
use vfs::{FileTable, MountTable};

pub struct VPE {
    cap: Capability,
    pe: PEDesc,
    mem: MemGate,
    next_sel: Selector,
    eps: u64,
    rbufs: arch::rbufs::RBufSpace,
    pager: Option<Pager>,
    files: FileTable,
    mounts: MountTable,
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
        syscalls::vpe_ctrl(self.vpe().sel(), kif::syscalls::VPEOp::START, 0).map(|_| ())
    }

    fn stop(&self) -> Result<(), Error> {
        syscalls::vpe_ctrl(self.vpe().sel(), kif::syscalls::VPEOp::STOP, 0).map(|_| ())
    }

    fn wait(&self) -> Result<i32, Error> {
        syscalls::vpe_ctrl(self.vpe().sel(), kif::syscalls::VPEOp::WAIT, 0)
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
    _file: BufReader<FileRef>,
}

impl<'v> ExecActivity<'v> {
    pub fn new<'vo : 'v>(vpe: &'vo mut VPE, file: BufReader<FileRef>) -> ExecActivity<'v> {
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

static CUR: StaticCell<Option<VPE>> = StaticCell::new(None);

impl VPE {
    fn new_cur() -> Self {
        // currently, the bitmask limits us to 64 endpoints
        const_assert!(EP_COUNT < util::size_of::<u64>() * 8);

        VPE {
            cap: Capability::new(0, CapFlags::KEEP_CAP),
            pe: PEDesc::new_from(0),
            mem: MemGate::new_bind(1),
            // 0 and 1 are reserved for VPE cap and mem cap
            next_sel: 2,
            eps: 0,
            rbufs: arch::rbufs::RBufSpace::new(),
            pager: None,
            files: FileTable::default(),
            mounts: MountTable::default(),
        }
    }

    fn init(&mut self) {
        let env = arch::env::get();
        self.pe = env.pe_desc();
        let (caps, eps) = env.load_caps_eps();
        self.next_sel = caps;
        self.eps = eps;
        self.rbufs = env.load_rbufs();
        self.pager = env.load_pager();
        // mounts first; files depend on mounts
        self.mounts = env.load_mounts();
        self.files = env.load_fds();
    }

    pub fn cur() -> &'static mut VPE {
        if arch::env::get().has_vpe() {
            arch::env::get().vpe()
        }
        else {
            CUR.get_mut().as_mut().unwrap()
        }
    }

    pub fn new(name: &str) -> Result<Self, Error> {
        Self::new_with(VPEArgs::new(name))
    }

    pub fn new_with(args: VPEArgs) -> Result<Self, Error> {
        let sels = VPE::cur().alloc_sels(2);

        let mut vpe = VPE {
            cap: Capability::new(sels + 0, CapFlags::empty()),
            pe: args.pe,
            mem: MemGate::new_bind(sels + 1),
            next_sel: 2,
            eps: 0,
            rbufs: arch::rbufs::RBufSpace::new(),
            pager: None,
            files: FileTable::default(),
            mounts: MountTable::default(),
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

            // now create VPE, which implicitly obtains the gate cap from us
            vpe.pe = syscalls::create_vpe(
                vpe.sel(), vpe.mem().sel(), sgate_sel, args.name,
                args.pe, vpe.alloc_ep()?, pg.rep(), args.muxable
            )?;

            // after the VPE creation, we can activate the receive gate
            // note that we do that here in case neither run nor exec is used
            pg.activate(vpe.sel())?;

            // mark the pager caps allocated
            vpe.next_sel = util::max(sgate_sel + 1, vpe.next_sel);
            // now delegate our VPE cap and memory cap to the pager
            pg.delegate_caps(&vpe)?;
            // and delegate the pager cap to the VPE
            vpe.delegate_obj(pg.sel())?;
            Some(pg)
        }
        else {
            vpe.pe = syscalls::create_vpe(
                sels + 0, sels + 1, INVALID_SEL, args.name,
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
    pub fn pe_id(&self) -> u64 {
        arch::env::get().pe_id()
    }
    pub fn mem(&self) -> &MemGate {
        &self.mem
    }

    pub(crate) fn rbufs(&mut self) -> &mut arch::rbufs::RBufSpace {
        &mut self.rbufs
    }

    pub fn files(&mut self) -> &mut FileTable {
        &mut self.files
    }
    pub fn mounts(&mut self) -> &mut MountTable {
        &mut self.mounts
    }

    pub fn pager(&self) -> Option<&Pager> {
        self.pager.as_ref()
    }

    pub fn alloc_sel(&mut self) -> Selector {
        self.alloc_sels(1)
    }
    pub fn alloc_sels(&mut self, count: u32) -> Selector {
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
        Err(Error::new(Code::NoSpace))
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
    pub fn free_rbuf(&mut self, addr: usize, size: usize) {
        self.rbufs.free(addr, size)
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
        let start = self.alloc_sels(count);
        self.obtain_to(crd, start)
    }

    pub fn obtain_to(&mut self, crd: CapRngDesc, dst: Selector) -> Result<(), Error> {
        let own = CapRngDesc::new(crd.cap_type(), dst, crd.count());
        syscalls::exchange(self.sel(), own, crd.start(), true)
    }

    pub fn revoke(&mut self, crd: CapRngDesc, del_only: bool) -> Result<(), Error> {
        syscalls::revoke(self.sel(), crd, !del_only)
    }

    pub fn obtain_fds(&mut self) -> Result<(), Error> {
        let mut caps = Vec::new();
        self.files.collect_caps(&mut caps);
        for c in caps {
            self.delegate_obj(c)?;
        }
        Ok(())
    }
    pub fn obtain_mounts(&mut self) -> Result<(), Error> {
        let mut caps = Vec::new();
        self.mounts.collect_caps(&mut caps);
        for c in caps {
            self.delegate_obj(c)?;
        }
        Ok(())
    }

    #[cfg(target_os = "none")]
    pub fn run<F>(&mut self, func: Box<F>) -> Result<ClosureActivity, Error>
                  where F: FnBox() -> i32, F: Send + 'static {
        use cfg;

        let sel = self.sel();
        if let Some(ref mut pg) = self.pager {
            pg.activate(sel)?;
        }

        let env = arch::env::get();
        let mut senv = arch::env::EnvData::default();

        let closure = {
            let mut loader = arch::loader::Loader::new(
                self.pager.as_ref(),
                Self::cur().pager().is_some(),
                &self.mem
            );

            // copy all regions to child
            senv.set_sp(arch::loader::get_sp());
            let entry = loader.copy_regions(senv.sp())?;
            senv.set_entry(entry);
            senv.set_heap_size(env.heap_size());
            senv.set_lambda(true);

            // store VPE address to reuse it in the child
            senv.set_vpe(self);

            // env goes first
            let mut off = cfg::RT_START + util::size_of_val(&senv);

            // create and write closure
            let closure = env::Closure::new(func);
            self.mem.write_obj(&closure, off)?;
            off += util::size_of_val(&closure);

            // write args
            senv.set_argc(env.argc());
            senv.set_argv(loader.write_arguments(&mut off, env::args())?);

            senv.set_pedesc(&self.pe());

            // write start env to PE
            self.mem.write_obj(&senv, cfg::RT_START)?;

            closure
        };

        // go!
        let act = ClosureActivity::new(self, closure);
        act.start().map(|_| act)
    }

    #[cfg(target_os = "linux")]
    pub fn run<F>(&mut self, func: Box<F>) -> Result<ClosureActivity, Error>
                  where F: FnBox() -> i32, F: Send + 'static {
        use libc;

        let mut closure = env::Closure::new(func);

        let mut chan = arch::loader::Channel::new()?;

        match unsafe { libc::fork() } {
            -1  => {
                Err(Error::new(Code::OutOfMem))
            },

            0   => {
                chan.wait();

                arch::env::reinit();
                arch::env::get().set_vpe(self);
                ::io::reinit();
                self::reinit();
                ::com::reinit();
                arch::dtu::init();

                let res = closure.call();
                unsafe { libc::exit(res) };
            },

            pid => {
                // let the kernel create the config-file etc. for the given pid
                syscalls::vpe_ctrl(self.sel(), kif::syscalls::VPEOp::START, pid as u64).unwrap();

                chan.signal();

                Ok(ClosureActivity::new(self, closure))
            },
        }
    }

    #[cfg(target_os = "none")]
    pub fn exec<S: AsRef<str>>(&mut self, args: &[S]) -> Result<ExecActivity, Error> {
        use cfg;
        use serialize::Sink;
        use com::VecSink;

        let file = VFS::open(args[0].as_ref(), OpenFlags::RX)?;
        let mut file = BufReader::new(file);

        let sel = self.sel();
        if let Some(ref mut pg) = self.pager {
            pg.activate(sel)?;
        }

        let mut senv = arch::env::EnvData::default();

        {
            let loader = arch::loader::Loader::new(
                self.pager.as_ref(),
                Self::cur().pager().is_some(),
                &self.mem
            );

            // load program segments
            senv.set_sp(cfg::STACK_TOP);
            senv.set_entry(loader.load_program(&mut file)?);

            // write args
            let mut off = cfg::RT_START + util::size_of_val(&senv);
            senv.set_argc(args.len());
            senv.set_argv(loader.write_arguments(&mut off, args)?);

            // write file table
            {
                let mut fds = VecSink::new();
                self.files.serialize(&self.mounts, &mut fds);
                self.mem.write(fds.words(), off)?;
                senv.set_files(off, fds.size());
                off += fds.size();
            }

            // write mounts table
            {
                let mut mounts = VecSink::new();
                self.mounts.serialize(&mut mounts);
                self.mem.write(mounts.words(), off)?;
                senv.set_mounts(off, mounts.size());
            }

            senv.set_rbufs(&self.rbufs);
            senv.set_next_sel(self.next_sel);
            senv.set_eps(self.eps);
            senv.set_pedesc(&self.pe());

            if let Some(ref pg) = self.pager {
                senv.set_pager(pg);
                senv.set_heap_size(cfg::APP_HEAP_SIZE);
            }

            // write start env to PE
            self.mem.write_obj(&senv, cfg::RT_START)?;
        }

        // go!
        let act = ExecActivity::new(self, file);
        act.start().map(|_| act)
    }

    #[cfg(target_os = "linux")]
    pub fn exec<S: AsRef<str>>(&mut self, args: &[S]) -> Result<ExecActivity, Error> {
        use com::VecSink;
        use libc;
        use serialize::Sink;

        let mut file = VFS::open(args[0].as_ref(), OpenFlags::RX)?;
        let path = arch::loader::copy_file(&mut file)?;

        let mut chan = arch::loader::Channel::new()?;

        match unsafe { libc::fork() } {
            -1  => {
                Err(Error::new(Code::OutOfMem))
            },

            0   => {
                chan.wait();

                let pid = unsafe { libc::getpid() };

                // write sels and EPs
                let mut other = VecSink::new();
                other.push(&self.next_sel);
                other.push(&self.eps);
                arch::loader::write_env_file(pid, "other", other.words(), other.size());

                // write file table
                let mut fds = VecSink::new();
                self.files.serialize(&self.mounts, &mut fds);
                arch::loader::write_env_file(pid, "fds", fds.words(), fds.size());

                // write mounts table
                let mut mounts = VecSink::new();
                self.mounts.serialize(&mut mounts);
                arch::loader::write_env_file(pid, "ms", mounts.words(), mounts.size());

                arch::loader::exec(args, &path);
            },

            pid => {
                // let the kernel create the config-file etc. for the given pid
                syscalls::vpe_ctrl(self.sel(), kif::syscalls::VPEOp::START, pid as u64).unwrap();

                chan.signal();

                Ok(ExecActivity::new(self, BufReader::new(file)))
            },
        }
    }
}

impl fmt::Debug for VPE {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(f, "VPE[sel: {}, pe: {:?}]", self.sel(), self.pe())
    }
}

pub fn init() {
    CUR.set(Some(VPE::new_cur()));
    VPE::cur().init();
}

pub fn reinit() {
    VPE::cur().cap.set_flags(CapFlags::KEEP_CAP);
    VPE::cur().cap = Capability::new(0, CapFlags::KEEP_CAP);
    VPE::cur().mem = MemGate::new_bind(1);
}
