use core::intrinsics;
use dtu;
use errors::Error;
use kif::{cap, syscalls, Perm, PEDesc};
use util;

type CapSel = cap::CapSel;

struct Reply<R: 'static> {
    msg: &'static dtu::Message,
    data: &'static R,
}

impl<R: 'static> Drop for Reply<R> {
    fn drop(&mut self) {
        dtu::DTU::mark_read(dtu::SYSC_REP, self.msg);
    }
}

fn send_receive<T, R>(msg: *const T) -> Result<Reply<R>, Error> {
    try!(dtu::DTU::send(dtu::SYSC_SEP, msg as *const u8, util::size_of::<T>(), 0, dtu::SYSC_REP));

    loop {
        try!(dtu::DTU::try_sleep(false, 0));

        let msg = dtu::DTU::fetch_msg(dtu::SYSC_REP);
        if let Some(m) = msg {
            let data: &[R] = unsafe { intrinsics::transmute(&m.data) };
            return Ok(Reply {
                msg: m,
                data: &data[0],
            })
        }
    }
}

fn send_receive_result<T>(msg: *const T) -> Result<(), Error> {
    let reply: Reply<syscalls::DefaultReply> = try!(send_receive(msg));

    match reply.data.error {
        0 => Ok(()),
        e => Err(Error::from(e as u32)),
    }
}

pub fn create_srv(dst: CapSel, rgate: CapSel, name: &str) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::create_srv(dst={}, rgate={}, name={})",
        dst, rgate, name
    );

    let mut req = syscalls::CreateSrv {
        opcode: syscalls::Operation::CreateSrv as u64,
        dst_sel: dst as u64,
        rgate_sel: rgate as u64,
        namelen: name.len() as u64,
        name: unsafe { intrinsics::uninit() },
    };

    // copy name
    for (a, c) in req.name.iter_mut().zip(name.bytes()) {
        *a = c as u8;
    }

    send_receive_result(&req)
}

pub fn activate(vpe: CapSel, gate: CapSel, ep: dtu::EpId, addr: usize) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::activate(vpe={}, gate={}, ep={}, addr={})",
        vpe, gate, ep, addr
    );

    let req = syscalls::Activate {
        opcode: syscalls::Operation::Activate as u64,
        vpe_sel: vpe as u64,
        gate_sel: gate as u64,
        ep: ep as u64,
        addr: addr as u64,
    };
    send_receive_result(&req)
}

pub fn create_sess(dst: CapSel, name: &str, arg: u64) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::create_sess(dst={}, name={}, arg={:#x})",
        dst, name, arg
    );

    let mut req = syscalls::CreateSess {
        opcode: syscalls::Operation::CreateSess as u64,
        dst_sel: dst as u64,
        namelen: name.len() as u64,
        name: unsafe { intrinsics::uninit() },
        arg: arg,
    };

    // copy name
    for (a, c) in req.name.iter_mut().zip(name.bytes()) {
        *a = c as u8;
    }

    send_receive_result(&req)
}

pub fn create_sgate(dst: CapSel, rgate: CapSel, label: dtu::Label, credits: u64) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::create_sgate(dst={}, rgate={}, lbl={:#x}, credits={})",
        dst, rgate, label, credits
    );

    let req = syscalls::CreateSGate {
        opcode: syscalls::Operation::CreateSGate as u64,
        dst_sel: dst as u64,
        rgate_sel: rgate as u64,
        label: label,
        credits: credits,
    };
    send_receive_result(&req)
}

pub fn create_mgate(dst: CapSel, addr: usize, size: usize, perms: Perm) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::create_mgate(dst={}, addr={:#x}, size={:#x}, perms={:?})",
        dst, addr, size, perms
    );

    let req = syscalls::CreateMGate {
        opcode: syscalls::Operation::CreateMGate as u64,
        dst_sel: dst as u64,
        addr: addr as u64,
        size: size as u64,
        perms: perms.bits() as u64,
    };
    send_receive_result(&req)
}

