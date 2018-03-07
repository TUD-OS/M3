use cap::Selector;
use cell::RefCell;
use col::Vec;
use core::{fmt, intrinsics};
use com::{MemGate, RecvGate, SendGate, SliceSource, VecSink};
use serialize::Sink;
use errors::Error;
use io::{Read, Write};
use kif::{CapRngDesc, CapType, INVALID_SEL, Perm, syscalls};
use rc::Rc;
use session::{Pager, Session};
use time;
use util;
use vfs;
use vpe::VPE;

int_enum! {
    pub struct Operation : u64 {
        const STAT    = 0;
        const SEEK    = 1;
        const READ    = 2;
        const WRITE   = 3;
    }
}

pub struct GenericFile {
    sess: Session,
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
            sess: Session::new_bind(sel),
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
        let sess = s.pop();
        Rc::new(RefCell::new(GenericFile {
            sess: Session::new_bind(sess),
            sgate: SendGate::new_bind(sess + 1),
            mgate: MemGate::new_bind(INVALID_SEL),
            goff: s.pop(),
            off: s.pop(),
            pos: s.pop(),
            len: s.pop(),
            writing: s.pop(),
        }))
    }

    fn submit(&mut self) -> Result<(), Error> {
        if self.writing && self.pos > 0 {
            let mut reply = send_recv_res!(
                &self.sgate, RecvGate::def(),
                Operation::WRITE, self.off + self.pos
            )?;
            // if we append, the file was truncated
            let filesize = reply.pop();
            if self.goff + self.len > filesize {
                self.len = filesize - self.goff;
            }
        }
        Ok(())
    }

    fn delegate_ep(&mut self) -> Result<(), Error> {
        if self.mgate.ep().is_none() {
            let ep = VPE::cur().alloc_ep()?;
            self.sess.delegate_obj(VPE::cur().ep_sel(ep))?;
            self.mgate.set_ep(ep);
        }
        Ok(())
    }
}

impl vfs::File for GenericFile {
    fn close(&mut self) {
        self.submit().ok();

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

    fn exchange_caps(&self, vpe: Selector, _dels: &mut Vec<Selector>, max_sel: &mut Selector) {
        let mut args = syscalls::ExchangeArgs {
            count: 0,
            vals: unsafe { intrinsics::uninit() },
        };

        // TODO error case?
        let crd = CapRngDesc::new(CapType::OBJECT, self.sess.sel(), 2);
        self.sess.obtain_for(vpe, crd, &mut args).ok();
        *max_sel = util::max(*max_sel, self.sess.sel() + 1);
    }

    fn serialize(&self, s: &mut VecSink) {
        s.push(&self.sess.sel());
        s.push(&self.goff);
        s.push(&self.off);
        s.push(&self.pos);
        s.push(&self.len);
        s.push(&self.writing);
    }
}

impl vfs::Seek for GenericFile {
    fn seek(&mut self, mut off: usize, mut whence: vfs::SeekMode) -> Result<usize, Error> {
        self.submit()?;

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
        self.submit()?;

        if self.pos == self.len {
            time::start(0xbbbb);
            let mut reply = send_recv_res!(
                &self.sgate, RecvGate::def(),
                Operation::READ
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
            self.mgate.read(&mut buf[0..amount], self.off + self.pos)?;
            time::stop(0xaaaa);
            self.pos += amount;
        }
        self.writing = false;
        Ok(amount)
    }
}

impl Write for GenericFile {
    fn flush(&mut self) -> Result<(), Error> {
        Ok(())
    }

    fn write(&mut self, buf: &[u8]) -> Result<usize, Error> {
        self.delegate_ep()?;

        if self.pos == self.len {
            time::start(0xbbbb);
            let mut reply = send_recv_res!(
                &self.sgate, RecvGate::def(),
                Operation::WRITE, 0
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
            self.mgate.write(&buf[0..amount], self.off + self.pos)?;
            time::stop(0xaaaa);
            self.pos += amount;
        }
        self.writing = true;
        Ok(amount)
    }
}

impl vfs::Map for GenericFile {
    fn map(&self, pager: &Pager, virt: usize, off: usize, len: usize, prot: Perm) -> Result<(), Error> {
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
