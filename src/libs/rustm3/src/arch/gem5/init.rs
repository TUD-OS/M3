use arch;
use com;
use heap;
use io;
use syscalls;
use util;
use vfs;
use vpe;

#[no_mangle]
pub extern "C" fn exit(code: i32) {
    io::deinit();
    vfs::deinit();
    syscalls::exit(code);
    util::jmp_to(arch::env::get().exit_addr());
}

extern "C" {
    fn main() -> i32;
}

#[no_mangle]
pub extern "C" fn env_run() {
    let res = if arch::env::get().has_lambda() {
        io::reinit();
        vpe::reinit();
        com::reinit();
        arch::env::closure().call()
    }
    else {
        heap::init();
        vpe::init();
        io::init();
        com::init();
        unsafe { main() }
    };
    exit(res)
}
