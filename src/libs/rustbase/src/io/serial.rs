//! Contains the serial struct

use arch;
use cell::RefCell;
use errors::Error;
use io;
use rc::Rc;

/// The serial line
pub struct Serial {
}

impl Serial {
    /// Creates a new serial line
    pub fn new() -> Rc<RefCell<Self>> {
        Rc::new(RefCell::new(Serial {}))
    }
}

impl io::Read for Serial {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize, Error> {
        arch::serial::read(buf)
    }
}

impl io::Write for Serial {
    fn flush(&mut self) -> Result<(), Error> {
        Ok(())
    }

    fn write(&mut self, buf: &[u8]) -> Result<usize, Error> {
        arch::serial::write(buf)
    }
}
