use cell::{RefCell, RefMut};
use com::MemGate;
use errors::Error;
use kif::INVALID_SEL;
use rc::Rc;
use session::{ExtId, Fd, M3FS, LocList, LocFlags};
use time;
use vfs;

struct FilePos {
    valid: bool,
    extended: bool,
    local: ExtId,
    global: ExtId,
    off: usize,
    begin: usize,
    fd: Fd,
    locs: LocList,
    length: usize,
    mem: MemGate,
    last_extent: ExtId,
    last_off: usize,
}

impl FilePos {
    pub fn new(fd: Fd) -> Self {
        FilePos {
            valid: false,
            extended: false,
            local: 0,
            global: 0,
            off: 0,
            begin: 0,
            fd: fd,
            locs: LocList::new(),
            length: 0,
            mem: MemGate::new_bind(INVALID_SEL),
            last_extent: 0,
            last_off: 0,
        }
    }

    pub fn find(&mut self, off: usize) -> Option<(ExtId, usize)> {
        let mut begin = self.begin;
        for i in 0..self.locs.count() {
            let len = self.locs.get_len(i as ExtId);
            if len == 0 || (off >= begin && off < begin + len) {
                return Some((i as ExtId, begin));
            }
            begin += len;
        }
        None
    }

    pub fn get_amount(&mut self, extlen: usize, count: usize) -> usize {
        if count >= extlen - self.off {
            let res = extlen - self.off;
            self.next_extent();
            res
        }
        else {
            let res = count;
            self.off += res;
            res
        }
    }

    pub fn get(&mut self, sess: RefMut<M3FS>, writing: bool, rebind: bool) -> Result<usize, Error> {
        if !self.valid || self.locs.get_len(self.local) == 0 {
            try!(self.request(sess, writing));
        }

        // don't read past the so far written part
        if self.extended && !writing && self.global + self.local >= self.last_extent {
            if self.global + self.local > self.last_extent {
                Ok(0)
            }
            // take care that there is at least something to read; if not, break here to not advance
            // to the next extent (see get_amount).
            else if self.off >= self.last_off {
                Ok(0)
            }
            else {
                Ok(self.last_off)
            }
        }
        else {
            let len = self.locs.get_len(self.local);

            if rebind && len != 0 && self.mem.sel() != self.locs.get_sel(self.local) {
                try!(self.mem.rebind(self.locs.get_sel(self.local)));
            }
            Ok(len)
        }
    }

    pub fn adjust_written_part(&mut self) {
        if self.extended && self.global + self.local > self.last_extent ||
           (self.global + self.local == self.last_extent && self.off > self.last_off) {
            self.last_extent = self.global + self.local;
            self.last_off = self.off;
        }
    }

    fn request(&mut self, mut sess: RefMut<M3FS>, writing: bool) -> Result<(), Error> {
        let flags = if writing { LocFlags::EXTEND } else { LocFlags::empty() };

        // move forward
        self.begin += self.length;
        self.length = 0;
        self.global += self.local;
        self.local = 0;
        self.locs.clear();

        // get new locations
        self.extended |= try!(sess.get_locs(self.fd, self.global, &mut self.locs, flags)).1;
        self.valid = true;

        // cache new length
        self.length = self.locs.total_length();

        let len = self.locs.get_len(0);
        if self.off == len {
            self.next_extent();
        }
        Ok(())
    }

    fn next_extent(&mut self) {
        self.local += 1;
        self.off = 0;
    }
}

pub struct RegularFile {
    sess: Rc<RefCell<M3FS>>,
    pos: FilePos,
    flags: vfs::OpenFlags,
}


impl RegularFile {
    pub fn new(sess: Rc<RefCell<M3FS>>, fd: Fd, flags: vfs::OpenFlags) -> Self {
        RegularFile {
            sess: sess,
            pos: FilePos::new(fd),
            flags: flags,
        }
    }
}

impl vfs::File for RegularFile {
    fn flags(&self) -> vfs::OpenFlags {
        self.flags
    }