pub fn create_rgate(dst: CapSel, order: i32, msgorder: i32) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::create_rgate(dst={}, order={}, msgorder={})",
        dst, order, msgorder
    );

    let req = syscalls::CreateRGate {
        opcode: syscalls::Operation::CreateRGate as u64,
        dst_sel: dst as u64,
        order: order as u64,
        msgorder: msgorder as u64,
    };
    send_receive_result(&req)
}

pub fn create_vpe(dst: CapSel, mgate: CapSel, sgate: CapSel, rgate: CapSel, name: &str,
                  pe: PEDesc, sep: dtu::EpId, rep: dtu::EpId, tmuxable: bool) -> Result<PEDesc, Error> {
    log!(
        SYSC,
        "syscalls::create_vpe(dst={}, mgate={}, sgate={}, rgate={}, name={}, pe={:?}, sep={}, rep={}, tmuxable={})",
        dst, mgate, sgate, rgate, name, pe, sep, rep, tmuxable
    );

    let mut req = syscalls::CreateVPE {
        opcode: syscalls::Operation::CreateVPE as u64,
        dst_sel: dst as u64,
        mgate_sel: mgate as u64,
        sgate_sel: sgate as u64,
        rgate_sel: rgate as u64,
        pe: pe.value() as u64,
        sep: sep as u64,
        rep: rep as u64,
        muxable: tmuxable as u64,
        namelen: name.len() as u64,
        name: unsafe { intrinsics::uninit() },
    };

    // copy name
    for (a, c) in req.name.iter_mut().zip(name.bytes()) {
        *a = c as u8;
    }

    let reply: Reply<syscalls::CreateVPEReply> = try!(send_receive(&req));
    match reply.data.error {
        0 => Ok(PEDesc::new_from_val(reply.data.pe as u32)),
        e => Err(Error::from(e as u32))
    }
}

pub fn derive_mem(dst: CapSel, src: CapSel, offset: usize, size: usize, perms: Perm) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::derive_mem(dst={}, src={}, off={:#x}, size={:#x}, perms={:?})",
        dst, src, offset, size, perms
    );

    let req = syscalls::DeriveMem {
        opcode: syscalls::Operation::DeriveMem as u64,
        dst_sel: dst as u64,
        src_sel: src as u64,
        offset: offset as u64,
        size: size as u64,
        perms: perms.bits() as u64,
    };
    send_receive_result(&req)
}

pub fn vpe_ctrl(vpe: CapSel, op: syscalls::VPEOp, arg: u64) -> Result<i32, Error> {
    log!(
        SYSC,
        "syscalls::vpe_ctrl(vpe={}, op={:?}, arg={})",
        vpe, op, arg
    );

    let req = syscalls::VPECtrl {
        opcode: syscalls::Operation::VpeCtrl as u64,
        vpe_sel: vpe as u64,
        op: op as u64,
        arg: arg as u64,
    };

    let reply: Reply<syscalls::VPECtrlReply> = try!(send_receive(&req));
    match reply.data.error {
        0 => Ok(reply.data.exitcode as i32),
        e => Err(Error::from(e as u32))
    }
}

pub fn exchange(vpe: CapSel, own: cap::CapRngDesc, other: CapSel, obtain: bool) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::exchange(vpe={}, own={}, other={}, obtain={})",
        vpe, own, other, obtain
    );

    let req = syscalls::Exchange {
        opcode: syscalls::Operation::Exchange as u64,
        vpe_sel: vpe as u64,
        own_crd: own.value(),
        other_sel: other as u64,
        obtain: obtain as u64,
    };
    send_receive_result(&req)
}

pub fn delegate(sess: CapSel, crd: cap::CapRngDesc, sargs: &[u64], rargs: &mut [u64]) -> Result<usize, Error> {
    log!(SYSC, "syscalls::delegate(sess={}, crd={})", sess, crd);

    exchange_sess(syscalls::Operation::Delegate, sess, crd, sargs, rargs)
}

pub fn obtain(sess: CapSel, crd: cap::CapRngDesc, sargs: &[u64], rargs: &mut [u64]) -> Result<usize, Error> {
    log!(SYSC, "syscalls::obtain(sess={}, crd={})", sess, crd);

    exchange_sess(syscalls::Operation::Obtain, sess, crd, sargs, rargs)
}

