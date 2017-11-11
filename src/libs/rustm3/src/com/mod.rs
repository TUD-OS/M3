#[macro_use]
mod stream;

mod epmux;
mod gate;
mod mgate;
mod rgate;
mod sgate;

pub use self::mgate::{MemGate, MGateArgs, Perm};
pub use self::rgate::{RecvGate, RGateArgs};
pub use self::sgate::{SendGate, SGateArgs};
pub use self::epmux::EpMux;
pub use self::stream::*;

pub fn init() {
    rgate::init();
}

pub fn reinit() {
    epmux::EpMux::get().reset();
}
