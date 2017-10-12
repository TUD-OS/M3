use core::fmt;
use com::{Marshallable, Unmarshallable, GateOStream, GateIStream};
use collections::*;
use errors::Error;
use util;

pub type FileMode = u16;
pub type DevId = u8;
pub type INodeId = u32;
pub type BlockId = u32;

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
        const RWX       = Self::R.bits | Self::W.bits | Self::X.bits;
    }
}

#[derive(Debug)]
#[repr(C, packed)]
pub struct FileInfo {
    devno: DevId,
    inode: INodeId,
    mode: FileMode,
    links: u32,
    size: usize,
    lastaccess: u32,
    lastmod: u32,
    // for debugging
    extents: u32,
    firstblock: BlockId,
}

impl Marshallable for FileInfo {
    fn marshall(&self, os: &mut GateOStream) {
        os.push(&self.devno);
        os.push(&self.inode);
        os.push(&self.mode);
        os.push(&self.links);
        os.push(&self.size);
        os.push(&self.lastaccess);
        os.push(&self.lastmod);
        os.push(&self.extents);
        os.push(&self.firstblock);
    }
}

impl Unmarshallable for FileInfo {
    fn unmarshall(is: &mut GateIStream) -> Self {
        FileInfo {
            devno: is.pop(),
            inode: is.pop(),
            mode: is.pop(),
            links: is.pop(),
            size: is.pop(),
            lastaccess: is.pop(),
            lastmod: is.pop(),
            extents: is.pop(),
            firstblock: is.pop(),
        }
    }
}

pub trait File {
    fn flags(&self) -> OpenFlags;

    fn stat(&self) -> Result<FileInfo, Error>;

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
            unsafe { buf.set_len(off); }
            let amount = try!(self.read(&mut buf.as_mut_slice()[off..max]));

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
                let count = try!(self.read(&mut buf.as_mut_slice()[off..cap]));

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
