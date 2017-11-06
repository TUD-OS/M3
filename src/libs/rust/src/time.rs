use arch::time;

pub type Time = u64;

pub fn start(msg: u64) -> Time {
    time::start(msg)
}

pub fn stop(msg: u64) -> Time {
    time::stop(msg)
}
