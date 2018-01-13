use cap::Selector;
use col::Vec;
use com::VecSink;
use errors::{Code, Error};
use io;
use kif;
use session::Pager;
use vfs;

impl vfs::Seek for io::Serial {
    fn seek(&mut self, _off: usize, _whence: vfs::SeekMode) -> Result<usize, Error> {
        Err(Error::new(Code::NotSup))
    }
}

impl vfs::Map for io::Serial {
    fn map(&self, _pager: &Pager, _virt: usize,
           _off: usize, _len: usize, _prot: kif::Perm) -> Result<(), Error> {
        Err(Error::new(Code::NotSup))
    }
}

impl vfs::File for io::Serial {
    fn close(&mut self) {
    }

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
