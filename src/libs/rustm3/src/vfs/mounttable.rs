use cap::Selector;
use cell::RefCell;
use col::{String, ToString, Vec};
use com::{SliceSource, VecSink};
use core::fmt;
use errors::{Code, Error};
use rc::Rc;
use serialize::Sink;
use session::M3FS;
use vfs::FileSystem;

pub type FSHandle = Rc<RefCell<FileSystem>>;

pub struct MountPoint {
    path: String,
    fs: FSHandle,
}

impl MountPoint {
    pub fn new(path: &str, fs: FSHandle) -> MountPoint {
        MountPoint {
            path: path.to_string(),
            fs: fs,
        }
    }
}

#[derive(Default)]
pub struct MountTable {
    mounts: Vec<MountPoint>,
}

impl MountTable {
    pub fn add(&mut self, path: &str, fs: FSHandle) -> Result<(), Error> {
        if self.path_to_idx(path).is_some() {
            return Err(Error::new(Code::Exists))
        }

        let pos = self.insert_pos(path);
        self.mounts.insert(pos, MountPoint::new(path, fs));
        Ok(())
    }

    pub fn get_by_path(&self, path: &str) -> Option<FSHandle> {
        match self.path_to_idx(path) {
            Some(i) => Some(self.mounts[i].fs.clone()),
            None    => None,
        }
    }
    pub fn get_by_index(&self, mid: usize) -> Option<FSHandle> {
        match self.mounts.get(mid) {
            Some(mp) => Some(mp.fs.clone()),
            None     => None,
        }
    }

    pub fn index_of(&self, fs: &FSHandle) -> Option<usize> {
        for (i, m) in self.mounts.iter().enumerate() {
            if Rc::ptr_eq(&m.fs, fs) {
                return Some(i)
            }
        }
        None
    }

    pub fn resolve(&self, path: &str) -> Result<(FSHandle, usize), Error> {
        for m in &self.mounts {
            if path.starts_with(m.path.as_str()) {
                return Ok((m.fs.clone(), m.path.len()))
            }
        }
        Err(Error::new(Code::NoSuchFile))
    }

    pub fn remove(&mut self, path: &str) -> Result<(), Error> {
        match self.path_to_idx(path) {
            Some(i) => {
                self.mounts.remove(i);
                Ok(())
            },
            None    => Err(Error::new(Code::NoSuchFile)),
        }
    }

    pub fn collect_caps(&self, caps: &mut Vec<Selector>) {
        for m in &self.mounts {
            m.fs.borrow().collect_caps(caps);
        }
    }

    pub fn serialize(&self, s: &mut VecSink) {
        let count = self.mounts.len();
        s.push(&count);

        for m in &self.mounts {
            let fs = m.fs.borrow();
            let fs_type = fs.fs_type();
            s.push(&m.path);
            s.push(&fs_type);
            fs.serialize(s);
        }
    }

    pub fn unserialize(s: &mut SliceSource) -> MountTable {
        let mut mt = MountTable::default();

        let count = s.pop();
        for _ in 0..count {
            let path: String = s.pop();
            let fs_type: u8 = s.pop();
            mt.add(&path, match fs_type {
                b'M' => M3FS::unserialize(s),
                _    => panic!("Unexpected fs type {}", fs_type),
            }).unwrap();
        }

        mt
    }

    fn path_to_idx(&self, path: &str) -> Option<usize> {
        // TODO support imperfect paths
        assert!(path.starts_with("/"));
        assert!(path.ends_with("/"));
        assert!(path.find("..").is_none());

        for (i, m) in self.mounts.iter().enumerate() {
            if m.path == path {
                return Some(i)
            }
        }
        None
    }

    fn insert_pos(&self, path: &str) -> usize {
        let comp_count = Self::path_comps(path);
        for (i, m) in self.mounts.iter().enumerate() {
            let cnt = Self::path_comps(m.path.as_str());
            if comp_count > cnt {
                return i;
            }
        }
        self.mounts.len()
    }

    fn path_comps(path: &str) -> usize {
        let mut comp_count = path.chars().filter(|&c| c == '/').count();
        if !path.ends_with('/') {
            comp_count += 1;
        }
        comp_count
    }
}

impl fmt::Debug for MountTable {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "MountTable[\n")?;
        for m in self.mounts.iter() {
            write!(f, "  {} -> {:?}\n", m.path, m.fs.borrow())?;
        }
        write!(f, "]")
    }
}
