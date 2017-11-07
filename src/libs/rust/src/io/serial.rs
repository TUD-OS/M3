use arch;
use cap::Selector;
use cell::RefCell;
use col::Vec;
use com::VecSink;
use errors::{Code, Error};
use kif;
use rc::Rc;
use session::Pager;
use vfs;

pub struct Serial {
}

impl Serial {
    pub fn get() -> Rc<RefCell<Self>> {
        Rc::new(RefCell::new(Serial {}))
    }
}

impl vfs::Read for Serial {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize, Error> {
        arch::serial::read(buf)
    }
}

impl vfs::Write for Serial {
    fn flush(&mut self) -> Result<(), Error> {
        Ok(())
    }

    fn write(&mut self, buf: &[u8]) -> Result<usize, Error> {
        arch::serial::write(buf)
    }
}

impl vfs::Seek for Serial {
    fn seek(&mut self, _off: usize, _whence: vfs::SeekMode) -> Result<usize, Error> {
        Err(Error::new(Code::NotSup))
    }
}

impl vfs::Map for Serial {
    fn map(&self, _pager: &Pager, _virt: usize,
           _off: usize, _len: usize, _prot: kif::Perm) -> Result<(), Error> {
        Err(Error::new(Code::NotSup))
    }
}

impl vfs::File for Serial {
    fn flags(&self) -> vfs::OpenFlags {
        vfs::OpenFlags::RW
    }

    fn stat(&self) -> Result<vfs::FileInfo, Error> {
        Err(Error::new(Code::NotSup))
    }

    fn file_type(&self) -> u8 {
        b'S'
    }

    fn collect_caps(&self, _caps: &mut Vec<Selector>) {
        // nothing to do
    }

    fn serialize(&self, _mounts: &vfs::MountTable, _s: &mut VecSink) {
        // nothing to do
    }
}
