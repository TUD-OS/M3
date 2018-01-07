use arch;
use com;
use heap;
use io;
use libc;
use syscalls;
use vfs;
use vpe;

fn rust_exit(code: i32) {
    io::deinit();
    vfs::deinit();
    syscalls::exit(code);
    arch::dtu::deinit();
}

#[no_mangle]
pub extern "C" fn rust_init(argc: i32, argv: *const *const i8) {
    extern "C" {
        fn dummy_func();
    }

    // ensure that host's init library gets linked in
    unsafe {
        dummy_func();
    }

    heap::init();
    arch::env::init(argc, argv);
    vpe::init();
    io::init();
    com::init();
    arch::dtu::init();
}

#[no_mangle]
pub extern "C" fn rust_deinit(status: i32, _arg: *const libc::c_void) {
    rust_exit(status);
}
