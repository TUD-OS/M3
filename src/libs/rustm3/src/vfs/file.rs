use cap::Selector;
use col::Vec;
use com::VecSink;
use errors::Error;
use io::{Read, Write};
use kif;
use serialize::{Marshallable, Unmarshallable, Sink, Source};
use session::Pager;
use vfs::{BlockId, DevId, INodeId, FileMode, MountTable};

int_enum! {
    pub struct SeekMode : u32 {
        const SET       = 0x0;
        const CUR       = 0x1;
        const END       = 0x2;
    }
}

bitflags! {
    pub struct OpenFlags : u32 {
        const R         = 0b000001;
        const W         = 0b000010;
        const X         = 0b000100;
        const TRUNC     = 0b001000;
        const APPEND    = 0b010000;
        const CREATE    = 0b100000;

        const RW        = Self::R.bits | Self::W.bits;
        const RX        = Self::R.bits | Self::X.bits;
        const RWX       = Self::R.bits | Self::W.bits | Self::X.bits;
    }
}

#[derive(Copy, Clone, Debug)]
#[repr(C, packed)]
pub struct FileInfo {
    pub devno: DevId,
    pub inode: INodeId,
    pub mode: FileMode,
    pub links: u32,
    pub size: usize,
    pub lastaccess: u32,
    pub lastmod: u32,
    // for debugging
    pub extents: u32,
    pub firstblock: BlockId,
}

impl Marshallable for FileInfo {
    fn marshall(&self, s: &mut Sink) {
        s.push(&self.devno);
        s.push(&{self.inode});
        s.push(&{self.mode});
        s.push(&{self.links});
        s.push(&{self.size});
        s.push(&{self.lastaccess});
        s.push(&{self.lastmod});
        s.push(&{self.extents});
        s.push(&{self.firstblock});
    }
}

impl Unmarshallable for FileInfo {
    fn unmarshall(s: &mut Source) -> Self {
        FileInfo {
            devno:      s.pop_word() as DevId,
            inode:      s.pop_word() as INodeId,
            mode:       s.pop_word() as FileMode,
            links:      s.pop_word() as u32,
            size:       s.pop_word() as usize,
            lastaccess: s.pop_word() as u32,
            lastmod:    s.pop_word() as u32,
            extents:    s.pop_word() as u32,
            firstblock: s.pop_word() as BlockId,
        }
    }
}

pub trait File : Read + Write + Seek + Map {
    fn flags(&self) -> OpenFlags;

    fn stat(&self) -> Result<FileInfo, Error>;

    fn file_type(&self) -> u8;
    fn collect_caps(&self, caps: &mut Vec<Selector>);
    fn serialize(&self, mounts: &MountTable, s: &mut VecSink);
}

pub trait Seek {
    fn seek(&mut self, off: usize, whence: SeekMode) -> Result<usize, Error>;
}

pub trait Map {
    fn map(&self, pager: &Pager, virt: usize,
           off: usize, len: usize, prot: kif::Perm) -> Result<(), Error>;
}
