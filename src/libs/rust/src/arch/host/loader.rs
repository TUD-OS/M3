use col::{String, ToString, Vec};
use core::intrinsics;
use errors::{Code, Error};
use libc;
use vfs::{FileRef, Read};

pub struct Channel {
    fds: [i32; 2],
}

impl Channel {
    pub fn new() -> Result<Channel, Error> {
        let mut fds = [0i32; 2];
        match unsafe { libc::pipe(fds.as_mut_ptr()) } {
            -1  => Err(Error::new(Code::InvArgs)),
            _   => Ok(Channel { fds: fds })
        }
    }

    pub fn wait(&mut self) {
        unsafe {
            libc::close(self.fds[1]);
            self.fds[1] = -1;

            // wait until parent notifies us
            libc::read(self.fds[0], [0u8; 1].as_mut_ptr() as *mut libc::c_void, 1);
            libc::close(self.fds[0]);
            self.fds[0] = -1;
        }
    }

    pub fn signal(&mut self) {
        unsafe {
            libc::close(self.fds[0]);
            self.fds[0] = -1;

            // notify child; it can start now
            libc::write(self.fds[1], [0u8; 1].as_ptr() as *const libc::c_void, 1);
            libc::close(self.fds[1]);
            self.fds[1] = -1;
        }
    }
}

impl Drop for Channel {
    fn drop(&mut self) {
        unsafe {
            if self.fds[0] != -1 {
                libc::close(self.fds[0]);
            }
            if self.fds[1] != -1 {
                libc::close(self.fds[1]);
            }
        }
    }
}

pub fn copy_file(file: &mut FileRef) -> Result<String, Error> {
    let mut buf = vec![0u8; 4096];

    let mut path = "/tmp/m3-XXXXXX\0".to_string();

    unsafe {
        let tmp = libc::mkstemp(path.as_bytes_mut().as_mut_ptr() as *mut i8);
        if tmp < 0 {
            return Err(Error::new(Code::InvArgs));
        }

        // copy executable from m3fs to a temp file
        loop {
            let res = file.read(&mut buf)?;
            if res == 0 {
                break;
            }

            libc::write(tmp, buf.as_ptr() as *const libc::c_void, res);
        }

        // close writable fd to make it non-busy
        libc::close(tmp);
    }

    Ok(path)
}

pub fn read_env_file(suffix: &str) -> Option<Vec<u64>> {
    unsafe {
        let path = format!("/tmp/m3/{}-{}\0", libc::getpid(), suffix);
        let path_ptr = path.as_bytes().as_ptr() as *const i8;
        let fd = libc::open(path_ptr, libc::O_RDONLY);
        if fd == -1 {
            return None;
        }

        let mut info: libc::stat = intrinsics::uninit();
        assert!(libc::fstat(fd, &mut info) != -1);
        let size = info.st_size as usize;
        assert!(size & 7 == 0);

        let mut res: Vec<u64> = Vec::with_capacity(size);
        res.set_len(size / 8);
        libc::read(fd, res.as_mut_ptr() as *mut libc::c_void, size);

        libc::unlink(path_ptr);

        libc::close(fd);
        Some(res)
    }
}

pub fn write_env_file(pid: i32, suffix: &str, data: &[u64], size: usize) {
    let path = format!("/tmp/m3/{}-{}\0", pid, suffix);
    unsafe {
        let fd = libc::open(
            path.as_bytes().as_ptr() as *const i8,
            libc::O_WRONLY | libc::O_TRUNC | libc::O_CREAT,
            0o600
        );
        assert!(fd != -1);
        libc::write(fd, data.as_ptr() as *const libc::c_void, size);
        libc::close(fd);
    }
}

pub fn exec<S: AsRef<str>>(args: &[S], path: &String) -> ! {
    let mut buf = vec![0u8; 4096];

    unsafe {
        // copy args and null-terminate them
        let mut argv: Vec<*const i8> = Vec::new();
        buf.set_len(0);
        for arg in args {
            let ptr = buf.as_slice()[buf.len()..].as_ptr();
            buf.extend_from_slice(arg.as_ref().as_bytes());
            buf.push(b'\0');
            argv.push(ptr as *const i8);
        }
        argv.push(0 as *const i8);

        // open it readonly again as fexecve requires
        let path_ptr = path.as_bytes().as_ptr() as *const i8;
        let tmpdup = libc::open(path_ptr, libc::O_RDONLY);
        // we don't need it anymore afterwards
        libc::unlink(path_ptr);
        // it needs to be executable
        libc::fchmod(tmpdup, 0o700);

        // execute that file
        extern {
            static environ: *const *const i8;
        }
        libc::fexecve(tmpdup, argv.as_ptr(), environ);
        libc::_exit(1);
    }
}
