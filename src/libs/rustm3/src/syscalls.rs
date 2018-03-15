use cap::Selector;
use core::intrinsics;
use dtu;
use errors::Error;
use kif::{CapRngDesc, syscalls, Perm, PEDesc};
use util;

struct Reply<R: 'static> {
    msg: &'static dtu::Message,
    data: &'static R,
}

impl<R: 'static> Drop for Reply<R> {
    fn drop(&mut self) {
        dtu::DTU::mark_read(dtu::SYSC_REP, self.msg);
    }
}

fn send<T>(msg: *const T) -> Result<(), Error> {
    dtu::DTU::send(dtu::SYSC_SEP, msg as *const u8, util::size_of::<T>(), 0, dtu::SYSC_REP)
}

fn send_receive<T, R>(msg: *const T) -> Result<Reply<R>, Error> {
    send(msg)?;

    loop {
        dtu::DTU::try_sleep(false, 0)?;

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
    let reply: Reply<syscalls::DefaultReply> = send_receive(msg)?;

    match reply.data.error {
        0 => Ok(()),
        e => Err(Error::from(e as u32)),
    }
}

pub fn create_srv(dst: Selector, rgate: Selector, name: &str) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::create_srv(dst={}, rgate={}, name={})",
        dst, rgate, name
    );

    let mut req = syscalls::CreateSrv {
        opcode: syscalls::Operation::CREATE_SRV.val,
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

pub fn activate(ep: Selector, gate: Selector, addr: usize) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::activate(ep={}, gate={}, addr={})",
        ep, gate, addr
    );

    let req = syscalls::Activate {
        opcode: syscalls::Operation::ACTIVATE.val,
        ep_sel: ep as u64,
        gate_sel: gate as u64,
        addr: addr as u64,
    };
    send_receive_result(&req)
}

pub fn create_sess(dst: Selector, name: &str, arg: u64) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::create_sess(dst={}, name={}, arg={:#x})",
        dst, name, arg
    );

    let mut req = syscalls::CreateSess {
        opcode: syscalls::Operation::CREATE_SESS.val,
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

pub fn create_sgate(dst: Selector, rgate: Selector, label: dtu::Label, credits: u64) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::create_sgate(dst={}, rgate={}, lbl={:#x}, credits={})",
        dst, rgate, label, credits
    );

    let req = syscalls::CreateSGate {
        opcode: syscalls::Operation::CREATE_SGATE.val,
        dst_sel: dst as u64,
        rgate_sel: rgate as u64,
        label: label,
        credits: credits,
    };
    send_receive_result(&req)
}

pub fn create_mgate(dst: Selector, addr: usize, size: usize, perms: Perm) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::create_mgate(dst={}, addr={:#x}, size={:#x}, perms={:?})",
        dst, addr, size, perms
    );

    let req = syscalls::CreateMGate {
        opcode: syscalls::Operation::CREATE_MGATE.val,
        dst_sel: dst as u64,
        addr: addr as u64,
        size: size as u64,
        perms: perms.bits() as u64,
    };
    send_receive_result(&req)
}

pub fn create_rgate(dst: Selector, order: i32, msgorder: i32) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::create_rgate(dst={}, order={}, msgorder={})",
        dst, order, msgorder
    );

    let req = syscalls::CreateRGate {
        opcode: syscalls::Operation::CREATE_RGATE.val,
        dst_sel: dst as u64,
        order: order as u64,
        msgorder: msgorder as u64,
    };
    send_receive_result(&req)
}

pub fn create_map(dst: Selector, vpe: Selector, mgate: Selector, first: Selector,
                  pages: u32, perms: Perm) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::create_map(dst={}, vpe={}, mgate={}, first={}, pages={}, perms={:?})",
        dst, vpe, mgate, first, pages, perms
    );

    let req = syscalls::CreateMap {
        opcode: syscalls::Operation::CREATE_MAP.val,
        dst_sel: dst as u64,
        vpe_sel: vpe as u64,
        mgate_sel: mgate as u64,
        first: first as u64,
        pages: pages as u64,
        perms: perms.bits() as u64,
    };
    send_receive_result(&req)
}

pub fn create_vpe(dst: CapRngDesc, sgate: Selector, name: &str, pe: PEDesc,
                  sep: dtu::EpId, rep: dtu::EpId, tmuxable: bool) -> Result<PEDesc, Error> {
    log!(
        SYSC,
        "syscalls::create_vpe(dst={}, sgate={}, name={}, pe={:?}, sep={}, rep={}, tmuxable={})",
        dst, sgate, name, pe, sep, rep, tmuxable
    );

    let mut req = syscalls::CreateVPE {
        opcode: syscalls::Operation::CREATE_VPE.val,
        dst_crd: dst.value() as u64,
        sgate_sel: sgate as u64,
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

    let reply: Reply<syscalls::CreateVPEReply> = send_receive(&req)?;
    match reply.data.error {
        0 => Ok(PEDesc::new_from(reply.data.pe as u32)),
        e => Err(Error::from(e as u32))
    }
}

pub fn derive_mem(dst: Selector, src: Selector, offset: usize, size: usize, perms: Perm) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::derive_mem(dst={}, src={}, off={:#x}, size={:#x}, perms={:?})",
        dst, src, offset, size, perms
    );

    let req = syscalls::DeriveMem {
        opcode: syscalls::Operation::DERIVE_MEM.val,
        dst_sel: dst as u64,
        src_sel: src as u64,
        offset: offset as u64,
        size: size as u64,
        perms: perms.bits() as u64,
    };
    send_receive_result(&req)
}

pub fn vpe_ctrl(vpe: Selector, op: syscalls::VPEOp, arg: u64) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::vpe_ctrl(vpe={}, op={:?}, arg={})",
        vpe, op, arg
    );

    let req = syscalls::VPECtrl {
        opcode: syscalls::Operation::VPE_CTRL.val,
        vpe_sel: vpe as u64,
        op: op.val,
        arg: arg as u64,
    };
    send_receive_result(&req)
}

