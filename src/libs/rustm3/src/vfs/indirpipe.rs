use cap::Selector;
use cell::RefCell;
use col::Vec;
use com::{MemGate, SliceSource, VecSink};
use core::fmt;
use errors::{Code, Error};
use io::{Read, Write};
use kif::Perm;
use rc::Rc;
use serialize::Sink;
use session::{Pager, Pipe};
use time;
use vfs::{Fd, File, FileHandle, FileInfo, OpenFlags, Map, MountTable, Seek, SeekMode};
use vpe::VPE;

pub struct IndirectPipe {
    _pipe: Rc<Pipe>,
    rd_fd: Fd,
    wr_fd: Fd,
}

impl IndirectPipe {
    pub fn new(mem: &MemGate, mem_size: usize) -> Result<Self, Error> {
        let pipe = Rc::new(Pipe::new("pipe", mem_size)?);
        Ok(IndirectPipe {
            rd_fd: VPE::cur().files().alloc(IndirectPipeReader::new(mem.sel(), &pipe))?,
            wr_fd: VPE::cur().files().alloc(IndirectPipeWriter::new(mem.sel(), &pipe))?,
            _pipe: pipe,
        })
    }

    pub fn reader_fd(&self) -> Fd {
        self.rd_fd
    }

    pub fn close_reader(&self) {
        VPE::cur().files().remove(self.rd_fd);
    }

    pub fn writer_fd(&self) -> Fd {
        self.wr_fd
    }

    pub fn close_writer(&self) {
        VPE::cur().files().remove(self.wr_fd);
    }
}

impl Drop for IndirectPipe {
    fn drop(&mut self) {
        self.close_reader();
        self.close_writer();
    }
}

struct IndirectPipeFile {
    mem: MemGate,
    pipe: Rc<Pipe>,
}

impl IndirectPipeFile {
    fn new(mem: Selector, pipe: &Rc<Pipe>) -> Self {
        IndirectPipeFile {
            mem: MemGate::new_bind(mem),
            pipe: pipe.clone(),
        }
    }

    fn collect_caps(&self, caps: &mut Vec<Selector>, reading: bool) {
        caps.push(self.mem.sel());
        caps.push(self.pipe.meta_gate_sel());
        if reading {
            caps.push(self.pipe.read_gate_sel());
        }
        else {
            caps.push(self.pipe.write_gate_sel());
        }
        self.pipe.attach(reading).unwrap();
    }

    fn serialize(&self, s: &mut VecSink) {
        s.push(&self.mem.sel());
        s.push(&self.pipe.sel());
        s.push(&self.pipe.meta_gate_sel());
        s.push(&self.pipe.read_gate_sel());
        s.push(&self.pipe.write_gate_sel());
    }

    fn unserialize(s: &mut SliceSource) -> Self {
        let mem = s.pop();
        let sess = s.pop();
        let meta_gate = s.pop();
        let rd_gate = s.pop();
        let wr_gate = s.pop();
        IndirectPipeFile {
            mem: MemGate::new_bind(mem),
            pipe: Rc::new(Pipe::new_bind(sess, meta_gate, rd_gate, wr_gate)),
        }
    }
}

pub struct IndirectPipeReader {
    file: IndirectPipeFile,
}

impl IndirectPipeReader {
    fn new(mem: Selector, pipe: &Rc<Pipe>) -> FileHandle {
        Rc::new(RefCell::new(IndirectPipeReader {
            file: IndirectPipeFile::new(mem, pipe),
        }))
    }

    pub fn unserialize(s: &mut SliceSource) -> FileHandle {
        Rc::new(RefCell::new(IndirectPipeReader {
            file: IndirectPipeFile::unserialize(s),
        }))
    }
}

impl File for IndirectPipeReader {
    fn close(&mut self) {
        // ignore errors here
        self.file.pipe.close(true, 0).ok();
    }

    fn flags(&self) -> OpenFlags {
        OpenFlags::R
    }

    fn stat(&self) -> Result<FileInfo, Error> {
        Err(Error::new(Code::NotSup))
    }

    fn file_type(&self) -> u8 {
        b'I'
    }

