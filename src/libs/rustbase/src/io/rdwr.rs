use core::{fmt, intrinsics};
use col::*;
use errors::{Code, Error};
use util;

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
            Err(Error::new(Code::EndOfFile))
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
                Ok(0)   => return Err(Error::new(Code::WriteFailed)),
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
                    Err(Error::new(Code::WriteFailed))
                }
            }
        }
    }
}

pub fn read_object<T : Sized>(r: &mut Read) -> Result<T, Error> {
    let mut obj: T = unsafe { intrinsics::uninit() };
    r.read_exact(util::object_to_bytes_mut(&mut obj)).map(|_| obj)
}
