mod server;
mod sesscon;

pub use self::server::{Handler, Server};
pub use self::sesscon::{SessId, SessionContainer};

use dtu::DTU;
use errors::Error;

pub fn server_loop<F : FnMut() -> Result<(), Error>>(mut func: F) -> Result<(), Error> {
    loop {
        DTU::try_sleep(true, 0).ok();

        try!(func());
    }
}
