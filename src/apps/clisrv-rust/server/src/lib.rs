#![no_std]

#[macro_use]
extern crate m3;

use m3::errors::Error;
use m3::collections::String;
use m3::com::*;
use m3::kif::{cap, service};
use m3::session::Session;
use m3::server::{Handler, Server, SessId, SessionContainer, server_loop};
use m3::util;

#[derive(Debug)]
struct MySession {
    arg: u64,
    sgate: SendGate,
}

struct MyHandler<'r> {
    sessions: SessionContainer<MySession>,
    rgate: RecvGate<'r>,
}

int_enum! {
    struct Operation : u64 {
        const REVERSE_STRING = 0x0;
    }
}

impl<'r> Handler<Session> for MyHandler<'r> {
    fn open(&mut self, arg: u64) -> Result<SessId, Error> {
        let sgate = SendGate::new_with(
            SGateArgs::new(&self.rgate).label(self.sessions.next_id()).credits(256)
        )?;
        Ok(self.sessions.add(MySession {
            arg: arg,
            sgate: sgate,
        }))
    }

    fn obtain(&mut self, sid: SessId, data: &mut service::ExchangeData) -> Result<(), Error> {
        if data.argcount != 0 || data.caps != 1 {
            Err(Error::InvArgs)
        }
        else {
            let sess = self.sessions.get(sid).unwrap();
            data.caps = cap::CapRngDesc::new_from(cap::Type::OBJECT, sess.sgate.sel(), 1).value();
            Ok(())
        }
    }

    fn close(&mut self, sid: SessId) {
        self.sessions.remove(sid);
    }
}

impl<'r> MyHandler<'r> {
    pub fn new() -> Result<Self, Error> {
        let mut rgate = RecvGate::new(util::next_log2(256), util::next_log2(256))?;
        rgate.activate()?;
        Ok(MyHandler {
            sessions: SessionContainer::new(),
            rgate: rgate,
        })
    }

    pub fn handle(&mut self) -> Result<(), Error> {
        if let Some(mut is) = self.rgate.fetch() {
            match is.pop() {
                Operation::REVERSE_STRING   => Self::reverse_string(is),
                _                           => reply_vmsg!(is, Error::InvArgs as u64),
            }
        }
        else {
            Ok(())
        }
    }

    fn reverse_string(mut is: GateIStream) -> Result<(), Error> {
        let s: String = is.pop();
        let mut res = String::new();

        for i in s.chars().rev() {
            res.push(i);
        }

        reply_vmsg!(is, res)?;

        // pretend that we're crashing after a few requests
        static mut COUNT: i32 = 0;
        unsafe {
            if COUNT >= 5 {
                return Err(Error::EndOfFile);
            }
            COUNT += 1;
        }

        Ok(())
    }
}

#[no_mangle]
pub fn main() -> i32 {
    let s = Server::new("test").expect("Unable to create service 'test'");

    let mut hdl = MyHandler::new().expect("Unable to create handler");

    let res = server_loop(|| {
        s.handle_ctrl_chan(&mut hdl)?;

        hdl.handle()
    });
    println!("Exited with {:?}", res);

    0
}
