use arch::dtu::{EpId, Label};
use col::{String, Vec};
use kif::PEDesc;
use libc;

pub struct EnvData {
    pub pe: u64,
    pub argc: u32,
    pub argv: u64,

    pub fds: u64,   // TODO
    pub pedesc: PEDesc,

    pub sysc_crd: u64,
    pub sysc_lbl: Label,
    pub sysc_ep: EpId,
    pub shm_prefix: String,
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
        fds: 0,
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
