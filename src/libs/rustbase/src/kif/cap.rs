use core::fmt;

/// A capability selector
pub type CapSel = u32;

/// A capability range descriptor, which describes a continuous range of capabilities
#[derive(Copy, Clone, Default)]
pub struct CapRngDesc {
    value: u64,
}

int_enum! {
    /// The capability types
    pub struct CapType : u64 {
        /// Object capabilities are used for kernel objects (SendGate, VPE, ...)
        const OBJECT        = 0x0;
        /// Mapping capabilities are used for page table entries
        const MAPPING       = 0x1;
    }
}

impl CapRngDesc {
    /// Creates a new capability range descriptor. `start` is the first capability selector and
    /// `start + count - 1` is the last one.
    pub fn new(ty: CapType, start: CapSel, count: CapSel) -> CapRngDesc {
        CapRngDesc {
            value: ty.val | ((start as u64) << 33) | ((count as u64) << 1)
        }
    }

    /// Creates a new capability range descriptor from the given raw value
    pub fn new_from(val: u64) -> CapRngDesc {
        CapRngDesc {
            value: val
        }
    }

    /// Returns the raw value
    pub fn value(&self) -> u64 {
        self.value
    }
    /// Returns the capability type
    pub fn cap_type(&self) -> CapType {
        CapType::from(self.value & 0x1)
    }
    /// Returns the first capability selector
    pub fn start(&self) -> CapSel {
        (self.value >> 33) as CapSel
    }
    /// Returns the number of capability selectors
    pub fn count(&self) -> CapSel {
        ((self.value >> 1) & 0xFFFFFFFF) as CapSel
    }
}

impl fmt::Display for CapRngDesc {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "CRD[{}: {}:{}]", self.cap_type(), self.start(), self.count())
    }
}
