use cap::Selector;
use cell::RefCell;
use collections::Vec;
use com::{Sink, VecSink, SliceSource};
use errors::Error;
use rc::Rc;
use vfs::{File, FileRef, MountTable, RegularFile};

pub type Fd = usize;

pub const MAX_FILES: usize = 32;

pub type FileHandle = Rc<RefCell<File>>;

#[derive(Default)]
pub struct FileTable {
    files: [Option<FileHandle>; MAX_FILES],
}

impl FileTable {
    pub fn add(&mut self, file: FileHandle) -> Result<FileRef, Error> {
        for fd in 0..MAX_FILES {
            if self.files[fd].is_none() {
                self.files[fd] = Some(file.clone());
                return Ok(FileRef::new(file, fd));
            }
        }
        Err(Error::NoSpace)
    }

    pub fn get(&self, fd: Fd) -> Option<FileHandle> {
        match self.files[fd] {
            Some(ref f) => Some(f.clone()),
            None        => None,
        }
    }

    pub fn set(&mut self, fd: Fd, file: FileHandle) {
        self.files[fd] = Some(file);
    }

    pub fn remove(&mut self, fd: Fd) {
        assert!(self.files[fd].is_some());
        self.files[fd] = None;
    }

    pub fn collect_caps(&self, caps: &mut Vec<Selector>) {
        for fd in 0..MAX_FILES {
            if let Some(ref f) = self.files[fd] {
                f.borrow().collect_caps(caps);
            }
        }
    }

    pub fn serialize(&self, mounts: &MountTable, s: &mut VecSink) {
        let count = self.files.iter().filter(|&f| f.is_some()).count();
        s.push(&count);

        for fd in 0..MAX_FILES {
            if let Some(ref f) = self.files[fd] {
                let file = f.borrow();
                s.push(&fd);
                s.push(&file.file_type());
                file.serialize(mounts, s);
            }
        }
    }

    pub fn unserialize(s: &mut SliceSource) -> FileTable {
        let mut ft = FileTable::default();

        let count = s.pop();
        for _ in 0..count {
            let fd: Fd = s.pop();
            let file_type: u8 = s.pop();
            ft.set(fd, match file_type {
                b'M' => RegularFile::unserialize(s),
                _    => panic!("Unexpected file type {}", file_type),
            });
        }

        ft
    }
}
