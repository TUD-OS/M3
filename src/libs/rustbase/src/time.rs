//! Contains time measurement functions

use arch::time;

/// A timestamp
pub type Time = u64;

/// Starts a time measurement with given message
pub fn start(msg: u64) -> Time {
    time::start(msg)
}

/// Stops a time measurement with given message
pub fn stop(msg: u64) -> Time {
    time::stop(msg)
}
