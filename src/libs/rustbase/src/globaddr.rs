use arch::dtu::PEId;
use core::fmt;
use core::ops;

#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd)]
#[repr(packed)]
pub struct GlobAddr {
    val: u64,
}

impl GlobAddr {
    pub fn new(addr: u64) -> GlobAddr {
        GlobAddr {
            val: addr
        }
    }
    pub fn new_with(pe: PEId, off: usize) -> GlobAddr {
        Self::new(((0x80 + pe as u64) << 44) | off as u64)
    }

    pub fn raw(&self) -> u64 {
        self.val
    }
    pub fn pe(&self) -> PEId {
        ((self.val >> 44) - 0x80) as PEId
    }
    pub fn offset(&self) -> usize {
        (self.val & ((1 << 44) - 1)) as usize
    }
}

impl fmt::Debug for GlobAddr {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "GlobAddr[pe: {}, off: {:#x}]", self.pe(), self.offset())
    }
}

impl ops::Add<usize> for GlobAddr {
    type Output = GlobAddr;

    fn add(self, rhs: usize) -> Self::Output {
        GlobAddr::new(self.val + rhs as u64)
    }
}
