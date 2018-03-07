mod bufio;
mod dir;
mod file;
mod fileref;
mod filesystem;
mod filetable;
mod genericfile;
mod indirpipe;
mod mounttable;
mod vfs;

pub type FileMode = u16;
pub type DevId = u8;
pub type INodeId = u32;
pub type BlockId = u32;

pub use self::bufio::{BufReader, BufWriter};
pub use self::dir::{DirEntry, ReadDir, read_dir};
pub use self::file::{File, FileInfo, Map, OpenFlags, SeekMode, Seek};
pub use self::fileref::FileRef;
pub use self::filesystem::FileSystem;
pub use self::filetable::{Fd, FileHandle, FileTable};
pub use self::genericfile::GenericFile;
pub use self::indirpipe::IndirectPipe;
pub use self::mounttable::{FSHandle, MountTable};

#[allow(non_snake_case)]
pub mod VFS {
    pub use vfs::vfs::*;
}

pub fn deinit() {
    filetable::deinit();
}
