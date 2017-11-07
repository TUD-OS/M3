use errors::{Code, Error};
use rc::Rc;
use session::M3FS;
use vfs::{FileInfo, FileMode, FileRef, FSHandle, OpenFlags};
use vpe::VPE;

pub fn mount(path: &str, fs: &str) -> Result<(), Error> {
    let fsobj = match fs {
        "m3fs" => M3FS::new(fs)?,
        _      => return Err(Error::new(Code::InvArgs)),
    };
    VPE::cur().mounts().add(path, fsobj)
}

pub fn unmount(path: &str) -> Result<(), Error> {
    VPE::cur().mounts().remove(path)
}

fn with_path<F, R>(path: &str, func: F) -> Result<R, Error>
                   where F : Fn(&FSHandle, usize) -> Result<R, Error> {
    let (fs, pos) = VPE::cur().mounts().resolve(path)?;
    func(&fs, pos)
}

pub fn open(path: &str, flags: OpenFlags) -> Result<FileRef, Error> {
    with_path(path, |fs, pos| {
        let file = fs.borrow_mut().open(&path[pos..], flags)?;
        VPE::cur().files().add(file)
    })
}

pub fn stat(path: &str) -> Result<FileInfo, Error> {
    with_path(path, |fs, pos| {
        fs.borrow().stat(&path[pos..])
    })
}

pub fn mkdir(path: &str, mode: FileMode) -> Result<(), Error> {
    with_path(path, |fs, pos| {
        fs.borrow().mkdir(&path[pos..], mode)
    })
}

pub fn rmdir(path: &str) -> Result<(), Error> {
    with_path(path, |fs, pos| {
        fs.borrow().rmdir(&path[pos..])
    })
}

pub fn link(old: &str, new: &str) -> Result<(), Error> {
    let (fs1, pos1) = VPE::cur().mounts().resolve(old)?;
    let (fs2, pos2) = VPE::cur().mounts().resolve(new)?;
    if !Rc::ptr_eq(&fs1, &fs2) {
        return Err(Error::new(Code::XfsLink))
    }
    let res = fs1.borrow().link(&old[pos1..], &new[pos2..]);
    res
}

pub fn unlink(path: &str) -> Result<(), Error> {
    with_path(path, |fs, pos| {
        fs.borrow().unlink(&path[pos..])
    })
}
