//! Contains the backtrace generation function

use arch::cfg;
use arch::cpu;
use util;

/// Walks up the stack and stores the return addresses into the given slice and returns the number
/// of addresses.
///
/// The function assumes that the stack is aligned by `cfg::STACK_SIZE` and ensures to not access
/// below or above the stack.
pub fn collect(addr: &mut [usize]) -> usize {
    let mut bp = cpu::get_bp();

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
            addr[i] = *bp_ptr.offset(1);
            if addr[i] >= 5 {
                addr[i] -= 5;
            }
            bp = *bp_ptr;
        }
    }
    return addr.len();
}
