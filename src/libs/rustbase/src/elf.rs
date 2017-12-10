//! Contains the basics of the ELF interface

use kif;

const EI_NIDENT: usize = 16;

int_enum! {
    /// The program header entry types
    pub struct PT : u32 {
        /// Load segment
        const LOAD = 0x1;
    }
}

bitflags! {
    /// The program header flags
    pub struct PF : u32 {
        /// Executable
        const X = 0x1;
        /// Writable
        const W = 0x2;
        /// Readable
        const R = 0x4;
    }
}

/// ELF header
#[derive(Default)]
#[repr(C, packed)]
pub struct Ehdr {
    pub ident: [u8; EI_NIDENT],
    pub ty: u16,
    pub machine: u16,
    pub version: u32,
    pub entry: usize,
    pub phoff: usize,
    pub shoff: usize,
    pub flags: u32,
    pub ehsize: u16,
    pub phentsize: u16,
    pub phnum: u16,
    pub shentsize: u16,
    pub shnum: u16,
    pub shstrndx: u16,
}

/// Program header
#[derive(Default)]
#[repr(C, packed)]
pub struct Phdr {
    pub ty: u32,
    pub flags: u32,
    pub offset: usize,
    pub vaddr: usize,
    pub paddr: usize,
    pub filesz: usize,
    pub memsz: usize,
    pub align: usize,
}

impl From<PF> for kif::Perm {
    fn from(flags: PF) -> Self {
        let mut prot = kif::Perm::empty();
        if flags.contains(PF::R) {
            prot |= kif::Perm::R;
        }
        if flags.contains(PF::W) {
            prot |= kif::Perm::W;
        }
        if flags.contains(PF::X) {
            prot |= kif::Perm::X;
        }
        prot
    }
}
