use cap::Selector;
use cell::{RefCell, RefMut};
use com::MemGate;
use core::fmt;
use errors::Error;
use kif::INVALID_SEL;
use rc::Rc;
use session::{ExtId, Fd, M3FS, LocList, LocFlags};
use time;
use vfs;

struct ExtentCache {
    locs: LocList,
    first: ExtId,
    offset: usize,
    length: usize,
}

impl ExtentCache {
    pub fn new() -> Self {
        ExtentCache {
            locs: LocList::new(),
            first: 0,
            offset: 0,
            length: 0,
        }
    }

    pub fn valid(&self) -> bool {
        self.locs.count() > 0
    }
    pub fn invalidate(&mut self) {
        self.locs.clear();
    }

    pub fn contains_pos(&self, off: usize) -> bool {
        self.valid() && off >= self.offset && off < self.offset + self.length
    }
    pub fn contains_ext(&self, ext: ExtId) -> bool {
        ext >= self.first && ext < self.first + self.locs.count() as ExtId
    }

    pub fn ext_len(&self, ext: ExtId) -> usize {
        self.locs.get_len(ext - self.first)
    }
    pub fn sel(&self, ext: ExtId) -> Selector {
        self.locs.get_sel(ext - self.first)
    }

    pub fn find(&self, off: usize) -> Option<(ExtId, usize)> {
        let mut begin = self.offset;
        for i in 0..self.locs.count() {
            let len = self.locs.get_len(i as ExtId);
            if len == 0 || (off >= begin && off < begin + len) {
                return Some((i as ExtId, begin));
            }
            begin += len;
        }
        None
    }

    pub fn request_next(&mut self, mut sess: RefMut<M3FS>, fd: Fd, writing: bool) -> Result<bool, Error> {
        // move forward
        self.offset += self.length;
        self.length = 0;
        self.first += self.locs.count() as ExtId;
        self.locs.clear();

        // get new locations
        let flags = if writing { LocFlags::EXTEND } else { LocFlags::empty() };
        let extended = try!(sess.get_locs(fd, self.first, &mut self.locs, flags)).1;

        // cache new length
        self.length = self.locs.total_length();

        Ok(extended)
    }
}

#[derive(Copy, Clone)]
struct Position {
    ext: ExtId,
    extoff: usize,
    abs: usize,
}

impl Position {
    pub fn new() -> Self {
        Self::new_with(0, 0, 0)
    }

    pub fn new_with(ext: ExtId, extoff: usize, abs: usize) -> Self {
        Position {
            ext: ext,
            extoff: extoff,
            abs: abs,
        }
    }

    pub fn advance(&mut self, extlen: usize, count: usize) -> usize {
        if count >= extlen - self.extoff {
            let res = extlen - self.extoff;
            self.abs += res;
            self.ext += 1;
            self.extoff = 0;
            res
        }
        else {
            let res = count;
            self.extoff += res;
            self.abs += res;
            res
        }
    }
}

impl fmt::Display for Position {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Pos[abs={:#0x}, ext={}, extoff={:#0x}]", self.abs, self.ext, self.extoff)
    }
}

pub struct RegularFile {
    sess: Rc<RefCell<M3FS>>,
    fd: Fd,
    flags: vfs::OpenFlags,

    pos: Position,

    cache: ExtentCache,
    mem: MemGate,

    extended: bool,
    max_write: Position,
}

impl RegularFile {
    pub fn new(sess: Rc<RefCell<M3FS>>, fd: Fd, flags: vfs::OpenFlags) -> Self {
        RegularFile {
            sess: sess,
            fd: fd,
            flags: flags,
            pos: Position::new(),
            cache: ExtentCache::new(),
            mem: MemGate::new_bind(INVALID_SEL),
            extended: false,
            max_write: Position::new(),
        }
    }

