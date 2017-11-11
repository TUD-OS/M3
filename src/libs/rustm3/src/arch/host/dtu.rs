use arch::{env, rbufs};
use com::RecvGate;
use dtu;
use kif;
use syscalls;
use vpe;

pub fn init() {
    {
        let (ep, lbl, crd) = env::get().syscall_params();
        dtu::DTU::configure(dtu::SYSC_SEP, lbl, 0, ep, crd, rbufs::SYSC_RBUF_ORD);
    }

    let sysc = RecvGate::syscall();
    dtu::DTU::configure_recv(dtu::SYSC_REP, sysc.buffer(), rbufs::SYSC_RBUF_ORD, rbufs::SYSC_RBUF_ORD);

    let upc = RecvGate::upcall();
    dtu::DTU::configure_recv(dtu::UPCALL_REP, upc.buffer(), rbufs::UPCALL_RBUF_ORD, rbufs::UPCALL_RBUF_ORD);

    let def = RecvGate::def();
    dtu::DTU::configure_recv(dtu::DEF_REP, def.buffer(), rbufs::DEF_RBUF_ORD, rbufs::DEF_RBUF_ORD);

    dtu::init();

    let eps = dtu::ep_regs_addr();
    syscalls::vpe_ctrl(vpe::VPE::cur().sel(), kif::syscalls::VPEOp::Init, eps as u64).unwrap();
}

pub fn deinit() {
    dtu::deinit();
}
