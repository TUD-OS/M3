use base::col::Vec;
use base::cell::MutCell;
use base::dtu::PEId;
use base::errors::{Code, Error};
use base::kif;
use base::libc;
use core::ptr;
use core::sync::atomic;

use arch::kdtu;
use pes::{State, VPE, VPEId};
use pes::vpemng;
use platform;

const MAX_ARGS_LEN: usize = 4096;

pub fn init() {
    for i in platform::pes() {
        let desc: kif::PEDesc = platform::pe_desc(i);
        klog!(KENV,
            "PE{:02}: {} {} {} KiB memory",
            i, desc.pe_type(), desc.isa(), desc.mem_size() / 1024
        );
    }

    unsafe {
        libc::signal(libc::SIGCHLD, sigchld_handler as usize);
    }
}

static mut SIGCHLDS: atomic::AtomicUsize = atomic::AtomicUsize::new(0);

extern "C" fn sigchld_handler(_sig: i32) {
    unsafe {
        SIGCHLDS.fetch_add(1, atomic::Ordering::Relaxed);
        libc::signal(libc::SIGCHLD, sigchld_handler as usize);
    }
}

pub fn check_childs() {
    unsafe {
        while SIGCHLDS.load(atomic::Ordering::Relaxed) > 0 {
            SIGCHLDS.fetch_sub(1, atomic::Ordering::Relaxed);

            let mut status = 0;
            let pid = libc::wait(&mut status);
            if libc::WIFEXITED(status) {
                klog!(VPES, "Child {} exited with status {}", pid, libc::WEXITSTATUS(status));
            }
            else if libc::WIFSIGNALED(status) {
                klog!(VPES, "Child {} was killed by signal {}", pid, libc::WTERMSIG(status));
            }

            if libc::WIFSIGNALED(status) || libc::WEXITSTATUS(status) == 255 {
                if let Some(vid) = vpemng::get().pid_to_vpeid(pid) {
                    vpemng::get().remove(vid);
                }
            }
        }
    }
}

pub fn kill_child(pid: i32) {
    unsafe {
        libc::kill(pid, libc::SIGTERM);
        libc::waitpid(pid, ptr::null_mut(), 0);
    }
}

pub struct Loader {
}

impl Loader {
    pub fn get() -> &'static mut Loader {
        static LOADER: MutCell<Loader> = MutCell::new(Loader {});
        LOADER.get_mut()
    }

    pub fn load_app(&mut self, vpe: &mut VPE) -> Result<i32, Error> {
        if vpe.pid() != 0 {
            vpe.set_state(State::RUNNING);
            Self::write_env_file(vpe.pid(), vpe.id(), vpe.pe_id());
            return Ok(vpe.pid());
        }

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

                klog!(KENV, "Loading mod '{}':", vpe.args()[0]);

                unsafe {
                    libc::execv(argv[0], argv.as_ptr());
                    // special error code to let the WorkLoop delete the VPE
                    libc::exit(255);
                }
            }
            pid => {
                vpe.set_state(State::RUNNING);
                Ok(pid)
            },
        }
    }

    fn write_env_file(pid: i32, id: VPEId, pe: PEId) {
        let path = format!("/tmp/m3/{}\0", pid);
        let data = format!(
            "{}\n{}\n{}\n{}\n{}\n",
            "foo",  // TODO SHM prefix
            pe,
            id,
            kdtu::KSYS_EP,
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
