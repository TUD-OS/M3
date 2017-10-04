use core::intrinsics;

#[derive(PartialEq)]
pub enum PEType {
    // Compute PE with internal memory
    CompIMem    = 0,
    // Compute PE with cache and external memory
    CompEMem    = 1,
    // memory PE
    Mem         = 2,
}

#[derive(PartialEq)]
pub enum PEISA {
    None        = 0,
    X86         = 1,
    ARM         = 2,
    Xtensa      = 3,
    AccelSHA    = 4,
    AccelFFT    = 5,
    AccelToUp   = 6,
}

bitflags! {
    pub struct PEFlags : ValueType {
        const MMU_VM    = 1;
        const DTU_VM    = 2;
    }
}

pub type ValueType = u32;

// TODO better way?
impl From<ValueType> for PEType {
    fn from(ty: ValueType) -> Self {
        unsafe { intrinsics::transmute(ty as u8) }
    }
}

impl From<ValueType> for PEISA {
    fn from(ty: ValueType) -> Self {
        unsafe { intrinsics::transmute(ty as u8) }
    }
}

#[repr(C, packed)]
pub struct PEDesc {
    val: ValueType,
}

impl PEDesc {
    pub fn new() -> PEDesc {
        Self::new_from_val(0)
    }

    pub fn new_from_val(val: ValueType) -> PEDesc {
        PEDesc {
            val: val
        }
    }

    pub fn new_from(ty: PEType, isa: PEISA, memsize: usize) -> PEDesc {
        let val = (ty as ValueType) | ((isa as ValueType) << 3) | memsize as ValueType;
        Self::new_from_val(val)
    }

    pub fn value(&self) -> ValueType {
        self.val
    }

    pub fn pe_type(&self) -> PEType {
        PEType::from(self.val & 0x7)
    }

    pub fn isa(&self) -> PEISA {
        PEISA::from((self.val >> 3) & 0x7)
    }

    pub fn flags(&self) -> PEFlags {
        PEFlags::from_bits((self.val >> 6) & 0x3).unwrap()
    }

    pub fn mem_size(&self) -> usize {
        (self.val & !0xFFF) as usize
    }

    pub fn is_programmable(&self) -> bool {
        match self.isa() {
            PEISA::X86 | PEISA::ARM | PEISA::Xtensa  => true,
            _                                        => false,
        }
    }
    pub fn is_ffaccel(&self) -> bool {
        match self.isa() {
            PEISA::AccelSHA | PEISA::AccelFFT | PEISA::AccelToUp => true,
            _                                                    => false
        }
    }

    pub fn supports_multictx(&self) -> bool {
        self.has_cache() || self.is_ffaccel()
    }
    pub fn supports_ctxsw(&self) -> bool {
        self.pe_type() != PEType::Mem
    }

    pub fn has_mem(&self) -> bool {
        self.pe_type() == PEType::CompIMem || self.pe_type() == PEType::Mem
    }
    pub fn has_cache(&self) -> bool {
        self.pe_type() == PEType::CompEMem
    }
    pub fn has_virtmem(&self) -> bool {
        self.has_dtuvm() || self.has_mmu()
    }
    pub fn has_dtuvm(&self) -> bool {
        !(self.flags() & PEFlags::DTU_VM).is_empty()
    }
    pub fn has_mmu(&self) -> bool {
        !(self.flags() & PEFlags::MMU_VM).is_empty()
    }
}
