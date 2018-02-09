use arch::dtu::PEId;
use core::fmt;
use core::ops;

/// Represents a global address, which is a combination of a PE id and an offset within the PE.
///
/// If the PE supports virtual memory, the offset is a virtual address. Otherwise, it is a physical
/// address (the offset in the PE-internal memory).
#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd)]
#[repr(packed)]
pub struct GlobAddr {
    val: u64,
}

#[cfg(target_os = "none")]
const PE_SHIFT: u32 = 56;
#[cfg(target_os = "linux")]
const PE_SHIFT: u32 = 48;

impl GlobAddr {
    /// Creates a new global address from the given raw value
    pub fn new(addr: u64) -> GlobAddr {
        GlobAddr {
            val: addr
        }
    }
    /// Creates a new global address from the given PE id and offset
    pub fn new_with(pe: PEId, off: usize) -> GlobAddr {
        Self::new(((0x80 + pe as u64) << PE_SHIFT) | off as u64)
    }

    /// Returns the raw value
    pub fn raw(&self) -> u64 {
        self.val
    }
    /// Returns the PE id
    pub fn pe(&self) -> PEId {
        ((self.val >> PE_SHIFT) - 0x80) as PEId
    }
    /// Returns the offset
    pub fn offset(&self) -> usize {
        (self.val & ((1 << PE_SHIFT) - 1)) as usize
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
