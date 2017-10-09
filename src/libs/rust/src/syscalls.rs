use core::intrinsics;
use dtu;
use errors::Error;
use kif::syscalls;
use kif::cap;
use kif::Perm;
use util;

type CapSel = cap::CapSel;

fn send_receive<T>(msg: &[T]) -> Result<&'static dtu::Message, Error> {
    try!(dtu::DTU::send(dtu::SYSC_SEP, msg, 0, dtu::SYSC_REP));

    loop {
        try!(dtu::DTU::try_sleep(false, 0));

        let msg = dtu::DTU::fetch_msg(dtu::SYSC_REP);
        if let Some(m) = msg {
            return Ok(m)
        }
    }
}

fn send_receive_result<T>(msg: &[T]) -> Result<(), Error> {
    let reply = try!(send_receive(msg));

    // TODO better way?
    let vals: &[u64] = unsafe { intrinsics::transmute(&reply.data) };
    let err = vals[0] as u32;
    dtu::DTU::mark_read(dtu::SYSC_REP, &reply);

    match err {
        0 => Ok(()),
        e => Err(Error::from(e)),
    }
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
    send_receive_result(&[req])
}

pub fn create_sess(dst: CapSel, name: &str, arg: u64) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::create_sess(dst={}, name={}, arg={:#x})",
        dst, name, arg
    );

    let namelen = util::min(name.len(), syscalls::MAX_STR_SIZE);
    let mut req = syscalls::CreateSess {
        opcode: syscalls::Operation::CreateSess as u64,
        dst_sel: dst as u64,
        namelen: namelen as u64,
        name: unsafe { intrinsics::uninit() },
        arg: arg,
    };

    // copy name
    for (a, c) in req.name.iter_mut().zip(name.bytes()) {
        *a = c as u8;
    }

    send_receive_result(&[req])
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
    send_receive_result(&[req])
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
    send_receive_result(&[req])
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
    send_receive_result(&[req])
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
    send_receive_result(&[req])
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
    send_receive_result(&[req])
}

pub fn delegate(sess: CapSel, crd: cap::CapRngDesc, args: &mut [u64]) -> Result<(), Error> {
    log!(SYSC, "syscalls::delegate(sess={}, crd={})", sess, crd);

    exchange_sess(syscalls::Operation::Delegate, sess, crd, args)
}

pub fn obtain(sess: CapSel, crd: cap::CapRngDesc, args: &mut [u64]) -> Result<(), Error> {
    log!(SYSC, "syscalls::obtain(sess={}, crd={})", sess, crd);

    exchange_sess(syscalls::Operation::Obtain, sess, crd, args)
}

fn exchange_sess(op: syscalls::Operation, sess: CapSel, crd: cap::CapRngDesc,
                 args: &mut [u64]) -> Result<(), Error> {
    assert!(args.len() <= syscalls::MAX_EXCHG_ARGS);

    let mut req = syscalls::ExchangeSess {
        opcode: op as u64,
        sess_sel: sess as u64,
        crd: crd.value(),
        argcount: args.len() as u64,
        args: unsafe { intrinsics::uninit() },
    };

    for i in 0..args.len() {
        req.args[i] = args[i];
    }

    let msg = try!(send_receive(&[req]));
    // TODO better way?
    let data: &[syscalls::ExchangeSessReply] = unsafe { intrinsics::transmute(&msg.data) };
    let ref reply = data[0];

    let err = reply.error;
    if err == 0 {
        for i in 0..reply.argcount as usize {
            args[i] = reply.args[i];
        }
    }

    dtu::DTU::mark_read(dtu::SYSC_REP, &msg);
    match err {
        0   => Ok(()),
        e   => Err(Error::from(e as u32)),
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
    send_receive_result(&[req])
}

pub fn noop() -> Result<(), Error> {
    let req = syscalls::Noop {
        opcode: syscalls::Operation::Noop as u64,
    };
    send_receive_result(&[req])
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
    dtu::DTU::send(dtu::SYSC_SEP, &[req], 0, dtu::SYSC_REP).unwrap();
}
