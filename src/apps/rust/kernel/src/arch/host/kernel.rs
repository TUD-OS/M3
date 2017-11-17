use base::dtu;
use base::env;
use base::envdata;
use base::heap;
use base::io;
use base::kif;
use base::libc;

use arch::kdtu::KDTU;
use mem;
use pes;
use platform;
use syscalls;
use tests;

#[no_mangle]
pub extern "C" fn rust_init(argc: i32, argv: *const *const i8) {
    extern "C" {
        fn dummy_func();
    }

    // ensure that host's init library gets linked in
    unsafe {
        dummy_func();
    }

    envdata::set(envdata::EnvData::new(
        0,
        kif::PEDesc::new(kif::PEType::COMP_IMEM, kif::PEISA::X86, 1024 * 1024),
        argc,
        argv
    ));
    heap::init();
    io::init();
    dtu::init();
}

#[no_mangle]
pub extern "C" fn rust_deinit(_status: i32, _arg: *const libc::c_void) {
    dtu::deinit();
}

#[no_mangle]
pub fn main() -> i32 {
    if let Some(a) = env::args().nth(1) {
        if a == "test" {
            tests::run();
        }
    }

    mem::init();
    KDTU::init();
    platform::init();
    pes::vpemng::init();

    unsafe {
        libc::mkdir("/tmp/m3\0".as_ptr() as *const i8, 0o755);
    }

    let rbuf = vec![0u8; 512 * 32];
    dtu::DTU::configure_recv(0, rbuf.as_ptr() as usize, 14, 9);

    let vpemng = pes::vpemng::get();
    let mut args = env::args();
    args.next();
    vpemng.start(args).expect("init failed");

    while vpemng.count() > 0 {
        if let Some(msg) = dtu::DTU::fetch_msg(0) {
            syscalls::handle(msg);
        }
    }

    0
}
