use errors::Error;
use vfs::{File, OpenFlags, FileInfo, FileMode};

pub trait FileSystem<F : File + Sized> {
    fn open(&self, path: &str, perms: OpenFlags) -> Result<F, Error>;

    fn stat(&self, path: &str) -> Result<FileInfo, Error>;

    fn mkdir(&self, path: &str, mode: FileMode) -> Result<(), Error>;
    fn rmdir(&self, path: &str) -> Result<(), Error>;

    fn link(&self, old_path: &str, new_path: &str) -> Result<(), Error>;
    fn unlink(&self, path: &str) -> Result<(), Error>;
}