    fn stat(&self) -> Result<vfs::FileInfo, Error> {
        self.sess.borrow_mut().fstat(self.pos.fd)
    }

    fn seek(&mut self, off: usize, whence: vfs::SeekMode) -> Result<usize, Error> {
        struct Position {
            pub global: ExtId,
            pub extoff: usize,
            pub pos: usize,
        }

        // is it already in our local data?
        // TODO we could support that for SEEK_CUR as well
        let pos = if whence == vfs::SeekMode::SET && self.pos.valid &&
                     off >= self.pos.begin && off < self.pos.begin + self.pos.length {
            // this is always successful, because we checked the range before
            let (ext, begin) = self.pos.find(off).unwrap();

            self.pos.local = ext;
            Position {
                global: self.pos.global,
                extoff: off - begin,
                pos: off,
            }
        }
        // seek to beginning?
        else if whence == vfs::SeekMode::SET && off == 0 {
            Position {
                global: 0,
                extoff: 0,
                pos: 0,
            }
        }
        else {
            let (new_ext, new_ext_off, new_pos) = try!(self.sess.borrow_mut().seek(
                self.pos.fd, off, whence, self.pos.global, self.pos.off
            ));
            Position {
                global: new_ext,
                extoff: new_ext_off,
                pos: new_pos,
            }
        };

        // if our global extent has changed, we have to get new locations
        if pos.global != self.pos.global {
            self.pos.valid = false;
            self.pos.global = pos.global;
            self.pos.begin = pos.pos;
        }
        self.pos.off = pos.extoff;
        self.pos.adjust_written_part();

        log!(
            FS,
            "[{}] seek ({:#0x}, {}) -> ext={}, off={:#0x})",
            self.pos.fd, off, whence.val, self.pos.global, self.pos.off
        );

        Ok(pos.pos)
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
            let extlen = try!(self.pos.get(self.sess.borrow_mut(), false, true));
            if extlen == 0 {
                break;
            }

            // determine next off and idx
            let lastglobal = self.pos.global + self.pos.local;
            let memoff = self.pos.off;
            let amount = self.pos.get_amount(extlen, count);

            log!(
                FS,
                "[{}] read ({:#0x} bytes <- ext={}, off={:#0x})",
                self.pos.fd, amount, lastglobal, memoff
            );

            // read from global memory
            // we need to round up here because the filesize might not be a multiple of DTU_PKG_SIZE
            // in which case the last extent-size is not aligned
            time::start(0xaaaa);
            try!(self.pos.mem.read(&mut buf[bufoff..bufoff + amount], memoff));
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
            let extlen = try!(self.pos.get(self.sess.borrow_mut(), true, true));
            if extlen == 0 {
                break;
            }

            // determine next off and idx
            let lastglobal = self.pos.global + self.pos.local;
            let memoff = self.pos.off;
            let amount = self.pos.get_amount(extlen, count);

            // remember the max. position we wrote to
            if lastglobal >= self.pos.last_extent {
                if lastglobal > self.pos.last_extent || memoff + amount > self.pos.last_off {
                    self.pos.last_off = memoff + amount;
                }
                self.pos.last_extent = lastglobal;
            }

            log!(
                FS,
                "[{}] write ({:#0x} bytes -> ext={}, off={:#0x})",
                self.pos.fd, amount, lastglobal, memoff
            );

            // write to global memory
            time::start(0xaaaa);
            try!(self.pos.mem.write(&buf[bufoff..bufoff + amount], memoff));
            time::stop(0xaaaa);

            bufoff += amount;
            count -= amount;
        }
        Ok(bufoff)
    }
}

impl Drop for RegularFile {
    fn drop(&mut self) {
        self.sess.borrow_mut().close(self.pos.fd, self.pos.last_extent, self.pos.last_off).unwrap();
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

        let mut file = assert_ok!(m3fs.borrow_mut().open("/newfile", OpenFlags::CREATE | OpenFlags::RW));

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
            assert_eq!(file.seek(4 * 1024, SeekMode::SET), Ok(4 * 1024));
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
