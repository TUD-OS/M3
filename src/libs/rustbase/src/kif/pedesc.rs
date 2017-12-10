use core::fmt;

int_enum! {
    /// The different types of PEs
    pub struct PEType : PEDescRaw {
        /// Compute PE with internal memory
        const COMP_IMEM     = 0x0;
        /// Compute PE with cache and external memory
        const COMP_EMEM     = 0x1;
        /// Memory PE
        const MEM           = 0x2;
    }
}

int_enum! {
    /// The supported instruction set architectures (ISAs)
    pub struct PEISA : PEDescRaw {
        /// Dummy ISA to represent memory PEs
        const NONE          = 0x0;
        /// x86_64 as supported by gem5
        const X86           = 0x1;
        /// ARMv7 as supported by gem5
        const ARM           = 0x2;
        /// Xtensa as on Tomahawk 2/4
        const XTENSA        = 0x3;
        /// Dummy ISA to represent the SHA fixed-function accelerator
        const ACCEL_SHA     = 0x4;
        /// Dummy ISA to represent the FFT fixed-function accelerator
        const ACCEL_FFT     = 0x5;
        /// Dummy ISA to represent the toupper fixed-function accelerator
        const ACCEL_TOUP    = 0x6;
    }
}

bitflags! {
    /// Special flags for PE features
    pub struct PEFlags : PEDescRaw {
        /// This flag is set if the MMU of the CU should be used
        const MMU_VM        = 0b01;
        /// This flag is set if the DTU's virtual memory support should be used
        const DTU_VM        = 0b10;
    }
}

type PEDescRaw = u32;

/// Describes a processing element (PE).
///
/// This struct is used for the [`create_vpe`] syscall to let the kernel know about the desired PE
/// type. Additionally, it is used to tell a VPE about the attributes of the PE it has been assigned
/// to.
///
/// [`create_vpe`]: ../../m3/syscalls/fn.create_vpe.html
#[repr(C, packed)]
#[derive(Clone, Copy, Default)]
pub struct PEDesc {
    val: PEDescRaw,
}

impl PEDesc {
    /// Creates a new PE description from the given type, ISA, and memory size.
    pub fn new(ty: PEType, isa: PEISA, memsize: usize) -> PEDesc {
        let val = ty.val | (isa.val << 3) | memsize as PEDescRaw;
        Self::new_from(val)
    }

    /// Creates a new PE description from the given raw value
    pub fn new_from(val: PEDescRaw) -> PEDesc {
        PEDesc {
            val: val
        }
    }

    /// Returns the raw value
    pub fn value(&self) -> PEDescRaw {
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

    /// Returns the size of the internal memory (0 if none is present)
    pub fn mem_size(&self) -> usize {
        (self.val & !0xFFF) as usize
    }

    /// Returns whether the PE executes software
    pub fn is_programmable(&self) -> bool {
        match self.isa() {
            PEISA::X86 | PEISA::ARM | PEISA::XTENSA  => true,
            _                                        => false,
        }
    }
    /// Returns whether the PE contains a fixed-function accelerator
    pub fn is_ffaccel(&self) -> bool {
        match self.isa() {
            PEISA::ACCEL_SHA | PEISA::ACCEL_FFT | PEISA::ACCEL_TOUP => true,
            _                                                       => false
        }
    }

    /// Returns true if the PE supports multiple contexts
    pub fn supports_multictx(&self) -> bool {
        self.has_cache() || self.is_ffaccel()
    }
    /// Returns true if the PE supports the context switching protocol
    pub fn supports_ctxsw(&self) -> bool {
        self.pe_type() != PEType::MEM
    }

    /// Returns whether the PE has an internal memory (SPM, DRAM, ...)
    pub fn has_mem(&self) -> bool {
        self.pe_type() == PEType::COMP_IMEM || self.pe_type() == PEType::MEM
    }
    /// Returns whether the PE has a cache
    pub fn has_cache(&self) -> bool {
        self.pe_type() == PEType::COMP_EMEM
    }
    /// Returns whether the PE supports virtual memory (either by DTU or MMU)
    pub fn has_virtmem(&self) -> bool {
        self.has_dtuvm() || self.has_mmu()
    }
    /// Returns whether the PE uses DTU-based virtual memory
    pub fn has_dtuvm(&self) -> bool {
        self.flags().contains(PEFlags::DTU_VM)
    }
    /// Returns whether the PE uses MMU-based virtual memory
    pub fn has_mmu(&self) -> bool {
        self.flags().contains(PEFlags::MMU_VM)
    }
}

impl fmt::Debug for PEDesc {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "PEDesc[type={}, isa={}, memsz={}]", self.pe_type(), self.isa(), self.mem_size())
    }
}
