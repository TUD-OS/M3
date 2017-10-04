mod epmux;
mod gate;
mod mgate;
mod rgate;
mod sgate;

pub use self::mgate::*;
pub use self::rgate::{RecvGate, RGateArgs};
pub use self::sgate::*;
pub use self::epmux::EpMux;

use dtu;

// TODO move that to somewhere else?
pub fn init() {
    for ep in 0..dtu::FIRST_FREE_EP {
        EpMux::get().reserve(ep);
    }
    rgate::init();
}
