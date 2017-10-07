mod epmux;
mod gate;
mod mgate;
mod rgate;
mod sgate;

pub use self::mgate::*;
pub use self::rgate::{RecvGate, RGateArgs, RBufSpace};
pub use self::sgate::*;
pub use self::epmux::EpMux;

pub fn init() {
    rgate::init();
}
