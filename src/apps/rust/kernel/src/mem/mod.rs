mod main;
mod map;
mod module;

pub use self::main::{MainMemory, Allocation};
pub use self::map::*;
pub use self::module::*;

pub fn init() {
    main::init()
}

pub fn get() -> &'static mut MainMemory {
    main::get()
}
