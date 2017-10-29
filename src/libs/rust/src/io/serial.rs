use cap::Selector;
use cell::RefCell;
use collections::Vec;
use com::VecSink;
use dtu;
use errors::Error;
use kif;
use libc;
use rc::Rc;
use session::Pager;
use vfs;

pub struct Serial {
}

static mut SERIAL: Option<Rc<RefCell<Serial>>> = None;

impl Serial {
    fn new() -> Self {
        Serial {}
    }

    pub fn get() -> &'static Rc<RefCell<Serial>> {
        unsafe { &SERIAL.as_ref().unwrap() }
    }
}

impl vfs::Read for Serial {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize, Error> {
        unsafe {
            Ok(libc::gem5_readfile(buf.as_mut_ptr(), buf.len() as u64, 0) as usize)
        }
    }
}

impl vfs::Write for Serial {
    fn flush(&mut self) -> Result<(), Error> {
        Ok(())
    }

    fn write(&mut self, buf: &[u8]) -> Result<usize, Error> {
        dtu::DTU::print(buf);
        unsafe {
            libc::gem5_writefile(buf.as_ptr(), buf.len() as u64, 0, "stdout\0".as_ptr() as u64);
        }
        Ok(buf.len())
    }
}

impl vfs::Seek for Serial {
    fn seek(&mut self, _off: usize, _whence: vfs::SeekMode) -> Result<usize, Error> {
        Err(Error::NotSup)
    }
}

impl vfs::Map for Serial {
    fn map(&self, _pager: &Pager, _virt: usize,
           _off: usize, _len: usize, _prot: kif::Perm) -> Result<(), Error> {
        Err(Error::NotSup)
    }
}

impl vfs::File for Serial {
    fn flags(&self) -> vfs::OpenFlags {
        vfs::OpenFlags::RW
    }

    fn stat(&self) -> Result<vfs::FileInfo, Error> {
        Err(Error::NotSup)
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

pub fn init() {
    unsafe {
        if SERIAL.is_none() {
            SERIAL = Some(Rc::new(RefCell::new(Serial::new())));
        }
    }
}