    fn collect_caps(&self, caps: &mut Vec<Selector>) {
        self.file.collect_caps(caps, true);
    }

    fn serialize(&self, _mounts: &MountTable, s: &mut VecSink) {
        self.file.serialize(s);
    }
}

impl Seek for IndirectPipeReader {
    fn seek(&mut self, _off: usize, _whence: SeekMode) -> Result<usize, Error> {
        Err(Error::new(Code::NotSup))
    }
}

impl Read for IndirectPipeReader {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize, Error> {
        time::start(0xbbbb);
        let (pos, count) = self.file.pipe.request_read(buf.len())?;
        time::stop(0xbbbb);
        if count == 0 {
            return Ok(0);
        }

        time::start(0xaaaa);
        self.file.mem.read(&mut buf[0..count], pos)?;
        time::stop(0xaaaa);
        Ok(count)
    }
}

impl Write for IndirectPipeReader {
    fn flush(&mut self) -> Result<(), Error> {
        Ok(())
    }

    fn write(&mut self, _buf: &[u8]) -> Result<usize, Error> {
        Err(Error::new(Code::NotSup))
    }
}

impl Map for IndirectPipeReader {
    fn map(&self, _pager: &Pager, _virt: usize, _off: usize, _len: usize, _prot: Perm) -> Result<(), Error> {
        Err(Error::new(Code::NotSup))
    }
}

impl fmt::Debug for IndirectPipeReader {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "IndirPipeReader[sess={}]", self.file.pipe.meta_gate_sel())
    }
}

pub struct IndirectPipeWriter {
    file: IndirectPipeFile,
    last_write: usize,
}

impl IndirectPipeWriter {
    fn new(mem: Selector, pipe: &Rc<Pipe>) -> FileHandle {
        Rc::new(RefCell::new(IndirectPipeWriter {
            file: IndirectPipeFile::new(mem, pipe),
            last_write: 0,
        }))
    }

    pub fn unserialize(s: &mut SliceSource) -> FileHandle {
        Rc::new(RefCell::new(IndirectPipeWriter {
            file: IndirectPipeFile::unserialize(s),
            last_write: 0,
        }))
    }
}

impl File for IndirectPipeWriter {
    fn close(&mut self) {
        // ignore errors here
        self.file.pipe.close(false, self.last_write).ok();
    }

    fn flags(&self) -> OpenFlags {
        OpenFlags::W
    }

    fn stat(&self) -> Result<FileInfo, Error> {
        Err(Error::new(Code::NotSup))
    }

    fn file_type(&self) -> u8 {
        b'J'
    }

    fn collect_caps(&self, caps: &mut Vec<Selector>) {
        self.file.collect_caps(caps, false);
    }

    fn serialize(&self, _mounts: &MountTable, s: &mut VecSink) {
        self.file.serialize(s);
    }
}

impl Seek for IndirectPipeWriter {
    fn seek(&mut self, _off: usize, _whence: SeekMode) -> Result<usize, Error> {
        Err(Error::new(Code::NotSup))
    }
}

impl Read for IndirectPipeWriter {
    fn read(&mut self, _buf: &mut [u8]) -> Result<usize, Error> {
        Err(Error::new(Code::NotSup))
    }
}

impl Write for IndirectPipeWriter {
    fn flush(&mut self) -> Result<(), Error> {
        Ok(())
    }

    fn write(&mut self, buf: &[u8]) -> Result<usize, Error> {
        time::start(0xbbbb);
        let pos = self.file.pipe.request_write(buf.len(), self.last_write)?;
        time::stop(0xbbbb);

        time::start(0xaaaa);
        self.file.mem.write(buf, pos)?;
        time::stop(0xaaaa);
        self.last_write = buf.len();
        Ok(buf.len())
    }
}

impl Map for IndirectPipeWriter {
    fn map(&self, _pager: &Pager, _virt: usize, _off: usize, _len: usize, _prot: Perm) -> Result<(), Error> {
        Err(Error::new(Code::NotSup))
    }
}

impl fmt::Debug for IndirectPipeWriter {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "IndirPipeWriter[sess={}, last_write={:#x}]",
            self.file.pipe.meta_gate_sel(), self.last_write)
    }
}
