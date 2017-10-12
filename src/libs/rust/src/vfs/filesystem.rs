use errors::Error;
use vfs::{File, OpenFlags, FileInfo, FileMode};

pub trait FileSystem<F : File + Sized> {
    fn open(&mut self, path: &str, perms: OpenFlags) -> Result<F, Error>;

    fn stat(&mut self, path: &str) -> Result<FileInfo, Error>;

    fn mkdir(&mut self, path: &str, mode: FileMode) -> Result<(), Error>;
    fn rmdir(&mut self, path: &str) -> Result<(), Error>;

    fn link(&mut self, old_path: &str, new_path: &str) -> Result<(), Error>;
    fn unlink(&mut self, path: &str) -> Result<(), Error>;
}
