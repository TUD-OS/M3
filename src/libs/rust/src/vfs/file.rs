use cap::Selector;
use core::{fmt, intrinsics};
use com::{Marshallable, Unmarshallable, Sink, Source, VecSink};
use collections::*;
use errors::Error;
use kif;
use session::Pager;
use util;
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

#[derive(Debug)]
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
        s.push(&self.inode);
        s.push(&self.mode);
        s.push(&self.links);
        s.push(&self.size);
        s.push(&self.lastaccess);
        s.push(&self.lastmod);
        s.push(&self.extents);
        s.push(&self.firstblock);
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

pub trait Map {
    fn map(&self, pager: &Pager, virt: usize,
           off: usize, len: usize, prot: kif::Perm) -> Result<(), Error>;
}

pub trait Seek {
    fn seek(&mut self, off: usize, whence: SeekMode) -> Result<usize, Error>;
}

// this is inspired from std::io::{Read, Write}

pub trait Read {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize, Error>;

    fn read_string(&mut self, max: usize) -> Result<String, Error> {
        let mut buf = Vec::with_capacity(max);

        let mut off = 0;
        while off < max {
            // increase length so that we can write into the slice
            unsafe { buf.set_len(max); }
            let amount = self.read(&mut buf.as_mut_slice()[off..max])?;

            // stop on EOF
            if amount == 0 {
                break;
            }

            off += amount;
        }

        unsafe {
            // set final length
            buf.set_len(off);
            Ok(String::from_utf8_unchecked(buf))
        }
    }

    fn read_to_end(&mut self, buf: &mut Vec<u8>) -> Result<usize, Error> {
        let mut cap = util::max(64, buf.capacity() * 2);
        let old_len = buf.len();
        let mut off = old_len;

        'outer: loop {
            buf.reserve_exact(cap - off);

            while off < cap {
                // increase length so that we can write into the slice
                unsafe { buf.set_len(cap); }
                let count = self.read(&mut buf.as_mut_slice()[off..cap])?;

                // stop on EOF
                if count == 0 {
                    break 'outer;
                }

                off += count;
            }

            cap *= 2;
        }

        // set final length
        unsafe { buf.set_len(off); }
        Ok(off - old_len)
    }

    fn read_to_string(&mut self, buf: &mut String) -> Result<usize, Error> {
        self.read_to_end(unsafe { buf.as_mut_vec() })
    }

    fn read_exact(&mut self, mut buf: &mut [u8]) -> Result<(), Error> {
        while !buf.is_empty() {
            match self.read(buf) {
                Err(e)  => return Err(e),
                Ok(0)   => break,
                Ok(n)   => {
                    let tmp = buf;
                    buf = &mut tmp[n..];
                }
            }
        }

        if !buf.is_empty() {
            Err(Error::EndOfFile)
        }
        else {
            Ok(())
        }
    }
}

pub trait Write {
    fn write(&mut self, buf: &[u8]) -> Result<usize, Error>;

    fn flush(&mut self) -> Result<(), Error>;

    fn write_all(&mut self, mut buf: &[u8]) -> Result<(), Error> {
        while !buf.is_empty() {
            match self.write(buf) {
                Err(e)  => return Err(e),
                Ok(0)   => return Err(Error::WriteFailed),
                Ok(n)   => buf = &buf[n..],
            }
        }
        Ok(())
    }

    fn write_fmt(&mut self, fmt: fmt::Arguments) -> Result<(), Error> {
        // Create a shim which translates a Write to a fmt::Write and saves
        // off I/O errors. instead of discarding them
        struct Adaptor<'a, T: ?Sized + 'a> {
            inner: &'a mut T,
            error: Result<(), Error>,
        }

        impl<'a, T: Write + ?Sized> fmt::Write for Adaptor<'a, T> {
            fn write_str(&mut self, s: &str) -> fmt::Result {
                match self.inner.write_all(s.as_bytes()) {
                    Ok(()) => Ok(()),
                    Err(e) => {
                        self.error = Err(e);
                        Err(fmt::Error)
                    }
                }
            }
        }

        let mut output = Adaptor { inner: self, error: Ok(()) };
        match fmt::write(&mut output, fmt) {
            Ok(()) => Ok(()),
            Err(..) => {
                // check if the error came from the underlying `Write` or not
                if output.error.is_err() {
                    output.error
                } else {
                    Err(Error::WriteFailed)
                }
            }
        }
    }
}

pub fn read_object<T : Sized>(r: &mut Read) -> Result<T, Error> {
    let mut obj: T = unsafe { intrinsics::uninit() };
    r.read_exact(util::object_to_bytes_mut(&mut obj)).map(|_| obj)
}
