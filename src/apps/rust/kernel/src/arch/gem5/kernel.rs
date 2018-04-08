use base::env;
use base::goff;
use base::heap;
use base::io;
use thread;

use arch::kdtu;
use arch::loader;
use arch::vm;
use com;
use mem;
use pes;
use platform;
use tests;
use workloop::workloop;

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
    vm::init();
    io::init();
    mem::init();

    if let Some(a) = env::args().nth(1) {
        if a == "test" {
            tests::run();
        }
    }

    com::init();
    kdtu::KDTU::init();
    platform::init();
    loader::init();
    pes::pemng::init();
    pes::vpemng::init();
    thread::init();

    for _ in 0..8 {
        thread::ThreadManager::get().add_thread(workloop as *const () as usize, 0);
    }

    let sysc_rbuf = vec![0u8; 512 * 32];
    kdtu::KDTU::get().recv_msgs(kdtu::KSYS_EP, sysc_rbuf.as_ptr() as goff, 14, 9)
        .expect("Unable to config syscall REP");

    let serv_rbuf = vec![0u8; 1024];
    kdtu::KDTU::get().recv_msgs(kdtu::KSRV_EP, serv_rbuf.as_ptr() as goff, 10, 8)
        .expect("Unable to config service REP");

    let vpemng = pes::vpemng::get();
    let mut args = env::args();
    args.next();

    vpemng.start(args).expect("init failed");

    klog!(DEF, "Kernel is ready!");

    workloop();

    pes::vpemng::deinit();
    klog!(DEF, "Shutting down");
    exit(0);
}
