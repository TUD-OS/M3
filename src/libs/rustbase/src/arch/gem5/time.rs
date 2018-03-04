use arch::cpu;
use time;

const START_TSC: usize    = 0x1FF10000;
const STOP_TSC: usize     = 0x1FF20000;

pub fn start(msg: usize) -> time::Time {
    cpu::gem5_debug(START_TSC | msg)
}

pub fn stop(msg: usize) -> time::Time {
    cpu::gem5_debug(STOP_TSC | msg)
}
