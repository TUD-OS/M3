mod epmux;
mod memgate;
mod gate;

pub use self::memgate::*;
pub use self::epmux::EpMux;

use dtu;

// TODO move that to somewhere else?
pub fn init() {
    for ep in 0..dtu::FIRST_FREE_EP {
        EpMux::get().reserve(ep);
    }
}