    fn get_ext_len(&mut self, writing: bool, rebind: bool) -> Result<usize, Error> {
        if !self.cache.valid() || self.cache.ext_len(self.pos.ext) == 0 {
            self.extended |= try!(self.cache.request_next(self.sess.borrow_mut(), self.fd, writing));
        }

        // don't read past the so far written part
        if self.extended && !writing && self.pos.ext >= self.max_write.ext {
            if self.pos.ext > self.max_write.ext || self.pos.extoff >= self.max_write.extoff {
                Ok(0)
            }
            else {
                Ok(self.max_write.extoff)
            }
        }
        else {
            let len = self.cache.ext_len(self.pos.ext);

            if rebind && len != 0 && self.mem.sel() != self.cache.sel(self.pos.ext) {
                try!(self.mem.rebind(self.cache.sel(self.pos.ext)));
            }
            Ok(len)
        }
    }

    fn set_pos(&mut self, pos: Position) {
        self.pos = pos;

        // if our global extent has changed, we have to get new locations
        if !self.cache.contains_ext(pos.ext) {
            self.cache.invalidate();
            self.cache.first = pos.ext;
            self.cache.offset = pos.abs;
        }

        // update last write pos accordingly
        if self.extended && self.pos.abs > self.max_write.abs {
            self.max_write = self.pos;
        }
    }

    fn advance(&mut self, extlen: usize, count: usize, writing: bool) -> usize {
        let lastpos = self.pos;
        let amount = self.pos.advance(extlen, count);

        // remember the max. position we wrote to
        if writing && lastpos.abs + amount > self.max_write.abs {
            self.max_write = lastpos;
            self.max_write.extoff += amount;
            self.max_write.abs += amount;
        }
        amount
    }
}

impl vfs::File for RegularFile {
    fn flags(&self) -> vfs::OpenFlags {
        self.flags
    }

    fn stat(&self) -> Result<vfs::FileInfo, Error> {
        self.sess.borrow_mut().fstat(self.fd)
    }

    fn seek(&mut self, off: usize, whence: vfs::SeekMode) -> Result<usize, Error> {
        // is it already in our cache?
        // TODO we could support that for SEEK_CUR as well
        let pos = if whence == vfs::SeekMode::SET && self.cache.contains_pos(off) {
            // this is always successful, because we checked the range before
            let (ext, begin) = self.cache.find(off).unwrap();

            Position {
                ext: self.cache.first + ext,
                extoff: off - begin,
                abs: off,
            }
        }
        // seek to beginning?
        else if whence == vfs::SeekMode::SET && off == 0 {
            Position {
                ext: 0,
                extoff: 0,
                abs: 0,
            }
        }
        else {
            let (new_ext, new_ext_off, new_pos) = try!(self.sess.borrow_mut().seek(
                self.fd, off, whence, self.cache.first, self.pos.extoff
            ));
            Position {
                ext: new_ext,
                extoff: new_ext_off,
                abs: new_pos,
            }
        };

        let res = pos.abs;
        self.set_pos(pos);

        log!(
            FS,
            "[{}] seek ({:#0x}, {}) -> {}",
            self.fd, off, whence.val, self.pos
        );

        Ok(res)
    }
}

impl vfs::Read for RegularFile {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize, Error> {
        if (self.flags & vfs::OpenFlags::R).is_empty() {
            return Err(Error::NoPerm)
        }

        let mut count = buf.len();
        let mut bufoff = 0;
        while count > 0 {
            // figure out where that part of the file is in memory, based on our location db
            let extlen = try!(self.get_ext_len(false, true));
            if extlen == 0 {
                break;
            }

            // determine next off and idx
            let lastpos = self.pos;
            let amount = self.advance(extlen, count, false);

            log!(
                FS,
                "[{}] read ({:#0x} bytes <- {})",
                self.fd, amount, lastpos
            );

            // read from global memory
            time::start(0xaaaa);
            try!(self.mem.read(&mut buf[bufoff..bufoff + amount], lastpos.extoff));
            time::stop(0xaaaa);

            bufoff += amount;
            count -= amount;
        }
        Ok(bufoff)
    }
}

impl vfs::Write for RegularFile {
    fn flush(&mut self) -> Result<(), Error> {
        Err(Error::NotSup)
    }