fn exchange_sess(op: syscalls::Operation, sess: CapSel, crd: cap::CapRngDesc,
                 sargs: &[u64], rargs: &mut [u64]) -> Result<usize, Error> {
    assert!(sargs.len() <= syscalls::MAX_EXCHG_ARGS);
    assert!(rargs.len() <= syscalls::MAX_EXCHG_ARGS);

    let mut req = syscalls::ExchangeSess {
        opcode: op as u64,
        sess_sel: sess as u64,
        crd: crd.value(),
        argcount: sargs.len() as u64,
        args: unsafe { intrinsics::uninit() },
    };

    for i in 0..sargs.len() {
        req.args[i] = sargs[i];
    }

    let reply: Reply<syscalls::ExchangeSessReply> = try!(send_receive(&req));
    if reply.data.error == 0 {
        for i in 0..reply.data.argcount as usize {
            rargs[i] = reply.data.args[i];
        }
    }

    match reply.data.error {
        0 => Ok(reply.data.argcount as usize),
        e => Err(Error::from(e as u32))
    }
}

pub fn revoke(vpe: CapSel, crd: cap::CapRngDesc, own: bool) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::revoke(vpe={}, crd={}, own={})",
        vpe, crd, own
    );

    let req = syscalls::Revoke {
        opcode: syscalls::Operation::Revoke as u64,
        vpe_sel: vpe as u64,
        crd: crd.value(),
        own: own as u64,
    };
    send_receive_result(&req)
}

pub fn forward_write(mgate: CapSel, data: &[u8], off: usize,
                     flags: syscalls::ForwardMemFlags, event: u64) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::forward_write(mgate={}, count={}, off={}, flags={}, event={})",
        mgate, data.len(), off, flags.bits(), event
    );

    let mut req = syscalls::ForwardMem {
        opcode: syscalls::Operation::ForwardMem as u64,
        mgate_sel: mgate as u64,
        offset: off as u64,
        flags: (flags | syscalls::ForwardMemFlags::WRITE).bits() as u64,
        event: event as u64,
        len: data.len() as u64,
        data: unsafe { intrinsics::uninit() },
    };
    req.data[0..data.len()].copy_from_slice(data);

    send_receive_result(&req)
}

pub fn forward_read(mgate: CapSel, data: &mut [u8], off: usize,
                    flags: syscalls::ForwardMemFlags, event: u64) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::forward_read(mgate={}, count={}, off={}, flags={}, event={})",
        mgate, data.len(), off, flags.bits(), event
    );

    let req = syscalls::ForwardMem {
        opcode: syscalls::Operation::ForwardMem as u64,
        mgate_sel: mgate as u64,
        offset: off as u64,
        flags: flags.bits() as u64,
        event: event as u64,
        len: data.len() as u64,
        data: unsafe { intrinsics::uninit() },
    };

    let reply: Reply<syscalls::ForwardMemReply> = try!(send_receive(&req));
    if reply.data.error == 0 {
        let len = data.len();
        data.copy_from_slice(&reply.data.data[0..len]);
    }

    match reply.data.error {
        0 => Ok(()),
        e => Err(Error::from(e as u32))
    }
}

pub fn noop() -> Result<(), Error> {
    let req = syscalls::Noop {
        opcode: syscalls::Operation::Noop as u64,
    };
    send_receive_result(&req)
}

pub fn exit(code: i32) {
    log!(
        SYSC,
        "syscalls::exit(code={})",
        code
    );

    let req = syscalls::VPECtrl {
        opcode: syscalls::Operation::VpeCtrl as u64,
        vpe_sel: 0,
        op: syscalls::VPEOp::Stop as u64,
        arg: code as u64,
    };
    let msg = &req as *const syscalls::VPECtrl as *const u8;
    dtu::DTU::send(dtu::SYSC_SEP, msg, util::size_of_val(&req), 0, dtu::SYSC_REP).unwrap();
}
