use cap::Selector;
use com::*;
use errors::Error;
use session::Session;

mod meta_op {
    pub const ATTACH: u8    = 0x0;
    pub const CLOSE: u8     = 0x1;
}

pub struct Pipe {
    sess: Session,
    meta_gate: SendGate,
    rd_gate: SendGate,
    wr_gate: SendGate,
}

impl Pipe {
    pub fn new(name: &str, mem_size: usize) -> Result<Self, Error> {
        let sess = try!(Session::new(name, mem_size as u64));
        let rgate = RecvGate::def();
        let meta = SendGate::new_bind(try!(sess.obtain(1, &mut [])).start(), rgate);
        let read = SendGate::new_bind(try!(sess.obtain(1, &mut [])).start(), rgate);
        let write = SendGate::new_bind(try!(sess.obtain(1, &mut [])).start(), rgate);
        Ok(Pipe {
            sess: sess,
            meta_gate: meta,
            rd_gate: read,
            wr_gate: write,
        })
    }

    pub fn new_bind(sess: Selector, meta_gate: Selector, rd_gate: Selector, wr_gate: Selector) -> Self {
        let rgate = RecvGate::def();
        Pipe {
            sess: Session::new_bind(sess),
            meta_gate: SendGate::new_bind(meta_gate, rgate),
            rd_gate: SendGate::new_bind(rd_gate, rgate),
            wr_gate: SendGate::new_bind(wr_gate, rgate),
        }
    }

    pub fn sel(&self) -> Selector {
        self.sess.sel()
    }

    pub fn attach(&mut self, reading: bool) -> Result<(), Error> {
        try!(send_vmsg!(&mut self.meta_gate, meta_op::ATTACH, reading as u8));
        recv_res(RecvGate::def()).map(|_| ())
    }

    pub fn request_read(&mut self, amount: usize) -> Result<(usize, usize), Error> {
        try!(send_vmsg!(&mut self.rd_gate, amount));
        let mut reply = try!(recv_res(RecvGate::def()));
        Ok((reply.pop(), reply.pop()))
    }

    pub fn request_write(&mut self, amount: usize, last_write: usize) -> Result<usize, Error> {
        try!(send_vmsg!(&mut self.wr_gate, amount, last_write));
        let mut reply = try!(recv_res(RecvGate::def()));
        Ok(reply.pop())
    }

    pub fn close(&mut self, reading: bool, last_write: usize) -> Result<(), Error> {
        try!(send_vmsg!(&mut self.meta_gate, meta_op::CLOSE, reading as u8, last_write));
        recv_res(RecvGate::def()).map(|_| ())
    }
}
