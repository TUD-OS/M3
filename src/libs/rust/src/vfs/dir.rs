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
        let mut entry = M3FSDirEntry { inode: 0, name_len: 0, next: 0 };
        if self.reader.read_exact(util::object_to_bytes_mut(&mut entry)).is_err() {
            return None
        }

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

pub mod tests {
    use collections::{Vec,ToString};
    use core::cmp;
    use super::*;

    pub fn run(t: &mut ::test::Tester) {
        run_test!(t, list_dir);
    }

    fn list_dir() {
        let m3fs = M3FS::new("m3fs").expect("connect to m3fs failed");

        // read a dir with known content
        let dirname = "/largedir";
        let mut vec = Vec::new();
        for e in assert_ok!(read_dir(m3fs.clone(), dirname)) {
            vec.push(e);
        }
        assert_eq!(vec.len(), 82);

        // sort the entries; keep "." and ".." at the front
        vec.sort_unstable_by(|a, b| {
            let aname = a.file_name();
            let bname = b.file_name();
            let aspec = aname == "." || aname == "..";
            let bspec = bname == "." || bname == "..";
            match (aspec, bspec) {
                (true, true)    => aname.cmp(bname),
                (true, false)   => cmp::Ordering::Less,
                (false, true)   => cmp::Ordering::Greater,
                (false, false)  => {
                    // cut off ".txt"
                    let anum = aname[0..aname.len() - 4].parse::<i32>().unwrap();
                    let bnum = bname[0..bname.len() - 4].parse::<i32>().unwrap();
                    anum.cmp(&bnum)
                }
            }
        });

        // now check file names
        assert_eq!(vec[0].file_name(), ".");
        assert_eq!(vec[1].file_name(), "..");
        for i in 0..80 {
            assert_eq!(i.to_string() + ".txt", vec[i + 2].file_name());
        }
    }
}
