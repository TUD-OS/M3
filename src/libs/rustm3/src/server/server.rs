/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
 *
 * M3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * M3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

use cap::{CapFlags, Capability, Selector};
use com::{GateIStream, RecvGate};
use errors::{Code, Error};
use kif::service;
use server::SessId;
use syscalls;
use util;
use vpe::VPE;

pub struct Server {
    cap: Capability,
    rgate: RecvGate,
}

pub trait Handler {
    fn open(&mut self, srv_sel: Selector, arg: u64) -> Result<Selector, Error>;

    fn obtain(&mut self, _sid: SessId, _data: &mut service::ExchangeData) -> Result<(), Error> {
        Err(Error::new(Code::NotSup))
    }
    fn delegate(&mut self, _sid: SessId, _data: &mut service::ExchangeData) -> Result<(), Error> {
        Err(Error::new(Code::NotSup))
    }
    fn close(&mut self, _sid: SessId) {
    }
    fn shutdown(&mut self) {
    }
}

impl Server {
    pub fn new(name: &str) -> Result<Self, Error> {
        let sel = VPE::cur().alloc_sel();
        let mut rgate = RecvGate::new(util::next_log2(256), util::next_log2(256))?;
        rgate.activate()?;

        syscalls::create_srv(sel, VPE::cur().sel(), rgate.sel(), name)?;

        Ok(Server {
            cap: Capability::new(sel, CapFlags::KEEP_CAP),
            rgate: rgate,
        })
    }

    pub fn sel(&self) -> Selector {
        self.cap.sel()
    }

    pub fn handle_ctrl_chan(&self, hdl: &mut Handler) -> Result<(), Error> {
        let is = self.rgate.fetch();
        if let Some(mut is) = is {
            let op: service::Operation = is.pop();
            match op {
                service::Operation::OPEN        => Self::handle_open(hdl, self.sel(), is),
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

    fn handle_open(hdl: &mut Handler, sel: Selector, mut is: GateIStream) -> Result<(), Error> {
        let arg: u64 = is.pop();
        let res = hdl.open(sel, arg);

        log!(SERV, "server::open({}) -> {:?}", arg, res);

        match res {
            Ok(sel) => {
                let reply = service::OpenReply { res: 0, sess: sel as u64, };
                is.reply(&[reply])?
            },
            Err(e) => {
                let reply = service::OpenReply { res: e.code() as u64, sess: 0, };
                is.reply(&[reply])?
            },
        };
        Ok(())
    }

    fn handle_obtain(hdl: &mut Handler, mut is: GateIStream) -> Result<(), Error> {
        let sid: u64 = is.pop();
        let mut data: service::ExchangeData = is.pop();
        let res = hdl.obtain(sid, &mut data);

        log!(SERV, "server::obtain({}, {:?}) -> {:?}", sid, data, res);

        let reply = service::ExchangeReply {
            res: match res {
                Ok(_)   => 0,
                Err(e)  => e.code() as u64,
            },
            data: data,
        };
        is.reply(&[reply])
    }

    fn handle_delegate(hdl: &mut Handler, mut is: GateIStream) -> Result<(), Error> {
        let sid: u64 = is.pop();
        let mut data: service::ExchangeData = is.pop();
        let res = hdl.delegate(sid, &mut data);

        log!(SERV, "server::delegate({}, {:?}) -> {:?}", sid, data, res);

        let reply = service::ExchangeReply {
            res: match res {
                Ok(_)   => 0,
                Err(e)  => e.code() as u64,
            },
            data: data,
        };
        is.reply(&[reply])
    }

    fn handle_close(hdl: &mut Handler, mut is: GateIStream) -> Result<(), Error> {
        let sid: u64 = is.pop();

        log!(SERV, "server::close({})", sid);

        hdl.close(sid);

        reply_vmsg!(is, 0)
    }

    fn handle_shutdown(hdl: &mut Handler, mut is: GateIStream) -> Result<(), Error> {
        log!(SERV, "server::shutdown()");

        hdl.shutdown();

        reply_vmsg!(is, 0)?;
        Err(Error::new(Code::EndOfFile))
    }
}
