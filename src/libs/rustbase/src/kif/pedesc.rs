use core::fmt;

int_enum! {
    pub struct PEType : ValueType {
        // Compute PE with internal memory
        const COMP_IMEM     = 0x0;
        // Compute PE with cache and external memory
        const COMP_EMEM     = 0x1;
        // memory PE
        const MEM           = 0x2;
    }
}

int_enum! {
    pub struct PEISA : ValueType {
        const NONE          = 0x0;
        const X86           = 0x1;
        const ARM           = 0x2;
        const XTENSA        = 0x3;
        const ACCEL_SHA     = 0x4;
        const ACCEL_FFT     = 0x5;
        const ACCEL_TOUP    = 0x6;
    }
}

bitflags! {
    pub struct PEFlags : ValueType {
        const MMU_VM        = 0b01;
        const DTU_VM        = 0b10;
    }
}

pub type ValueType = u32;

#[repr(C, packed)]
#[derive(Clone, Copy, Default)]
pub struct PEDesc {
    val: ValueType,
}

impl PEDesc {
    pub fn new(ty: PEType, isa: PEISA, memsize: usize) -> PEDesc {
        let val = ty.val | (isa.val << 3) | memsize as ValueType;
        Self::new_from(val)
    }

    pub fn new_from(val: ValueType) -> PEDesc {
        PEDesc {
            val: val
        }
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
            PEISA::X86 | PEISA::ARM | PEISA::XTENSA  => true,
            _                                        => false,
        }
    }
    pub fn is_ffaccel(&self) -> bool {
        match self.isa() {
            PEISA::ACCEL_SHA | PEISA::ACCEL_FFT | PEISA::ACCEL_TOUP => true,
            _                                                       => false
        }
    }

    pub fn supports_multictx(&self) -> bool {
        self.has_cache() || self.is_ffaccel()
    }
    pub fn supports_ctxsw(&self) -> bool {
        self.pe_type() != PEType::MEM
    }

    pub fn has_mem(&self) -> bool {
        self.pe_type() == PEType::COMP_IMEM || self.pe_type() == PEType::MEM
    }
    pub fn has_cache(&self) -> bool {
        self.pe_type() == PEType::COMP_EMEM
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

impl fmt::Debug for PEDesc {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "PEDesc[type={}, isa={}, memsz={}]", self.pe_type(), self.isa(), self.mem_size())
    }
}
