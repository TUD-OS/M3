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

use col::String;
use core::iter;
use errors::Error;
use io::{read_object, Read};
use util;
use vfs::{BufReader, FileRef, INodeId, OpenFlags, Seek, SeekMode, VFS};

#[derive(Debug)]
pub struct DirEntry {
    inode: INodeId,
    name: String,
}

impl DirEntry {
    pub fn new(inode: INodeId, name: String) -> Self {
        DirEntry {
            inode: inode,
            name: name,
        }
    }

    pub fn inode(&self) -> INodeId {
        self.inode
    }

    pub fn file_name(&self) -> &str {
        &self.name
    }
}

pub struct ReadDir {
    reader: BufReader<FileRef>,
}

impl iter::Iterator for ReadDir {
    type Item = DirEntry;

    fn next(&mut self) -> Option<Self::Item> {
        #[repr(C, packed)]
        struct M3FSDirEntry {
            inode: INodeId,
            name_len: u32,
            next: u32,
        }

        // read header
        let entry: M3FSDirEntry = match read_object(&mut self.reader) {
            Ok(obj) => obj,
            Err(_)  => return None,
        };

        // read name
        let res = DirEntry::new(
            entry.inode,
            match self.reader.read_string(entry.name_len as usize) {
                Ok(s)   => s,
                Err(_)  => return None,
            },
        );

        // move to next entry
        let off = entry.next as usize - (util::size_of::<M3FSDirEntry>() + entry.name_len as usize);
        if off != 0 {
            if self.reader.seek(off, SeekMode::CUR).is_err() {
                return None
            }
        }

        Some(res)
    }
}

pub fn read_dir(path: &str) -> Result<ReadDir, Error> {
    let dir = VFS::open(path, OpenFlags::R)?;
    Ok(ReadDir {
        reader: BufReader::new(dir),
    })
}