    fn write(&mut self, buf: &[u8]) -> Result<usize, Error> {
        if (self.flags & vfs::OpenFlags::W).is_empty() {
            return Err(Error::NoPerm)
        }

        let mut count = buf.len();
        let mut bufoff = 0;
        while count > 0 {
            // figure out where that part of the file is in memory, based on our location db
            let extlen = try!(self.get_ext_len(true, true));
            if extlen == 0 {
                break;
            }

            // determine next off and idx
            let lastpos = self.pos;
            let amount = self.advance(extlen, count, true);

            log!(
                FS,
                "[{}] write ({:#0x} bytes -> {})",
                self.fd, amount, lastpos
            );

            // write to global memory
            time::start(0xaaaa);
            try!(self.mem.write(&buf[bufoff..bufoff + amount], lastpos.extoff));
            time::stop(0xaaaa);

            bufoff += amount;
            count -= amount;
        }
        Ok(bufoff)
    }
}

impl Drop for RegularFile {
    fn drop(&mut self) {
        self.sess.borrow_mut().close(self.fd, self.max_write.ext, self.max_write.extoff).unwrap();
    }
}

pub mod tests {
    use super::*;
    use collections::*;
    use vfs::*;

    pub fn run(t: &mut ::test::Tester) {
        run_test!(t, permissions);
        run_test!(t, read_string);
        run_test!(t, read_exact);
        run_test!(t, read_file_at_once);
        run_test!(t, read_file_in_small_steps);
        run_test!(t, read_file_in_large_steps);
        run_test!(t, write_and_read_file);
        run_test!(t, write_fmt);
        run_test!(t, extend_small_file);
        run_test!(t, overwrite_beginning);
        run_test!(t, truncate);
        run_test!(t, append);
        run_test!(t, append_read);
    }

    fn permissions() {
        let m3fs = M3FS::new("m3fs").expect("connect to m3fs failed");
        let filename = "/subdir/subsubdir/testfile.txt";
        let mut buf = [0u8; 16];

        {
            let mut file = assert_ok!(m3fs.borrow_mut().open(filename, OpenFlags::R));
            assert_err!(file.write(&buf), Error::NoPerm);
        }

        {
            let mut file = assert_ok!(m3fs.borrow_mut().open(filename, OpenFlags::W));
            assert_err!(file.read(&mut buf), Error::NoPerm);
        }
    }

    fn read_string() {
        let m3fs = M3FS::new("m3fs").expect("connect to m3fs failed");
        let filename = "/subdir/subsubdir/testfile.txt";
        let content = "This is a test!\n";

        let mut file = assert_ok!(m3fs.borrow_mut().open(filename, OpenFlags::R));

        for i in 0..content.len() {
            assert_eq!(file.seek(0, SeekMode::SET), Ok(0));
            let s = assert_ok!(file.read_string(i));
            assert_eq!(&s, &content[0..i]);
        }
    }

    fn read_exact() {
        let m3fs = M3FS::new("m3fs").expect("connect to m3fs failed");
        let filename = "/subdir/subsubdir/testfile.txt";
        let content = b"This is a test!\n";

        let mut file = assert_ok!(m3fs.borrow_mut().open(filename, OpenFlags::R));

        let mut buf = [0u8; 32];
        assert_ok!(file.read_exact(&mut buf[0..8]));
        assert_eq!(&buf[0..8], &content[0..8]);

        assert_eq!(file.seek(0, SeekMode::SET), Ok(0));
        assert_ok!(file.read_exact(&mut buf[0..16]));
        assert_eq!(&buf[0..16], &content[0..16]);

        assert_eq!(file.seek(0, SeekMode::SET), Ok(0));
        assert_err!(file.read_exact(&mut buf), Error::EndOfFile);
    }

