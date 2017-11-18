use base::env;
use base::dtu;
use base::heap;
use base::io;

use arch::kdtu::KDTU;
use arch::loader;
use mem;
use pes;
use platform;
use syscalls;
use tests;

extern {
    pub fn gem5_shutdown(delay: u64);
}

#[no_mangle]
pub extern "C" fn exit(_code: i32) {
    unsafe {
        gem5_shutdown(0);
    }
}

#[no_mangle]
pub extern "C" fn env_run() {
    heap::init();
    io::init();
    mem::init();

    if let Some(a) = env::args().nth(1) {
        if a == "test" {
            tests::run();
        }
    }

    KDTU::init();
    platform::init();
    loader::init();
    pes::vpemng::init();

    let rbuf = vec![0u8; 512 * 32];
    KDTU::get().recv_msgs(0, rbuf.as_ptr() as usize, 14, 9)
        .expect("Unable to config syscall REP");

    let vpemng = pes::vpemng::get();
    let mut args = env::args();
    args.next();

    vpemng.start(args).expect("init failed");

    klog!(DEF, "Kernel is ready!");

    while vpemng.count() > 0 {
        if let Some(msg) = dtu::DTU::fetch_msg(0) {
            syscalls::handle(msg);
        }
    }

    klog!(DEF, "Shutting down");
    exit(0);
}
