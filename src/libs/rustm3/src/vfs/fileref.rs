use core::ops::Deref;
use core::fmt;
use errors::Error;
use goff;
use io::{Read, Write};
use kif;
use session::Pager;
use vfs::filetable::Fd;
use vfs::{FileHandle, Map, Seek, SeekMode};
use vpe::VPE;

#[derive(Clone)]
pub struct FileRef {
    file: FileHandle,
    fd: Fd,
}

impl FileRef {
    pub fn new(file: FileHandle, fd: Fd) -> Self {
        FileRef {
            file: file,
            fd: fd,
        }
    }

    pub fn fd(&self) -> Fd {
        self.fd
    }
    pub fn handle(&self) -> FileHandle {
        self.file.clone()
    }
}

impl Drop for FileRef {
    fn drop(&mut self) {
        VPE::cur().files().remove(self.fd);
    }
}

impl Deref for FileRef {
    type Target = FileHandle;

    fn deref(&self) -> &FileHandle {
        &self.file
    }
}

impl Read for FileRef {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize, Error> {
        self.file.borrow_mut().read(buf)
    }
}

impl Write for FileRef {
    fn flush(&mut self) -> Result<(), Error> {
        self.file.borrow_mut().flush()
    }
    fn write(&mut self, buf: &[u8]) -> Result<usize, Error> {
        self.file.borrow_mut().write(buf)
    }
}

impl Seek for FileRef {
    fn seek(&mut self, off: usize, whence: SeekMode) -> Result<usize, Error> {
        self.file.borrow_mut().seek(off, whence)
    }
}

impl Map for FileRef {
    fn map(&self, pager: &Pager, virt: goff, off: usize, len: usize, prot: kif::Perm) -> Result<(), Error> {
        self.file.borrow().map(pager, virt, off, len, prot)
    }
}

impl fmt::Debug for FileRef {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "FileRef[fd={}, file={:?}]", self.fd, self.file.borrow())
    }
}
