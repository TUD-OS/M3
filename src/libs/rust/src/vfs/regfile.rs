use cap::Selector;
use cell::RefCell;
use com::{Marshallable, Unmarshallable, MemGate, Sink, Source, VecSink, SliceSource};
use collections::Vec;
use core::fmt;
use errors::Error;
use kif::{Perm, INVALID_SEL};
use rc::Rc;
use session::{ExtId, FileId, M3FS, LocList, LocFlags, Pager};
use time;
use vfs;
use vpe::VPE;

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

    pub fn request_next(&mut self, sess: &M3FS, fid: FileId, writing: bool) -> Result<bool, Error> {
        // move forward
        self.offset += self.length;
        self.length = 0;
        self.first += self.locs.count() as ExtId;
        self.locs.clear();

        // get new locations
        let flags = if writing { LocFlags::EXTEND } else { LocFlags::empty() };
        let extended = sess.get_locs(fid, self.first, &mut self.locs, flags)?.1;

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

impl Marshallable for Position {
    fn marshall(&self, s: &mut Sink) {
        s.push(&self.ext);
        s.push(&self.extoff);
        s.push(&self.abs);
    }
}

impl Unmarshallable for Position {
    fn unmarshall(s: &mut Source) -> Self {
        Position::new_with(
            s.pop_word() as ExtId,
            s.pop_word() as usize,
            s.pop_word() as usize
        )
    }
}

macro_rules! sess_to_m3fs {
    ($s:expr) => (
        $s.borrow().as_any().downcast_ref::<M3FS>().unwrap()
    )
}

pub struct RegularFile {
    sess: vfs::FSHandle,
    fid: FileId,
    flags: vfs::OpenFlags,

    pos: Position,

    cache: ExtentCache,
    mem: MemGate,

    extended: bool,
    max_write: Position,
}

impl RegularFile {
    pub fn new(sess: vfs::FSHandle, fid: FileId, flags: vfs::OpenFlags) -> Self {
        Self::create(sess, fid, flags, Position::new(), false, Position::new())
    }

    fn create(sess: vfs::FSHandle, fid: FileId, flags: vfs::OpenFlags,
              pos: Position, extended: bool, max_write: Position) -> Self {
        RegularFile {
            sess: sess,
            fid: fid,
            flags: flags,
            pos: pos,
            cache: ExtentCache::new(),
            mem: MemGate::new_bind(INVALID_SEL),
            extended: extended,
            max_write: max_write,
        }
    }

    fn get_ext_len(&mut self, writing: bool, rebind: bool) -> Result<usize, Error> {
        if !self.cache.valid() || self.cache.ext_len(self.pos.ext) == 0 {
            self.extended |= self.cache.request_next(sess_to_m3fs!(self.sess), self.fid, writing)?;
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
                self.mem.rebind(self.cache.sel(self.pos.ext))?;
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
        // TODO good idea?
        if self.extended && self.pos.abs > self.max_write.abs {
            self.max_write = self.pos;
        }
    }

    fn advance(&mut self, count: usize, writing: bool) -> Result<usize, Error> {
        let extlen = self.get_ext_len(writing, true)?;
        if extlen == 0 {
            return Ok(0)
        }

        // determine next off and idx
        let lastpos = self.pos;
        let amount = self.pos.advance(extlen, count);

        // remember the max. position we wrote to
        if writing && lastpos.abs + amount > self.max_write.abs {
            self.max_write = lastpos;
            self.max_write.extoff += amount;
            self.max_write.abs += amount;
        }

        Ok(amount)
    }
}

impl vfs::File for RegularFile {
    fn flags(&self) -> vfs::OpenFlags {
        self.flags
    }

    fn stat(&self) -> Result<vfs::FileInfo, Error> {
        sess_to_m3fs!(self.sess).fstat(self.fid)
    }

    fn file_type(&self) -> u8 {
        b'M'
    }

    fn collect_caps(&self, _caps: &mut Vec<Selector>) {
        // nothing to do, because the child will fetch new caps
    }

    fn serialize(&self, mounts: &vfs::MountTable, s: &mut VecSink) {
        let mid = mounts.index_of(&self.sess).unwrap();
        s.push(&self.fid);
        s.push(&self.flags.bits());
        s.push(&mid);
        s.push(&self.extended);
        s.push(&self.pos);
        s.push(&self.max_write);
    }
}

impl RegularFile {
    pub fn unserialize(s: &mut SliceSource) -> vfs::FileHandle {
        let fid: FileId = s.pop();
        let flags: u32 = s.pop();
        let mid: usize = s.pop();
        let flags = vfs::OpenFlags::from_bits_truncate(flags);
        let pos: Position = s.pop();
        let extended: bool = s.pop();
        let max_write: Position = s.pop();

        let m3fs = VPE::cur().mounts().get_by_index(mid).unwrap();
        Rc::new(RefCell::new(
            Self::create(m3fs, fid, flags, pos, extended, max_write)
        ))
    }
}

impl vfs::Seek for RegularFile {
    fn seek(&mut self, mut off: usize, whence: vfs::SeekMode) -> Result<usize, Error> {
        // simple cases first
        let pos = if whence == vfs::SeekMode::CUR && off == 0 {
            self.pos
        }
        else if whence == vfs::SeekMode::SET && off == 0 {
            Position::new()
        }
        else {
            if whence == vfs::SeekMode::CUR {
                off += self.pos.abs;
            }

            // is it already in our cache?
            if whence != vfs::SeekMode::END && self.cache.contains_pos(off) {
                // this is always successful, because we checked the range before
                let (ext, begin) = self.cache.find(off).unwrap();

                Position {
                    ext: self.cache.first + ext,
                    extoff: off - begin,
                    abs: off,
                }
            }
            // otherwise, ask m3fs
            else {
                let (new_ext, new_ext_off, new_pos) = sess_to_m3fs!(self.sess).seek(
                    // TODO why all these arguments?
                    self.fid, off, whence, self.cache.first, self.pos.extoff
                )?;
                Position {
                    ext: new_ext,
                    extoff: new_ext_off,
                    abs: new_pos,
                }
            }
        };

        self.set_pos(pos);

        log!(
            FS,
            "[{}] seek ({:#0x}, {}) -> {}",
            self.fid, off, whence.val, self.pos
        );

        Ok(self.pos.abs)
    }
}

impl vfs::Read for RegularFile {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize, Error> {
        if (self.flags & vfs::OpenFlags::R).is_empty() {
            return Err(Error::NoPerm)
        }

        // determine the amount that we can read
        let lastpos = self.pos;
        let amount = self.advance(buf.len(), false)?;
        if amount == 0 {
            return Ok(0)
        }

        log!(
            FS,
            "[{}] read ({:#0x} bytes <- {})",
            self.fid, amount, lastpos
        );

        // read from global memory
        time::start(0xaaaa);
        self.mem.read(&mut buf[0..amount], lastpos.extoff)?;
        time::stop(0xaaaa);

        Ok(amount)
    }
}

impl vfs::Write for RegularFile {
    fn flush(&mut self) -> Result<(), Error> {
        Ok(())
    }

    fn write(&mut self, buf: &[u8]) -> Result<usize, Error> {
        if (self.flags & vfs::OpenFlags::W).is_empty() {
            return Err(Error::NoPerm)
        }

        // determine the amount that we can write
        let lastpos = self.pos;
        let amount = self.advance(buf.len(), true)?;
        if amount == 0 {
            return Ok(0)
        }

        log!(
            FS,
            "[{}] write ({:#0x} bytes -> {})",
            self.fid, amount, lastpos
        );

        // write to global memory
        time::start(0xaaaa);
        self.mem.write(&buf[0..amount], lastpos.extoff)?;
        time::stop(0xaaaa);

        Ok(amount)
    }
}

impl vfs::Map for RegularFile {
    fn map(&self, pager: &Pager, virt: usize, off: usize, len: usize, prot: Perm) -> Result<(), Error> {
        pager.map_ds(virt, len, off, prot, sess_to_m3fs!(self.sess).sess(), self.fid).map(|_| ())
    }
}

impl Drop for RegularFile {
    fn drop(&mut self) {
        sess_to_m3fs!(self.sess).close(self.fid, self.max_write.ext, self.max_write.extoff).unwrap();
    }
}
