/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
 *
 * M3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * M3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

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

/// Program header for 32-bit ELF files
#[derive(Default)]
#[repr(C, packed)]
pub struct Phdr32 {
    pub ty: u32,
    pub offset: u32,
    pub vaddr: usize,
    pub paddr: usize,
    pub filesz: u32,
    pub memsz: u32,
    pub flags: u32,
    pub align: u32,
}

/// Program header for 64-bit ELF files
#[derive(Default)]
#[repr(C, packed)]
pub struct Phdr64 {
    pub ty: u32,
    pub flags: u32,
    pub offset: u64,
    pub vaddr: usize,
    pub paddr: usize,
    pub filesz: u64,
    pub memsz: u64,
    pub align: u64,
}

#[cfg(target_pointer_width = "64")]
pub type Phdr = Phdr64;
#[cfg(target_pointer_width = "32")]
pub type Phdr = Phdr32;

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
