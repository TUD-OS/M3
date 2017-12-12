use arch;
use base::cell::StaticCell;
use base::dtu::PEId;
use base::GlobAddr;
use base::kif::PEDesc;
use core::iter;

pub const MAX_MODS: usize   = 64;
pub const MAX_PES: usize    = 64;

#[repr(C, packed)]
#[derive(Copy, Clone)]
pub struct KEnv {
    pub mods: [u64; MAX_MODS],
    pub pe_count: u64,
    pub pes: [u32; MAX_PES],
}

pub struct PEIterator {
    id: PEId,
    last: PEId,
}

impl iter::Iterator for PEIterator {
    type Item = PEId;

    fn next(&mut self) -> Option<Self::Item> {
        if self.id <= self.last {
            self.id += 1;
            Some(self.id - 1)
        }
        else {
            None
        }
    }
}

static KENV: StaticCell<Option<KEnv>> = StaticCell::new(None);

pub fn init() {
    KENV.set(Some(arch::platform::init()))
}

fn get() -> &'static mut KEnv {
    KENV.get_mut().as_mut().unwrap()
}

pub struct ModIterator {
    idx: usize,
}

impl iter::Iterator for ModIterator {
    type Item = GlobAddr;

    fn next(&mut self) -> Option<Self::Item> {
        self.idx += 1;
        match {get().mods}[self.idx - 1] {
            0 => None,
            a => Some(GlobAddr::new(a)),
        }
    }
}

pub fn pe_count() -> usize {
    get().pe_count as usize
}
pub fn pes() -> PEIterator {
    PEIterator {
        id: 0,
        last: pe_count() - 1,
    }
}
pub fn kernel_pe() -> PEId {
    arch::platform::kernel_pe()
}
pub fn user_pes() -> PEIterator {
    PEIterator {
        id: arch::platform::first_user_pe(),
        last: arch::platform::last_user_pe(),
    }
}

pub fn pe_desc(pe: PEId) -> PEDesc {
    PEDesc::new_from(get().pes[pe])
}

pub fn default_rcvbuf(pe: PEId) -> usize {
    arch::platform::default_rcvbuf(pe)
}
pub fn rcvbufs_size(pe: PEId) -> usize {
    arch::platform::rcvbufs_size(pe)
}

pub fn mods() -> ModIterator {
    ModIterator {
        idx: 0,
    }
}
