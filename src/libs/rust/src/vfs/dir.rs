use cell::RefCell;
use collections::String;
use core::iter;
use errors::Error;
use rc::Rc;
use util;
use session::M3FS;
use vfs::{BufReader, FileSystem, INodeId, OpenFlags, RegularFile, Read, Seek, SeekMode};

#[derive(Debug)]
pub struct DirEntry {
    inode: INodeId,
    name: String,
}

impl DirEntry {
    pub fn new(inode: INodeId, name: String) -> Self {
        DirEntry {
            inode: inode,
            name: name,
        }
    }

    pub fn inode(&self) -> INodeId {
        self.inode
    }

    pub fn file_name(&self) -> &str {
        &self.name
    }
}

pub struct ReadDir {
    reader: BufReader<RegularFile>,
}

impl iter::Iterator for ReadDir {
    type Item = DirEntry;

    fn next(&mut self) -> Option<Self::Item> {
        #[repr(C, packed)]
        struct M3FSDirEntry {
            inode: INodeId,
            name_len: u32,
            next: u32,
        }

        // read header
        let entry: M3FSDirEntry = match self.reader.read_object() {
            Ok(obj) => obj,
            Err(_)  => return None,
        };

        // read name
        let res = DirEntry::new(
            entry.inode,
            match self.reader.read_string(entry.name_len as usize) {
                Ok(s)   => s,
                Err(_)  => return None,
            },
        );

        // move to next entry
        let off = entry.next as usize - (util::size_of::<M3FSDirEntry>() + entry.name_len as usize);
        if off != 0 {
            if self.reader.seek(off, SeekMode::CUR).is_err() {
                return None
            }
        }

        Some(res)
    }
}

// TODO remove the argument as soon as we have the VFS
pub fn read_dir(sess: Rc<RefCell<M3FS>>, path: &str) -> Result<ReadDir, Error> {
    let dir = try!(sess.borrow_mut().open(path, OpenFlags::R));
    Ok(ReadDir {
        reader: BufReader::new(dir),
    })
}
