use arch;
use cap::Selector;
use com::SliceSource;
use core::intrinsics;
use env;
use kif::{INVALID_SEL, PEDesc};
use session::Pager;
use util;
use vfs::{FileTable, MountTable};
use vpe;

#[derive(Default)]
#[repr(C, packed)]
pub struct EnvData {
    pe: u64,
    argc: u32,
    argv: u64,

    sp: u64,
    entry: u64,
    lambda: u64,
    pager_sess: u32,
    pager_sgate: u32,
    pager_rgate: u32,
    mounts_len: u32,
    mounts: u64,
    fds_len: u32,
    fds: u64,
    rbuf_cur: u64,
    rbuf_end: u64,
    eps: u64,
    caps: u64,
    exit_addr: u64,
    heap_size: u64,

    _backend: u64,
    kenv: u64,

    pedesc: PEDesc,
    isrs: u64,
}

impl EnvData {
    pub fn pe_id(&self) -> u64 {
        self.pe
    }

    pub fn argc(&self) -> usize {
        self.argc as usize
    }
    pub fn argv(&self) -> *const *const i8 {
        self.argv as *const *const i8
    }

    pub fn pedesc<'a, 'e : 'a>(&'e self) -> &'a PEDesc {
        &self.pedesc
    }
    pub fn next_sel(&self) -> Selector {
        // it's initially 0. make sure it's at least the first usable selector
        util::max(2, self.caps as Selector)
    }
    pub fn eps(&self) -> u64 {
        self.eps
    }

    pub fn has_vpe(&self) -> bool {
        self.fds != 0
    }
    pub fn vpe(&self) -> &'static mut vpe::VPE {
        unsafe {
            intrinsics::transmute(self.fds)
        }
    }
    pub fn set_vpe(&mut self, vpe: &vpe::VPE) {
        self.fds = vpe as *const vpe::VPE as u64;
    }

    pub fn load_rbufs(&self) -> arch::rbufs::RBufSpace {
        arch::rbufs::RBufSpace::new_with(
            self.rbuf_cur as usize,
            self.rbuf_end as usize
        )
    }

    pub fn load_pager(&self) -> Option<Pager> {
        match self.pager_sess {
            0 => None,
            s => Some(Pager::new_bind(s, self.pager_sgate, self.pager_rgate)),
        }
    }

    pub fn load_mounts(&self) -> MountTable {
        if self.mounts_len != 0 {
            let slice = unsafe {
                util::slice_for(self.mounts as *const u64, self.mounts_len as usize)
            };
            MountTable::unserialize(&mut SliceSource::new(slice))
        }
        else {
            MountTable::default()
        }
    }

    pub fn load_fds(&self) -> FileTable {
        if self.fds_len != 0 {
            let slice = unsafe {
                util::slice_for(self.fds as *const u64, self.fds_len as usize)
            };
            FileTable::unserialize(&mut SliceSource::new(slice))
        }
        else {
            FileTable::default()
        }
    }

    // --- gem5 specific API ---

    pub fn sp(&self) -> usize {
        self.sp as usize
    }
    pub fn exit_addr(&self) -> usize {
        self.exit_addr as usize
    }

    pub fn has_lambda(&self) -> bool {
        self.lambda != 0
    }
    pub fn set_lambda(&mut self, lambda: bool) {
        self.lambda = lambda as u64;
    }

    pub fn heap_size(&self) -> usize {
        self.heap_size as usize
    }
    pub fn set_heap_size(&mut self, size: usize) {
        self.heap_size = size as u64;
    }

    pub fn set_pedesc(&mut self, pe: PEDesc) {
        self.pedesc = pe;
    }
    pub fn set_next_sel(&mut self, sel: Selector) {
        self.caps = sel as u64;
    }
    pub fn set_eps(&mut self, eps: u64) {
        self.eps = eps;
    }

    pub fn set_argc(&mut self, argc: usize) {
        self.argc = argc as u32;
    }
    pub fn set_argv(&mut self, argv: usize) {
        self.argv = argv as u64;
    }

    pub fn set_sp(&mut self, sp: usize) {
        self.sp = sp as u64;
    }
    pub fn set_entry(&mut self, entry: usize) {
        self.entry = entry as u64;
    }

    pub fn set_rbufs(&mut self, rbufs: &arch::rbufs::RBufSpace) {
        self.rbuf_cur = rbufs.cur as u64;
        self.rbuf_end = rbufs.end as u64;
    }

    pub fn set_files(&mut self, off: usize, len: usize) {
        self.fds = off as u64;
        self.fds_len = len as u32;
    }
    pub fn set_mounts(&mut self, off: usize, len: usize) {
        self.mounts = off as u64;
        self.mounts_len = len as u32;
    }

    pub fn set_pager(&mut self, pager: &Pager) {
        self.pager_sess = pager.sel();
        self.pager_sgate = pager.sgate().sel();
        self.pager_rgate = match pager.rgate() {
            Some(rg) => rg.sel(),
            None     => INVALID_SEL,
        };
    }
}

pub fn data() -> &'static mut EnvData {
    unsafe {
        intrinsics::transmute(0x6000 as u64)
    }
}

pub fn closure() -> &'static mut env::Closure {
    unsafe {
        intrinsics::transmute((0x6000 + util::size_of::<EnvData>()) as u64)
    }
}
