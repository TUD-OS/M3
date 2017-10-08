use core::intrinsics;
use core::fmt;

pub type CapSel = u32;

#[derive(Copy, Clone)]
pub struct CapRngDesc {
    value: u64,
}

#[derive(Debug)]
pub enum Type {
    Object,
    Mapping,
}

impl From<u64> for Type {
    fn from(val: u64) -> Self {
        unsafe { intrinsics::transmute(val as u8) }
    }
}

impl fmt::Display for CapRngDesc {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "CRD[{:?}: {}:{}]", self.cap_type(), self.start(), self.count())
    }
}

impl CapRngDesc {
    pub fn new() -> CapRngDesc {
        Self::new_from(Type::Object, 0, 0)
    }

    pub fn new_from(ty: Type, start: CapSel, count: CapSel) -> CapRngDesc {
        CapRngDesc {
            value: (ty as u64) | ((start as u64) << 33) | ((count as u64) << 1)
        }
    }

    pub fn new_from_val(val: u64) -> CapRngDesc {
        CapRngDesc {
            value: val
        }
    }

    pub fn value(&self) -> u64 {
        self.value
    }
    pub fn cap_type(&self) -> Type {
        Type::from(self.value & 0x1)
    }
    pub fn start(&self) -> CapSel {
        (self.value >> 33) as CapSel
    }
    pub fn count(&self) -> CapSel {
        ((self.value >> 1) & 0xFFFFFFFF) as CapSel
    }
}