    fn read_file_at_once() {
        let m3fs = M3FS::new("m3fs").expect("connect to m3fs failed");
        let filename = "/subdir/subsubdir/testfile.txt";

        let mut file = assert_ok!(m3fs.borrow_mut().open(filename, OpenFlags::R));
        let mut s = String::new();
        assert_eq!(file.read_to_string(&mut s), Ok(16));
        assert_eq!(s, "This is a test!\n");
    }

    fn read_file_in_small_steps() {
        let m3fs = M3FS::new("m3fs").expect("connect to m3fs failed");
        let filename = "/pat.bin";

        let mut file = assert_ok!(m3fs.borrow_mut().open(filename, OpenFlags::R));
        let mut buf = [0u8; 64];

        assert_eq!(_validate_pattern_content(&mut file, &mut buf), 64 * 1024);
    }

    fn read_file_in_large_steps() {
        let m3fs = M3FS::new("m3fs").expect("connect to m3fs failed");
        let filename = "/pat.bin";

        let mut file = assert_ok!(m3fs.borrow_mut().open(filename, OpenFlags::R));
        let mut buf = vec![0u8; 8 * 1024];

        assert_eq!(_validate_pattern_content(&mut file, &mut buf), 64 * 1024);
    }

    fn write_and_read_file() {
        let m3fs = M3FS::new("m3fs").expect("connect to m3fs failed");
        let content = "Foobar, a test and more and more and more!";
        let filename = "/mat.txt";

        let mut file = assert_ok!(m3fs.borrow_mut().open(filename, OpenFlags::RW));

        assert_ok!(write!(file, "{}", content));

        assert_eq!(file.seek(0, SeekMode::CUR), Ok(content.len()));
        assert_eq!(file.seek(0, SeekMode::SET), Ok(0));

        let res = assert_ok!(file.read_string(content.len()));
        assert_eq!(&content, &res);

        // undo the write
        let mut old = vec![0u8; content.len()];
        assert_eq!(file.seek(0, SeekMode::SET), Ok(0));
        for i in 0..content.len() {
            old[i] = i as u8;
        }
        assert_eq!(file.write(&old), Ok(content.len()));
    }

    fn write_fmt() {
        let m3fs = M3FS::new("m3fs").expect("connect to m3fs failed");

        let mut file = assert_ok!(m3fs.borrow_mut().open("/newfile",
            OpenFlags::CREATE | OpenFlags::RW));

        assert_ok!(write!(file, "This {:.3} is the {}th test of {:#0X}!\n", "foobar", 42, 0xABCDEF));
        assert_ok!(write!(file, "More formatting: {:?}", Some(Some(1))));

        assert_eq!(file.seek(0, SeekMode::SET), Ok(0));

        let mut s = String::new();
        assert_eq!(file.read_to_string(&mut s), Ok(69));
        assert_eq!(s, "This foo is the 42th test of 0xABCDEF!\nMore formatting: Some(Some(1))");
    }

    fn extend_small_file() {
        let m3fs = M3FS::new("m3fs").expect("connect to m3fs failed");

        {
            let mut file = assert_ok!(m3fs.borrow_mut().open("/test.txt", OpenFlags::W));

            let buf = _get_pat_vector(1024);
            for _ in 0..33 {
                assert_eq!(file.write(&buf[0..1024]), Ok(1024));
            }
        }

        _validate_pattern_file(m3fs, "/test.txt", 1024 * 33);
    }