pub fn vpe_wait(vpes: &[Selector]) -> Result<(Selector, i32), Error> {
    log!(
        SYSC,
        "syscalls::vpe_wait(vpes={})",
        vpes.len()
    );

    let mut req = syscalls::VPEWait {
        opcode: syscalls::Operation::VPE_WAIT.val,
        vpe_count: vpes.len() as u64,
        sels: unsafe { intrinsics::uninit() },
    };
    for i in 0..vpes.len() {
        req.sels[i] = vpes[i] as u64;
    }

    let reply: Reply<syscalls::VPEWaitReply> = send_receive(&req)?;
    match reply.data.error {
        0 => Ok((reply.data.vpe_sel as Selector, reply.data.exitcode as i32)),
        e => Err(Error::from(e as u32))
    }
}

pub fn exchange(vpe: Selector, own: CapRngDesc, other: Selector, obtain: bool) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::exchange(vpe={}, own={}, other={}, obtain={})",
        vpe, own, other, obtain
    );

    let req = syscalls::Exchange {
        opcode: syscalls::Operation::EXCHANGE.val,
        vpe_sel: vpe as u64,
        own_crd: own.value(),
        other_sel: other as u64,
        obtain: obtain as u64,
    };
    send_receive_result(&req)
}

pub fn delegate(vpe: Selector, sess: Selector, crd: CapRngDesc,
                args: &mut syscalls::ExchangeArgs) -> Result<(), Error> {
    log!(SYSC, "syscalls::delegate(vpe={}, sess={}, crd={})", vpe, sess, crd);

    exchange_sess(vpe, syscalls::Operation::DELEGATE, sess, crd, args)
}

pub fn obtain(vpe: Selector, sess: Selector, crd: CapRngDesc,
              args: &mut syscalls::ExchangeArgs) -> Result<(), Error> {
    log!(SYSC, "syscalls::obtain(vpe={}, sess={}, crd={})", vpe, sess, crd);

    exchange_sess(vpe, syscalls::Operation::OBTAIN, sess, crd, args)
}

fn exchange_sess(vpe: Selector, op: syscalls::Operation, sess: Selector, crd: CapRngDesc,
                 args: &mut syscalls::ExchangeArgs) -> Result<(), Error> {
    let req = syscalls::ExchangeSess {
        opcode: op.val,
        vpe_sel: vpe as u64,
        sess_sel: sess as u64,
        crd: crd.value(),
        args: args.clone(),
    };

    let reply: Reply<syscalls::ExchangeSessReply> = send_receive(&req)?;
    if reply.data.error == 0 {
        *args = reply.data.args;
    }

    match reply.data.error {
        0 => Ok(()),
        e => Err(Error::from(e as u32))
    }
}

pub fn revoke(vpe: Selector, crd: CapRngDesc, own: bool) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::revoke(vpe={}, crd={}, own={})",
        vpe, crd, own
    );

    let req = syscalls::Revoke {
        opcode: syscalls::Operation::REVOKE.val,
        vpe_sel: vpe as u64,
        crd: crd.value(),
        own: own as u64,
    };
    send_receive_result(&req)
}

pub fn forward_write(mgate: Selector, data: &[u8], off: usize,
                     flags: syscalls::ForwardMemFlags, event: u64) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::forward_write(mgate={}, count={}, off={}, flags={}, event={})",
        mgate, data.len(), off, flags.bits(), event
    );

    let mut req = syscalls::ForwardMem {
        opcode: syscalls::Operation::FORWARD_MEM.val,
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

pub fn forward_read(mgate: Selector, data: &mut [u8], off: usize,
                    flags: syscalls::ForwardMemFlags, event: u64) -> Result<(), Error> {
    log!(
        SYSC,
        "syscalls::forward_read(mgate={}, count={}, off={}, flags={}, event={})",
        mgate, data.len(), off, flags.bits(), event
    );

    let req = syscalls::ForwardMem {
        opcode: syscalls::Operation::FORWARD_MEM.val,
        mgate_sel: mgate as u64,
        offset: off as u64,
        flags: flags.bits() as u64,
        event: event as u64,
        len: data.len() as u64,
        data: unsafe { intrinsics::uninit() },
    };

    let reply: Reply<syscalls::ForwardMemReply> = send_receive(&req)?;
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
        opcode: syscalls::Operation::NOOP.val,
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
        opcode: syscalls::Operation::VPE_CTRL.val,
        vpe_sel: 0,
        op: syscalls::VPEOp::STOP.val,
        arg: code as u64,
    };
    send(&req).unwrap();
}
