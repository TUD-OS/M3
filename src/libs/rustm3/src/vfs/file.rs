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

use cap::Selector;
use col::Vec;
use com::VecSink;
use core::fmt::Debug;
use errors::Error;
use goff;
use io::{Read, Write};
use kif;
use serialize::{Marshallable, Unmarshallable, Sink, Source};
use session::Pager;
use vfs::{BlockId, DevId, Fd, INodeId, FileMode};

int_enum! {
    pub struct SeekMode : u32 {
        const SET       = 0x0;
        const CUR       = 0x1;
        const END       = 0x2;
    }
}

bitflags! {
    pub struct OpenFlags : u32 {
        const R         = 0b000001;
        const W         = 0b000010;
        const X         = 0b000100;
        const TRUNC     = 0b001000;
        const APPEND    = 0b010000;
        const CREATE    = 0b100000;

        const RW        = Self::R.bits | Self::W.bits;
        const RX        = Self::R.bits | Self::X.bits;
        const RWX       = Self::R.bits | Self::W.bits | Self::X.bits;
    }
}

#[derive(Copy, Clone, Debug)]
#[repr(C, packed)]
pub struct FileInfo {
    pub devno: DevId,
    pub inode: INodeId,
    pub mode: FileMode,
    pub links: u32,
    pub size: usize,
    pub lastaccess: u32,
    pub lastmod: u32,
    // for debugging
    pub extents: u32,
    pub firstblock: BlockId,
}

impl Marshallable for FileInfo {
    fn marshall(&self, s: &mut Sink) {
        s.push(&self.devno);
        s.push(&{self.inode});
        s.push(&{self.mode});
        s.push(&{self.links});
        s.push(&{self.size});
        s.push(&{self.lastaccess});
        s.push(&{self.lastmod});
        s.push(&{self.extents});
        s.push(&{self.firstblock});
    }
}

impl Unmarshallable for FileInfo {
    fn unmarshall(s: &mut Source) -> Self {
        FileInfo {
            devno:      s.pop_word() as DevId,
            inode:      s.pop_word() as INodeId,
            mode:       s.pop_word() as FileMode,
            links:      s.pop_word() as u32,
            size:       s.pop_word() as usize,
            lastaccess: s.pop_word() as u32,
            lastmod:    s.pop_word() as u32,
            extents:    s.pop_word() as u32,
            firstblock: s.pop_word() as BlockId,
        }
    }
}

pub trait File : Read + Write + Seek + Map + Debug {
    fn fd(&self) -> Fd;
    fn set_fd(&mut self, fd: Fd);

    fn evict(&mut self);

    fn close(&mut self);

    fn stat(&self) -> Result<FileInfo, Error>;

    fn file_type(&self) -> u8;
    fn exchange_caps(&self, vpe: Selector,
                            dels: &mut Vec<Selector>,
                            max_sel: &mut Selector) -> Result<(), Error>;
    fn serialize(&self, s: &mut VecSink);
}

pub trait Seek {
    fn seek(&mut self, off: usize, whence: SeekMode) -> Result<usize, Error>;
}

pub trait Map {
    fn map(&self, pager: &Pager, virt: goff,
           off: usize, len: usize, prot: kif::Perm) -> Result<(), Error>;
}
