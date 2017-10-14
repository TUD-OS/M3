mod epmux;
mod gate;
mod mgate;
mod rgate;
mod sgate;
#[macro_use]
mod stream;

pub use self::mgate::{MemGate, MGateArgs, Perm};
pub use self::rgate::{RecvGate, RGateArgs, RBufSpace};
pub use self::sgate::{SendGate, SGateArgs};
pub use self::epmux::EpMux;
pub use self::stream::*;

pub mod tests {
    pub use super::mgate::tests as mgate;
    pub use super::rgate::tests as rgate;
    pub use super::sgate::tests as sgate;
}

pub fn init() {
    rgate::init();
}
