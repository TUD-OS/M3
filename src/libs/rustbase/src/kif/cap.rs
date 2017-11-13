use core::fmt;

pub type CapSel = u32;

#[derive(Copy, Clone, Default)]
pub struct CapRngDesc {
    value: u64,
}

int_enum! {
    pub struct CapType : u64 {
        const OBJECT        = 0x0;
        const MAPPING       = 0x1;
    }
}

impl fmt::Display for CapRngDesc {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "CRD[{}: {}:{}]", self.cap_type(), self.start(), self.count())
    }
}

impl CapRngDesc {
    pub fn new(ty: CapType, start: CapSel, count: CapSel) -> CapRngDesc {
        CapRngDesc {
            value: ty.val | ((start as u64) << 33) | ((count as u64) << 1)
        }
    }

    pub fn new_from(val: u64) -> CapRngDesc {
        CapRngDesc {
            value: val
        }
    }

    pub fn value(&self) -> u64 {
        self.value
    }
    pub fn cap_type(&self) -> CapType {
        CapType::from(self.value & 0x1)
    }
    pub fn start(&self) -> CapSel {
        (self.value >> 33) as CapSel
    }
    pub fn count(&self) -> CapSel {
        ((self.value >> 1) & 0xFFFFFFFF) as CapSel
    }
}
