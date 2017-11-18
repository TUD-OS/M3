use base::col::Vec;
use base::cell::RefMut;
use base::dtu::PEId;
use base::errors::{Code, Error};
use base::libc;

use pes::{VPE, VPEId};

const MAX_ARGS_LEN: usize = 4096;

pub struct Loader {
}

static mut LOADER: Loader = Loader {};

impl Loader {
    pub fn get() -> &'static mut Loader {
        unsafe {
            &mut LOADER
        }
    }

    pub fn load_app(&mut self, vpe: RefMut<VPE>) -> Result<i32, Error> {
        let pid = unsafe { libc::fork() };
        match pid {
            -1  => Err(Error::new(Code::OutOfMem)),
            0   => {
                let pid = unsafe { libc::getpid() };
                Self::write_env_file(pid, vpe.id(), vpe.pe_id());

                let mut buf = vec![0u8; MAX_ARGS_LEN];
                let mut argv: Vec<*const i8> = Vec::new();
                unsafe { buf.set_len(0) };
                for arg in vpe.args() {
                    // kernel argument?
                    if arg.starts_with("pe=") || arg == "daemon" || arg.starts_with("requires=") {
                        continue;
                    }

                    let ptr = buf.as_slice()[buf.len()..].as_ptr();
                    buf.extend_from_slice(arg.as_str().as_bytes());
                    buf.push(b'\0');
                    argv.push(ptr as *const i8);
                }
                argv.push(0 as *const i8);

                unsafe {
                    libc::execv(argv[0], argv.as_ptr());
                    // special error code to let the WorkLoop delete the VPE
                    libc::exit(255);
                }
            }
            pid => Ok(pid),
        }
    }

    fn write_env_file(pid: i32, id: VPEId, pe: PEId) {
        let path = format!("/tmp/m3/{}\0", pid);
        let data = format!(
            "{}\n{}\n{}\n{}\n{}\n",
            "foo",  // TODO SHM prefix
            pe,
            id,
            0,      // TODO syscall EP
            512,    // TODO credits
        );

        unsafe {
            let fd = libc::open(
                path.as_bytes().as_ptr() as *const i8,
                libc::O_WRONLY | libc::O_TRUNC | libc::O_CREAT,
                0o600
            );
            assert!(fd != -1);
            libc::write(fd, data.as_ptr() as *const libc::c_void, data.len());
            libc::close(fd);
        }
    }
}
