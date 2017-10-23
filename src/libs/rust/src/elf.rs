const EI_NIDENT: usize = 16;

int_enum! {
    pub struct PT : u32 {
        const LOAD = 0x1;
    }
}

bitflags! {
    pub struct PF : u32 {
        const X = 0x1;
        const W = 0x2;
        const R = 0x4;
    }
}

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
