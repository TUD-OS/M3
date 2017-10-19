use cap::{Capability, Flags, Selector};
use com::{GateIStream, RecvGate};
use server::SessId;
use errors::Error;
use syscalls;
use kif::service;
use util;
use vpe::VPE;

pub struct Server<'v> {
    cap: Capability,
    rgate: RecvGate<'v>,
}

pub trait Handler<S> {
    fn open(&mut self, arg: u64) -> Result<SessId, Error>;

    fn obtain(&mut self, _sid: SessId, _data: &mut service::ExchangeData) -> Result<(), Error> {
        Err(Error::NotSup)
    }
    fn delegate(&mut self, _sid: SessId, _data: &mut service::ExchangeData) -> Result<(), Error> {
        Err(Error::NotSup)
    }
    fn close(&mut self, _sid: SessId) {
    }
    fn shutdown(&mut self) {
    }
}

impl<'v> Server<'v> {
    pub fn new(name: &str) -> Result<Self, Error> {
        let sel = VPE::cur().alloc_cap();
        let mut rgate = try!(RecvGate::new(util::next_log2(256), util::next_log2(256)));
        try!(rgate.activate());

        try!(syscalls::create_srv(sel, rgate.sel(), name));

        Ok(Server {
            cap: Capability::new(sel, Flags::KEEP_CAP),
            rgate: rgate,
        })
    }

    pub fn sel(&self) -> Selector {
        self.cap.sel()
    }

    pub fn handle_ctrl_chan<S>(&self, hdl: &mut Handler<S>) -> Result<(), Error> {
        let is = self.rgate.fetch();
        if let Some(mut is) = is {
            let op: service::Operation = is.pop();
            match op {
                service::Operation::OPEN        => Self::handle_open(hdl, is),
                service::Operation::OBTAIN      => Self::handle_obtain(hdl, is),
                service::Operation::DELEGATE    => Self::handle_delegate(hdl, is),
                service::Operation::CLOSE       => Self::handle_close(hdl, is),
                service::Operation::SHUTDOWN    => Self::handle_shutdown(hdl, is),
                _                               => unreachable!(),
            }
        }
        else {
            Ok(())
        }
    }

    fn handle_open<S>(hdl: &mut Handler<S>, mut is: GateIStream) -> Result<(), Error> {
        let arg: u64 = is.pop();
        let res = hdl.open(arg);

        log!(SERV, "server::open({}) -> {:?}", arg, res);

        match res {
            Ok(sid) => {
                let reply = service::OpenReply { res: 0, sess: sid, };
                try!(is.reply(&[reply]))
            },
            Err(e) => {
                let reply = service::OpenReply { res: e as u64, sess: 0, };
                try!(is.reply(&[reply]))
            },
        };
        Ok(())
    }

    fn handle_obtain<S>(hdl: &mut Handler<S>, mut is: GateIStream) -> Result<(), Error> {
        let sid: u64 = is.pop();
        let mut data: service::ExchangeData = is.pop();
        let res = hdl.obtain(sid, &mut data);

        log!(SERV, "server::obtain({}, {:?}) -> {:?}", sid, data, res);

        let reply = service::ExchangeReply {
            res: if res.is_err() { res.unwrap_err() as u64 } else { 0 },
            data: data,
        };
        is.reply(&[reply])
    }

    fn handle_delegate<S>(hdl: &mut Handler<S>, mut is: GateIStream) -> Result<(), Error> {
        let sid: u64 = is.pop();
        let mut data: service::ExchangeData = is.pop();
        let res = hdl.delegate(sid, &mut data);

        log!(SERV, "server::delegate({}, {:?}) -> {:?}", sid, data, res);

        let reply = service::ExchangeReply {
            res: if res.is_err() { res.unwrap_err() as u64 } else { 0 },
            data: data,
        };
        is.reply(&[reply])
    }

    fn handle_close<S>(hdl: &mut Handler<S>, mut is: GateIStream) -> Result<(), Error> {
        let sid: u64 = is.pop();

        log!(SERV, "server::close({})", sid);

        hdl.close(sid);

        reply_vmsg!(is, 0)
    }

    fn handle_shutdown<S>(hdl: &mut Handler<S>, mut is: GateIStream) -> Result<(), Error> {
        log!(SERV, "server::shutdown()");

        hdl.shutdown();

        try!(reply_vmsg!(is, 0));
        Err(Error::EndOfFile)
    }
}
