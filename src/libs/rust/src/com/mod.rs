mod epmux;
mod gate;
mod mgate;
mod rgate;
mod sgate;
#[macro_use]
mod stream;

pub use self::mgate::*;
pub use self::rgate::{RecvGate, RGateArgs, RBufSpace};
pub use self::sgate::*;
pub use self::epmux::EpMux;
pub use self::stream::*;

pub fn init() {
    rgate::init();
}
