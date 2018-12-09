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
use core::fmt;
use com::{MemGate, RecvGate, SendGate, SliceSource, VecSink};
use serialize::Sink;
use errors::Error;
use goff;
use io::{Read, Write};
use kif::{CapRngDesc, CapType, INVALID_SEL, Perm, syscalls};
use rc::Rc;
use session::{Pager, ClientSession};
use time;
use util;
use vfs;
use vpe::VPE;

int_enum! {
    pub struct Operation : u64 {
        const STAT      = 0;
        const SEEK      = 1;
        const NEXT_IN   = 2;
        const NEXT_OUT  = 3;
        const COMMIT    = 4;
    }
}

pub struct GenericFile {
    fd: vfs::Fd,
    sess: ClientSession,
    sgate: SendGate,
    mgate: MemGate,
    goff: usize,
    off: usize,
    pos: usize,
    len: usize,
    writing: bool,
}

impl GenericFile {
    pub fn new(sel: Selector) -> Self {
        GenericFile {
            fd: 0,
            sess: ClientSession::new_bind(sel),
            sgate: SendGate::new_bind(sel + 1),
            mgate: MemGate::new_bind(INVALID_SEL),
            goff: 0,
            off: 0,
            pos: 0,
            len: 0,
            writing: false,
        }
    }

    pub fn unserialize(s: &mut SliceSource) -> vfs::FileHandle {
        let _ : i32 = s.pop(); // flags
        Rc::new(RefCell::new(GenericFile::new(s.pop())))
    }

    fn submit(&mut self, force: bool) -> Result<(), Error> {
        if self.pos > 0 && (self.writing || force) {
            let mut reply = send_recv_res!(
                &self.sgate, RecvGate::def(),
                Operation::COMMIT, self.pos
            )?;
            // if we append, the file was truncated
            let filesize = reply.pop();
            if self.goff + self.len > filesize {
                self.len = filesize - self.goff;
            }
            self.goff += self.pos;
            self.pos = 0;
            self.len = 0;
            self.writing = false;
        }
        Ok(())
    }

    fn delegate_ep(&mut self) -> Result<(), Error> {
        if self.mgate.ep().is_none() {
            let ep = VPE::cur().files().request_ep(self.fd)?;
            self.sess.delegate_obj(VPE::cur().ep_sel(ep))?;
            self.mgate.set_ep(ep);
        }
        Ok(())
    }
}

impl vfs::File for GenericFile {
    fn fd(&self) -> vfs::Fd {
        self.fd
    }
    fn set_fd(&mut self, fd: vfs::Fd) {
        self.fd = fd;
    }

    fn evict(&mut self) {
        // submit read/written data
        self.submit(true).ok();

        // revoke EP cap
        let ep = self.mgate.ep().unwrap();
        let sel = VPE::cur().ep_sel(ep);
        VPE::cur().revoke(CapRngDesc::new(CapType::OBJECT, sel, 1), true).ok();
        self.mgate.unset_ep();
    }

    fn close(&mut self) {
        self.submit(false).ok();

        if let Some(ep) = self.mgate.ep() {
            let sel = VPE::cur().ep_sel(ep);
            VPE::cur().revoke(CapRngDesc::new(CapType::OBJECT, sel, 1), true).ok();
            VPE::cur().free_ep(ep);
        }

        // do that manually here, because the File object is not destructed if we've received this
        // file from our parent.
        VPE::cur().revoke(CapRngDesc::new(CapType::OBJECT, self.sess.sel(), 1), false).ok();
    }

    fn stat(&self) -> Result<vfs::FileInfo, Error> {
        let mut reply = send_recv_res!(
            &self.sgate, RecvGate::def(),
            Operation::STAT
        )?;
        Ok(reply.pop())
    }

    fn file_type(&self) -> u8 {
        b'F'
    }

    fn exchange_caps(&self, vpe: Selector,
                            _dels: &mut Vec<Selector>,
                            max_sel: &mut Selector) -> Result<(), Error> {
        let crd = CapRngDesc::new(CapType::OBJECT, self.sess.sel(), 2);
        let mut args = syscalls::ExchangeArgs::default();
        self.sess.obtain_for(vpe, crd, &mut args)?;
        *max_sel = util::max(*max_sel, self.sess.sel() + 2);
        Ok(())
    }

    fn serialize(&self, s: &mut VecSink) {
        s.push(&0); // flags
        s.push(&self.sess.sel());
        s.push(&0); // id
    }
}

impl vfs::Seek for GenericFile {
    fn seek(&mut self, mut off: usize, mut whence: vfs::SeekMode) -> Result<usize, Error> {
        self.submit(false)?;

        if whence == vfs::SeekMode::CUR {
            off = self.goff + self.pos + off;
            whence = vfs::SeekMode::SET;
        }

        if whence != vfs::SeekMode::END && self.pos < self.len {
            if off > self.goff && off < self.goff + self.len {
                self.pos = off - self.goff;
                return Ok(off)
            }
        }

        let mut reply = send_recv_res!(
            &self.sgate, RecvGate::def(),
            Operation::SEEK, off, whence
        )?;
        self.goff = reply.pop();
        let off: usize = reply.pop();
        self.pos = 0;
        self.len = 0;
        Ok(self.goff + off)
    }
}

impl Read for GenericFile {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize, Error> {
        self.delegate_ep()?;
        self.submit(false)?;

        if self.pos == self.len {
            time::start(0xbbbb);
            let mut reply = send_recv_res!(
                &self.sgate, RecvGate::def(),
                Operation::NEXT_IN
            )?;
            time::stop(0xbbbb);
            self.goff += self.len;
            self.off = reply.pop();
            self.len = reply.pop();
            self.pos = 0;
        }

        let amount = util::min(buf.len(), self.len - self.pos);
        if amount > 0 {
            time::start(0xaaaa);
            self.mgate.read(&mut buf[0..amount], (self.off + self.pos) as goff)?;
            time::stop(0xaaaa);
            self.pos += amount;
        }
        self.writing = false;
        Ok(amount)
    }
}

impl Write for GenericFile {
    fn flush(&mut self) -> Result<(), Error> {
        self.submit(false)
    }

    fn write(&mut self, buf: &[u8]) -> Result<usize, Error> {
        self.delegate_ep()?;

        if self.pos == self.len {
            time::start(0xbbbb);
            let mut reply = send_recv_res!(
                &self.sgate, RecvGate::def(),
                Operation::NEXT_OUT
            )?;
            time::stop(0xbbbb);
            self.goff += self.len;
            self.off = reply.pop();
            self.len = reply.pop();
            self.pos = 0;
        }

        let amount = util::min(buf.len(), self.len - self.pos);
        if amount > 0 {
            time::start(0xaaaa);
            self.mgate.write(&buf[0..amount], (self.off + self.pos) as goff)?;
            time::stop(0xaaaa);
            self.pos += amount;
        }
        self.writing = true;
        Ok(amount)
    }
}

impl vfs::Map for GenericFile {
    fn map(&self, pager: &Pager, virt: goff, off: usize, len: usize, prot: Perm) -> Result<(), Error> {
        // TODO maybe check here whether self is a pipe and return an error?
        pager.map_ds(virt, len, off, prot, &self.sess).map(|_| ())
    }
}

impl fmt::Debug for GenericFile {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "GenFile[sess={}, goff={:#x}, off={:#x}, pos={:#x}, len={:#x}]",
            self.sess.sel(), self.goff, self.off, self.pos, self.len)
    }
}
