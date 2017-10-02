use core::intrinsics;
use errors::Error;
use kif::syscalls;
use dtu;
use util;

// TODO move to appropriate place
pub type CapSel = u32;

const SEP: dtu::EpId = 0;
const REP: dtu::EpId = 1;

fn send_receive<T>(msg: T) -> Result<&'static dtu::Message, Error> {
    try!(dtu::DTU::send(SEP, msg, 0, REP));

    loop {
        // TODO sleep

        let msg = dtu::DTU::fetch_msg(REP);
        if let Some(m) = msg {
            return Ok(m)
        }
    }
}

fn send_receive_result<T>(msg: T) -> Result<(), Error> {
    let reply = try!(send_receive(msg));

    // TODO better way?
    let vals: &[u64] = unsafe { intrinsics::transmute(&reply.data) };
    let err = vals[0];
    dtu::DTU::mark_read(REP, &reply);

    match err {
        0 => Ok(()),
        e => Err(Error::from(e)),
    }
}

pub fn activate(vpe: CapSel, gate: CapSel, ep: dtu::EpId, addr: usize) -> Result<(), Error> {
    // LLOG(SYSC, "activate(vpe=" << vpe << ", gate=" << gate << ", ep=" << ep << ")");

    let req = syscalls::Activate {
        opcode: syscalls::Operation::Activate as u64,
        vpe_sel: vpe as u64,
        gate_sel: gate as u64,
        ep: ep as u64,
        addr: addr as u64,
    };
    send_receive_result(req)
}

pub fn create_sgate(dst: CapSel, rgate: CapSel, label: dtu::Label, credits: u64) -> Result<(), Error> {
    // TODO LLOG(SYSC, "createsgate(dst=" << dst << ", rgate=" << rgate << ", label=" << fmt(label, "#x")
    //     << ", credits=" << credits << ")");

    let req = syscalls::CreateSGate {
        opcode: syscalls::Operation::CreateSGate as u64,
        dst_sel: dst as u64,
        rgate_sel: rgate as u64,
        label: label,
        credits: credits,
    };
    send_receive_result(req)
}

pub fn create_mgate(dst: CapSel, addr: u64, size: usize, perms: u64) -> Result<(), Error> {
    // TODO LLOG(SYSC, "createmgate(dst=" << dst << ", addr=" << addr << ", size=" << size
    //      << ", perms=" << perms << ")");

    let req = syscalls::CreateMGate {
        opcode: syscalls::Operation::CreateMGate as u64,
        dst_sel: dst as u64,
        addr: addr,
        size: size as u64,
        perms: perms,
    };
    send_receive_result(req)
}

pub fn noop() -> Result<(), Error> {
    let req = syscalls::Noop {
        opcode: syscalls::Operation::Noop as u64,
    };
    send_receive_result(req)
}

pub fn exit(code: i32) {
    // TODO LLOG(SYSC, "exit(code=" << exitcode << ")");

    let req = syscalls::VPECtrl {
        opcode: syscalls::Operation::VpeCtrl as u64,
        vpe_sel: 0,
        op: syscalls::VPEOp::Stop as u64,
        arg: code as u64,
    };
    dtu::DTU::send(SEP, req, 0, REP).unwrap();
}