    fn overwrite_beginning() {
        let m3fs = M3FS::new("m3fs").expect("connect to m3fs failed");

        {
            let mut file = assert_ok!(m3fs.borrow_mut().open("/test.txt", OpenFlags::W));

            let buf = _get_pat_vector(1024);
            for _ in 0..3 {
                assert_eq!(file.write(&buf[0..1024]), Ok(1024));
            }
        }

        _validate_pattern_file(m3fs, "/test.txt", 1024 * 33);
    }

    fn truncate() {
        let m3fs = M3FS::new("m3fs").expect("connect to m3fs failed");

        {
            let mut file = assert_ok!(m3fs.borrow_mut().open("/test.txt",
                OpenFlags::W | OpenFlags::TRUNC));

            let buf = _get_pat_vector(1024);
            for _ in 0..2 {
                assert_eq!(file.write(&buf[0..1024]), Ok(1024));
            }
        }

        _validate_pattern_file(m3fs, "/test.txt", 1024 * 2);
    }

    fn append() {
        let m3fs = M3FS::new("m3fs").expect("connect to m3fs failed");

        {
            let mut file = assert_ok!(m3fs.borrow_mut().open("/test.txt",
                OpenFlags::W | OpenFlags::APPEND));
            // TODO perform the seek to end here, because we cannot do that during open atm (m3fs
            // already borrowed as mutable). it's the wrong semantic anyway, so ...
            assert_ok!(file.seek(0, SeekMode::END));

            let buf = _get_pat_vector(1024);
            for _ in 0..2 {
                assert_eq!(file.write(&buf[0..1024]), Ok(1024));
            }
        }

        _validate_pattern_file(m3fs, "/test.txt", 1024 * 4);
    }

    fn append_read() {
        let m3fs = M3FS::new("m3fs").expect("connect to m3fs failed");

        {
            let mut file = assert_ok!(m3fs.borrow_mut().open("/test.txt",
                OpenFlags::RW | OpenFlags::TRUNC | OpenFlags::CREATE));

            let pat = _get_pat_vector(1024);
            for _ in 0..2 {
                assert_eq!(file.write(&pat[0..1024]), Ok(1024));
            }

            // there is nothing to read now
            let mut buf = [0u8; 1024];
            assert_eq!(file.read(&mut buf), Ok(0));

            // seek beyond the end
            assert_eq!(file.seek(2 * 1024, SeekMode::CUR), Ok(4 * 1024));
            // seek back
            assert_eq!(file.seek(2 * 1024, SeekMode::SET), Ok(2 * 1024));

            // now reading should work
            assert_eq!(file.read(&mut buf), Ok(1024));

            // seek back and overwrite
            assert_eq!(file.seek(2 * 1024, SeekMode::SET), Ok(2 * 1024));

            for _ in 0..2 {
                assert_eq!(file.write(&pat[0..1024]), Ok(1024));
            }
        }

        _validate_pattern_file(m3fs, "/test.txt", 1024 * 4);
    }

    fn _get_pat_vector(size: usize) -> Vec<u8> {
        let mut buf = Vec::with_capacity(size);
        for i in 0..1024 {
            buf.push(i as u8)
        }
        buf
    }

    fn _validate_pattern_file(sess: Rc<RefCell<M3FS>>, filename: &str, size: usize) {
        let mut file = assert_ok!(sess.borrow_mut().open(filename, OpenFlags::R));

        let info = assert_ok!(file.stat());
        assert_eq!(info.size, size);

        let mut buf = [0u8; 1024];
        assert_eq!(_validate_pattern_content(&mut file, &mut buf), size);
    }

    fn _validate_pattern_content(file: &mut RegularFile, mut buf: &mut [u8]) -> usize {
        let mut pos: usize = 0;
        loop {
            let count = assert_ok!(file.read(&mut buf));
            if count == 0 { break; }

            for i in 0..count {
                assert_eq!(buf[i], (pos & 0xFF) as u8,
                    "content wrong at offset {}: {} vs. {}", pos, buf[i], (pos & 0xFF) as u8);
                pos += 1;
            }
        }
        pos
    }
}
