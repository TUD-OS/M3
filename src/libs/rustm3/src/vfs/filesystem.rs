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
use core::any::Any;
use core::fmt;
use errors::Error;
use vfs::{OpenFlags, FileHandle, FileInfo, FileMode};

pub trait FileSystem : fmt::Debug {
    fn as_any(&self) -> &Any;

    fn open(&self, path: &str, perms: OpenFlags) -> Result<FileHandle, Error>;

    fn stat(&self, path: &str) -> Result<FileInfo, Error>;

    fn mkdir(&self, path: &str, mode: FileMode) -> Result<(), Error>;
    fn rmdir(&self, path: &str) -> Result<(), Error>;

    fn link(&self, old_path: &str, new_path: &str) -> Result<(), Error>;
    fn unlink(&self, path: &str) -> Result<(), Error>;

    fn fs_type(&self) -> u8;
    fn exchange_caps(&self, vpe: Selector,
                            dels: &mut Vec<Selector>,
                            max_sel: &mut Selector) -> Result<(), Error>;
    fn serialize(&self, s: &mut VecSink);
}
