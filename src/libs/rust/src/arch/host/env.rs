use arch;
use arch::dtu::{EpId, Label};
use cap::Selector;
use col::{String, Vec};
use core::intrinsics;
use kif::PEDesc;
use libc;
use session::Pager;
use vfs::{FileTable, MountTable};
use vpe;

pub struct EnvData {
    pe: u64,
    argc: u32,
    argv: u64,

    pedesc: PEDesc,

    sysc_crd: u64,
    sysc_lbl: Label,
    sysc_ep: EpId,
    shm_prefix: String,
}

impl EnvData {
    pub fn pe_id(&self) -> u64 {
        self.pe
    }

    pub fn argc(&self) -> usize {
        self.argc as usize
    }
    pub fn argv(&self) -> *const *const i8 {
        self.argv as *const *const i8
    }

    pub fn pedesc<'a, 'e : 'a>(&'e self) -> &'a PEDesc {
        &self.pedesc
    }
    pub fn next_sel(&self) -> Selector {
        // TODO
        2
    }
    pub fn eps(&self) -> u64 {
        // TODO
        0
    }

    pub fn vpe(&self) -> &'static mut vpe::VPE {
        unsafe {
            // TODO
            intrinsics::transmute(0u64)
        }
    }

    pub fn load_rbufs(&self) -> arch::rbufs::RBufSpace {
        arch::rbufs::RBufSpace::new()
    }

    pub fn load_pager(&self) -> Option<Pager> {
        None
    }

    pub fn load_mounts(&self) -> MountTable {
        // TODO
        MountTable::default()
    }

    pub fn load_fds(&self) -> FileTable {
        // TODO
        FileTable::default()
    }

    // --- host specific API ---

    pub fn syscall_params(&self) -> (EpId, Label, u64) {
        (self.sysc_ep, self.sysc_lbl, self.sysc_crd)
    }
}

static mut ENV_DATA: Option<EnvData> = None;

pub fn data() -> &'static mut EnvData {
    unsafe {
        ENV_DATA.as_mut().unwrap()
    }
}

fn read_line(fd: i32) -> String {
    let mut vec = Vec::new();
    loop {
        let mut buf = [0u8; 1];
        if unsafe { libc::read(fd, buf.as_mut_ptr() as *mut libc::c_void, 1) } == 0 {
            break;
        }
        if buf[0] == b'\n' {
            break;
        }
        vec.push(buf[0]);
    }
    unsafe { String::from_utf8_unchecked(vec) }
}

pub fn init(argc: i32, argv: *const *const u8) {
    let fd = unsafe {
        let path = format!("/tmp/m3/{}", libc::getpid());
        libc::open(path.as_ptr() as *const libc::c_char, libc::O_RDONLY)
    };
    assert!(fd != -1);

    let data = EnvData {
        argc: argc as u32,
        argv: argv as u64,
        pedesc: PEDesc::new_from(0),

        shm_prefix: read_line(fd),
        pe: read_line(fd).parse::<u64>().unwrap(),
        sysc_lbl: read_line(fd).parse::<Label>().unwrap(),
        sysc_ep: read_line(fd).parse::<EpId>().unwrap(),
        sysc_crd: read_line(fd).parse::<u64>().unwrap(),
    };

    unsafe {
        libc::close(fd);
        ENV_DATA = Some(data);
    }
}
