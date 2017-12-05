use base::dtu;
use base::env;
use base::envdata;
use base::heap;
use base::io;
use base::kif;
use base::libc;
use thread;

use arch::kdtu::KDTU;
use arch::loader;
use com;
use mem;
use pes;
use platform;
use tests;
use workloop::workloop;

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

    unsafe {
        libc::mkdir("/tmp/m3\0".as_ptr() as *const i8, 0o755);
    }

    com::init();
    mem::init();
    KDTU::init();
    platform::init();
    loader::init();
    pes::vpemng::init();
    thread::init();

    for _ in 0..8 {
        thread::ThreadManager::get().add_thread(workloop as *const () as u64, 0);
    }

    let rbuf = vec![0u8; 512 * 32];
    dtu::DTU::configure_recv(0, rbuf.as_ptr() as usize, 14, 9);

    let serv_rbuf = vec![0u8; 1024];
    dtu::DTU::configure_recv(2, serv_rbuf.as_ptr() as usize, 10, 10);

    let vpemng = pes::vpemng::get();
    let mut args = env::args();
    args.next();
    vpemng.start(args).expect("init failed");

    klog!(DEF, "Kernel is ready!");

    workloop();

    klog!(DEF, "Shutting down");
    0
}
