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
use cell::RefCell;
use col::Vec;
use core::{fmt, mem};
use com::{VecSink, SliceSource};
use dtu::EpId;
use errors::{Code, Error};
use io::Serial;
use rc::Rc;
use serialize::Sink;
use vfs::{File, FileRef, GenericFile};
use vpe::VPE;

pub type Fd = usize;

const MAX_EPS: usize        = 4;
pub const MAX_FILES: usize  = 32;

pub type FileHandle = Rc<RefCell<File>>;

struct FileEP {
    fd: Fd,
    ep: EpId,
}

#[derive(Default)]
pub struct FileTable {
    file_ep_victim: usize,
    file_ep_count: usize,
    file_eps: [Option<FileEP>; MAX_EPS],
    files: [Option<FileHandle>; MAX_FILES],
}

impl FileTable {
    pub fn add(&mut self, file: FileHandle) -> Result<FileRef, Error> {
        self.alloc(file.clone()).map(|fd| FileRef::new(file, fd))
    }

    pub fn alloc(&mut self, file: FileHandle) -> Result<Fd, Error> {
        for fd in 0..MAX_FILES {
            if self.files[fd].is_none() {
                self.set(fd, file);
                return Ok(fd);
            }
        }
        Err(Error::new(Code::NoSpace))
    }

    pub fn get(&self, fd: Fd) -> Option<FileHandle> {
        match self.files[fd] {
            Some(ref f) => Some(f.clone()),
            None        => None,
        }
    }

    pub fn set(&mut self, fd: Fd, file: FileHandle) {
        file.borrow_mut().set_fd(fd);
        self.files[fd] = Some(file);
    }

    pub fn remove(&mut self, fd: Fd) {
        let find_file_ep = |files: &[Option<FileEP>], fd| -> Option<usize> {
            for i in 0..MAX_EPS {
                if let Some(ref fep) = files[i] {
                    if fep.fd == fd {
                        return Some(i);
                    }
                }
            }
            None
        };

        if let Some(ref mut f) = mem::replace(&mut self.files[fd], None) {
            f.borrow_mut().close();

            // remove from multiplexing table
            if let Some(idx) = find_file_ep(&self.file_eps, fd) {
                log!(FILES, "FileEPs[{}] = --", idx);
                self.file_eps[idx] = None;
                self.file_ep_count -= 1;
            }
        }
    }

    pub(crate) fn request_ep(&mut self, fd: Fd) -> Result<EpId, Error> {
        if self.file_ep_count < MAX_EPS {
            if let Ok(ep) = VPE::cur().alloc_ep() {
                for i in 0..MAX_EPS {
                    if self.file_eps[i].is_none() {
                        log!(
                            FILES,
                            "FileEPs[{}] = EP:{},FD:{}", i, ep, fd
                        );

                        self.file_eps[i] = Some(FileEP {
                            fd: fd,
                            ep: ep,
                        });
                        self.file_ep_count += 1;
                        return Ok(ep);
                    }
                }
            }
        }

        // TODO be smarter here
        let mut i = self.file_ep_victim;
        for _ in 0..MAX_EPS {
            if let Some(ref mut fep) = self.file_eps[i] {
                log!(
                    FILES,
                    "FileEPs[{}] = EP:{},FD: switching from {} to {}",
                    i, fep.ep, fep.fd, fd
                );

                let file = self.files[fep.fd].as_ref().unwrap();
                file.borrow_mut().evict();
                fep.fd = fd;
                self.file_ep_victim = (i + 1) % MAX_EPS;
                return Ok(fep.ep);
            }

            i = (i + 1) % MAX_EPS;
        }

        Err(Error::new(Code::NoSpace))
    }

    pub fn collect_caps(&self, vpe: Selector,
                               dels: &mut Vec<Selector>,
                               max_sel: &mut Selector) -> Result<(), Error> {
        for fd in 0..MAX_FILES {
            if let Some(ref f) = self.files[fd] {
                f.borrow().exchange_caps(vpe, dels, max_sel)?;
            }
        }
        Ok(())
    }

    pub fn serialize(&self, s: &mut VecSink) {
        let count = self.files.iter().filter(|&f| f.is_some()).count();
        s.push(&count);

        for fd in 0..MAX_FILES {
            if let Some(ref f) = self.files[fd] {
                let file = f.borrow();
                s.push(&fd);
                s.push(&file.file_type());
                file.serialize(s);
            }
        }
    }

    pub fn unserialize(s: &mut SliceSource) -> FileTable {
        let mut ft = FileTable::default();

        let count = s.pop();
        for _ in 0..count {
            let fd: Fd = s.pop();
            let file_type: u8 = s.pop();
            ft.set(fd, match file_type {
                b'F' => GenericFile::unserialize(s),
                b'S' => Serial::new(),
                _    => panic!("Unexpected file type {}", file_type),
            });
        }

        ft
    }
}

impl fmt::Debug for FileTable {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "FileTable[\n")?;
        for fd in 0..MAX_FILES {
            if let Some(ref file) = self.files[fd] {
                write!(f, "  {} -> {:?}\n", fd, file)?;
            }
        }
        write!(f, "]")
    }
}

pub fn deinit() {
    let ft = VPE::cur().files();
    for fd in 0..MAX_FILES {
        ft.remove(fd);
    }
}
