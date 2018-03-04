use time;

pub fn read8b(addr: usize) -> u64 {
    let res: u64;
    unsafe {
        asm!(
            "mov ($1), $0"
            : "=r"(res)
            : "r"(addr)
            : : "volatile"
        );
    }
    res
}

pub fn write8b(addr: usize, val: u64) {
    unsafe {
        asm!(
            "mov $0, ($1)"
            : : "r"(val), "r"(addr)
            : : "volatile"
        );
    }
}

pub fn get_sp() -> usize {
    let res: usize;
    unsafe {
        asm!(
            "mov %rsp, $0"
            : "=r"(res)
        );
    }
    res
}

pub fn get_bp() -> usize {
    let val: usize;
    unsafe {
        asm!(
            "mov %rbp, $0"
            : "=r"(val)
        );
    }
    val
}

pub fn jmp_to(addr: usize) {
    unsafe {
        asm!(
            "jmp *$0"
            : : "r"(addr)
        );
    }
}

pub fn gem5_debug(msg: usize) -> time::Time {
    let res: time::Time;
    unsafe {
        asm!(
            ".byte 0x0F, 0x04;
             .word 0x63"
            : "={rax}"(res)
            : "{rdi}"(msg)
        );
    }
    res
}
