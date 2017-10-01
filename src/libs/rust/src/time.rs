pub type Time = u64;

const START_TSC: u64    = 0x1FF10000;
const STOP_TSC: u64     = 0x1FF20000;

fn gem5_debug(msg: u64) -> Time {
    let res: Time;
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

pub fn start(msg: u64) -> Time {
    gem5_debug(START_TSC | msg)
}

pub fn stop(msg: u64) -> Time {
    gem5_debug(STOP_TSC | msg)
}
