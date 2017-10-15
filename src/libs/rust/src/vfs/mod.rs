pub mod bufio;
pub mod file;
pub mod filesystem;
pub mod regfile;

pub use self::bufio::{BufReader, BufWriter};
pub use self::file::{File, FileInfo, FileMode, OpenFlags, SeekMode, Seek, Read, Write};
pub use self::filesystem::FileSystem;
pub use self::regfile::RegularFile;

pub mod tests {
    pub use super::regfile::tests as regfile;
    pub use super::bufio::tests as bufio;
}
