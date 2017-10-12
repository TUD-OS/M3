use cell::{RefCell, RefMut};
use com::MemGate;
use errors::Error;
use kif::INVALID_SEL;
use rc::Rc;
use session::{ExtId, Fd, M3FS, LocList, LocFlags};
use time;
use vfs;

struct FilePos {
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
            extended: false,
            local: LocList::MAX as ExtId,
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

    pub fn valid(&self) -> bool {
        self.local < LocList::MAX as ExtId
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
        if !self.valid() || (writing && self.locs.get_len(self.local) == 0) {
            self.request(sess, writing, rebind)
        }
        else {
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
    }

    pub fn adjust_written_part(&mut self) {
        if self.extended && self.global > self.last_extent ||
           (self.global == self.last_extent && self.off > self.last_off) {
            self.last_extent = self.global;
            self.last_off = self.off;
        }
    }

    fn request(&mut self, mut sess: RefMut<M3FS>, writing: bool, rebind: bool) -> Result<usize, Error> {
        // move forward
        self.begin += self.length;
        self.length = 0;
        self.local = 0;

        // get new locations
        let flags = if writing { LocFlags::EXTEND } else { LocFlags::empty() };
        self.locs.clear();
        self.extended = try!(sess.get_locs(self.fd, self.global, &mut self.locs, flags)).1;

        // cache new length
        self.length = self.locs.total_length();

        let len = self.locs.get_len(0);
        if self.off == len {
            self.next_extent();
        }
        if rebind {
            try!(self.mem.rebind(self.locs.get_sel(self.local)));
        }
        Ok(self.locs.get_len(self.local))
    }

    fn next_extent(&mut self) {
        self.local += 1;
        self.global += 1;
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

        // seek to beginning?
        let pos = if whence == vfs::SeekMode::SET && off == 0 {
            Position {
                global: 0,
                extoff: 0,
                pos: 0,
            }
        }
        // is it already in our local data?
        // TODO we could support that for SEEK_CUR as well
        else if whence == vfs::SeekMode::SET && self.pos.valid() &&
                off >= self.pos.begin && off < self.pos.begin + self.pos.length {
            // this is always successful, because we checked the range before
            let (ext, begin) = self.pos.find(off).unwrap();

            self.pos.local = ext;
            Position {
                global: self.pos.global + (ext - self.pos.local),
                extoff: off - begin,
                pos: begin,
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
            self.pos.global = pos.global;
            self.pos.local = LocList::MAX as ExtId;
            self.pos.begin = pos.pos;
            self.pos.length = 0;
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
            let lastglobal = self.pos.global;
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
            let lastglobal = self.pos.global;
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
