use cfg;
use util;

fn get_bp() -> usize {
    let val: usize;
    unsafe {
        asm!(
            "mov %rbp, $0"
            : "=r"(val)
        );
    }
    val
}

pub fn collect(addr: &mut [usize]) -> usize {
    let mut bp = get_bp();

    let base = util::round_dn(bp, cfg::STACK_SIZE);
    let end = util::round_up(bp, cfg::STACK_SIZE);
    let start = end - cfg::STACK_SIZE;

    for i in 0..addr.len() {
        if bp < start || bp >= end {
            return i;
        }

        bp = base + (bp & cfg::STACK_SIZE - 1);
        let bp_ptr = bp as *const usize;
        unsafe {
            addr[i] = *bp_ptr.offset(1) - 5;
            bp = *bp_ptr;
        }
    }
    return addr.len();
}
