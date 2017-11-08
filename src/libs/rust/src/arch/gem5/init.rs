use arch;
use com;
use heap;
use io;
use syscalls;
use util;
use vpe;

pub fn exit(code: i32) {
    syscalls::exit(code);
    util::jmp_to(arch::env::data().exit_addr());
}

extern "C" {
    fn main() -> i32;
}

#[no_mangle]
pub extern "C" fn env_run() {
    let envdata = arch::env::data();
    let res = if envdata.has_lambda() {
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
