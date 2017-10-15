use collections::Vec;
use errors::Error;
use util;
use vfs::{SeekMode, Read, Seek, Write};

pub struct BufReader<R : Read> {
    reader: R,
    buf: Vec<u8>,
    pos: usize,
    cap: usize,
}

impl<R : Read> BufReader<R> {
    pub fn new(reader: R) -> Self {
        Self::with_capacity(reader, 512)
    }

    pub fn with_capacity(reader: R, cap: usize) -> Self {
        let mut br = BufReader {
            reader: reader,
            buf: Vec::with_capacity(cap),
            pos: 0,
            cap: 0,
        };
        unsafe { br.buf.set_len(cap) };
        br
    }
}

impl<R : Read> Read for BufReader<R> {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize, Error> {
        if self.pos >= self.cap {
            let end = self.buf.len();
            self.cap = try!(self.reader.read(&mut self.buf.as_mut_slice()[0..end]));
            self.pos = 0;
        }

        let end = util::min(self.cap, self.pos + buf.len());
        let res = end - self.pos;
        if end > self.pos {
            &buf[0..res].copy_from_slice(&self.buf[self.pos..end]);
        }
        self.pos += res;
        Ok(res)
    }
}

impl<R: Read + Seek> Seek for BufReader<R> {
    fn seek(&mut self, off: usize, whence: SeekMode) -> Result<usize, Error> {
        if whence != SeekMode::CUR || off != 0 {
            // invalidate buffer
            self.pos = 0;
            self.cap = 0;
        }
        self.reader.seek(off, whence)
    }
}

pub struct BufWriter<W : Write> {
    writer: W,
    buf: Vec<u8>,
    pos: usize,
}

impl<W : Write> BufWriter<W> {
    pub fn new(writer: W) -> Self {
        Self::with_capacity(writer, 512)
    }

    pub fn with_capacity(writer: W, cap: usize) -> Self {
        let mut br = BufWriter {
            writer: writer,
            buf: Vec::with_capacity(cap),
            pos: 0,
        };
        unsafe { br.buf.set_len(cap) };
        br
    }
}

impl<W : Write> Write for BufWriter<W> {
    fn write(&mut self, buf: &[u8]) -> Result<usize, Error> {
        if buf.len() > self.buf.len() {
            try!(self.flush());

            self.writer.write(buf)
        }
        else {
            let end = util::min(self.buf.len(), self.pos + buf.len());
            let res = end - self.pos;
            if end > self.pos {
                &self.buf[self.pos..end].copy_from_slice(&buf[0..res]);
            }

            self.pos += res;
            Ok(res)
        }
    }

    fn flush(&mut self) -> Result<(), Error> {
        if self.pos > 0 {
            try!(self.writer.write(&self.buf[0..self.pos]));
            self.pos = 0;
        }

        Ok(())
    }
}

impl<W: Write + Seek> Seek for BufWriter<W> {
    fn seek(&mut self, off: usize, whence: SeekMode) -> Result<usize, Error> {
        if whence != SeekMode::CUR || off != 0 {
            try!(self.flush());
        }
        self.writer.seek(off, whence)
    }
}

impl<W: Write> Drop for BufWriter<W> {
    fn drop(&mut self) {
        self.flush().unwrap();
    }
}

pub mod tests {
    use super::*;
    use collections::String;
    use session::M3FS;
    use vfs::*;

    pub fn run(t: &mut ::test::Tester) {
        run_test!(t, read_write);
    }

    fn read_write() {
        let m3fs = M3FS::new("m3fs").expect("connect to m3fs failed");

        {
            let file = assert_ok!(m3fs.borrow_mut().open("/myfile",
                OpenFlags::CREATE | OpenFlags::W));
            let mut bfile = BufWriter::new(file);

            assert_ok!(write!(bfile, "This {:.3} is the {}th test of {:#0X}!\n", "foobar", 42, 0xABCDEF));
        }

        {
            let file = assert_ok!(m3fs.borrow_mut().open("/myfile", OpenFlags::R));
            let mut bfile = BufReader::new(file);

            let mut s = String::new();
            assert_eq!(bfile.read_to_string(&mut s), Ok(39));
            assert_eq!(s, "This foo is the 42th test of 0xABCDEF!\n");
        }
    }
}
