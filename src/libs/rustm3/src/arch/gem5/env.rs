use arch;
use base;
use cap::Selector;
use com::SliceSource;
use core::intrinsics;
use env;
use kif::{INVALID_SEL, PEDesc};
use session::Pager;
use util;
use vfs::{FileTable, MountTable};
use vpe;

#[derive(Default, Copy, Clone)]
#[repr(C, packed)]
pub struct EnvData {
    base: base::envdata::EnvData,
}

impl EnvData {
    pub fn pe_id(&self) -> u64 {
        self.base.pe_id
    }

    pub fn pe_desc(&self) -> PEDesc {
        PEDesc::new_from(self.base.pe_desc)
    }
    pub fn set_pedesc(&mut self, pe: &PEDesc) {
        self.base.pe_desc = pe.value();
    }

    pub fn argc(&self) -> usize {
        self.base.argc as usize
    }
    pub fn set_argc(&mut self, argc: usize) {
        self.base.argc = argc as u32;
    }
    pub fn set_argv(&mut self, argv: usize) {
        self.base.argv = argv as u64;
    }

    pub fn sp(&self) -> usize {
        self.base.sp as usize
    }
    pub fn set_sp(&mut self, sp: usize) {
        self.base.sp = sp as u64;
    }

    pub fn set_entry(&mut self, entry: usize) {
        self.base.entry = entry as u64;
    }

    pub fn heap_size(&self) -> usize {
        self.base.heap_size as usize
    }
    pub fn set_heap_size(&mut self, size: usize) {
        self.base.heap_size = size as u64;
    }

    pub fn has_vpe(&self) -> bool {
        self.base.vpe != 0
    }
    pub fn vpe(&self) -> &'static mut vpe::VPE {
        unsafe {
            intrinsics::transmute(self.base.vpe)
        }
    }

    pub fn load_rbufs(&self) -> arch::rbufs::RBufSpace {
        arch::rbufs::RBufSpace::new_with(
            self.base.rbuf_cur as usize,
            self.base.rbuf_end as usize
        )
    }

    pub fn load_pager(&self) -> Option<Pager> {
        match self.base.pager_sess {
            0 => None,
            s => Some(Pager::new_bind(s, self.base.pager_sgate, self.base.pager_rgate)),
        }
    }

    pub fn load_caps_eps(&self) -> (Selector, u64) {
        (
            // it's initially 0. make sure it's at least the first usable selector
            util::max(2, self.base.caps as Selector),
            self.base.eps
        )
    }

    pub fn load_mounts(&self) -> MountTable {
        if self.base.mounts_len != 0 {
            let slice = unsafe {
                util::slice_for(self.base.mounts as *const u64, self.base.mounts_len as usize)
            };
            MountTable::unserialize(&mut SliceSource::new(slice))
        }
        else {
            MountTable::default()
        }
    }

    pub fn load_fds(&self) -> FileTable {
        if self.base.fds_len != 0 {
            let slice = unsafe {
                util::slice_for(self.base.fds as *const u64, self.base.fds_len as usize)
            };
            FileTable::unserialize(&mut SliceSource::new(slice))
        }
        else {
            FileTable::default()
        }
    }

    // --- gem5 specific API ---

    pub fn set_vpe(&mut self, vpe: &vpe::VPE) {
        self.base.vpe = vpe as *const vpe::VPE as u64;
    }

    pub fn exit_addr(&self) -> usize {
        self.base.exit_addr as usize
    }

    pub fn has_lambda(&self) -> bool {
        self.base.lambda == 1
    }
    pub fn set_lambda(&mut self, lambda: bool) {
        self.base.lambda = lambda as u64;
    }

    pub fn set_next_sel(&mut self, sel: Selector) {
        self.base.caps = sel as u64;
    }
    pub fn set_eps(&mut self, eps: u64) {
        self.base.eps = eps;
    }

    pub fn set_rbufs(&mut self, rbufs: &arch::rbufs::RBufSpace) {
        self.base.rbuf_cur = rbufs.cur as u64;
        self.base.rbuf_end = rbufs.end as u64;
    }

    pub fn set_files(&mut self, off: usize, len: usize) {
        self.base.fds = off as u64;
        self.base.fds_len = len as u32;
    }
    pub fn set_mounts(&mut self, off: usize, len: usize) {
        self.base.mounts = off as u64;
        self.base.mounts_len = len as u32;
    }

    pub fn set_pager(&mut self, pager: &Pager) {
        self.base.pager_sess = pager.sel();
        self.base.pager_sgate = pager.child_sgate().sel();
        self.base.pager_rgate = match pager.rgate() {
            Some(rg) => rg.sel(),
            None     => INVALID_SEL,
        };
    }
}

pub fn get() -> &'static mut EnvData {
    unsafe {
        intrinsics::transmute(0x6000 as u64)
    }
}

pub fn closure() -> &'static mut env::Closure {
    unsafe {
        intrinsics::transmute((0x6000 + util::size_of::<EnvData>()) as u64)
    }
}
