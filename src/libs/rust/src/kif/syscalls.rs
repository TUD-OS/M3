/// The maximum message length that can be used
pub const MAX_MSG_SIZE: usize = 440;

/// The maximum size of strings in system calls
pub const MAX_STR_SIZE: usize = 32;

/// The maximum number of arguments for the exchange syscalls
pub const MAX_EXCHG_ARGS: usize = 8;

pub enum Operation {
    // sent by the DTU if the PF handler is not reachable
    Pagefault = 0,

    // capability creations
    CreateSrv,
    CreateSess,
    CreateSessAt,
    CreateRGate,
    CreateSGate,
    CreateMGate,
    CreateMap,
    CreateVPE,

    // capability operations
    Activate,
    VpeCtrl,
    DeriveMem,

    // capability exchange
    Delegate,
    Obtain,
    Exchange,
    Revoke,

    // forwarding
    ForwardMsg,
    ForwardMem,
    ForwardReply,

    // misc
    Noop,
}

#[repr(C, packed)]
pub struct DefaultReply {
    pub error: u64,
}

#[repr(C, packed)]
pub struct Pagefault {
    pub opcode: u64,
    pub virt: u64,
    pub access: u64,
}

#[repr(C, packed)]
pub struct CreateSrv {
    pub opcode: u64,
    pub dst_sel: u64,
    pub rgate_sel: u64,
    pub namelen: u64,
    pub name: [u8; MAX_STR_SIZE],
}

#[repr(C, packed)]
pub struct CreateSess {
    pub opcode: u64,
    pub dst_sel: u64,
    pub arg: u64,
    pub namelen: u64,
    pub name: [u8; MAX_STR_SIZE],
}

#[repr(C, packed)]
pub struct CreateSessAt {
    pub opcode: u64,
    pub dst_sel: u64,
    pub srv_sel: u64,
    pub ident: u64,
}

#[repr(C, packed)]
pub struct CreateRGate {
    pub opcode: u64,
    pub dst_sel: u64,
    pub order: u64,
    pub msgorder: u64,
}

#[repr(C, packed)]
pub struct CreateSGate {
    pub opcode: u64,
    pub dst_sel: u64,
    pub rgate_sel: u64,
    pub label: u64,
    pub credits: u64,
}

#[repr(C, packed)]
pub struct CreateMGate {
    pub opcode: u64,
    pub dst_sel: u64,
    pub addr: u64,
    pub size: u64,
    pub perms: u64,
}

#[repr(C, packed)]
pub struct CreateMap {
    pub opcode: u64,
    pub dst_sel: u64,
    pub vpe_sel: u64,
    pub mgate_sel: u64,
    pub first: u64,
    pub pages: u64,
    pub perms: u64,
}

#[repr(C, packed)]
pub struct CreateVPE {
    pub opcode: u64,
    pub dst_sel: u64,
    pub mgate_sel: u64,
    pub sgate_sel: u64,
    pub rgate_sel: u64,
    pub pe: u64,
    pub sep: u64,
    pub rep: u64,
    pub muxable: u64,
    pub namelen: u64,
    pub name: [u8; MAX_STR_SIZE],
}

#[repr(C, packed)]
pub struct CreateVPEReply {
    pub error: u64,
    pub pe: u64,
}

#[repr(C, packed)]
pub struct Activate {
    pub opcode: u64,
    pub vpe_sel: u64,
    pub gate_sel: u64,
    pub ep: u64,
    pub addr: u64,
}

pub enum VPEOp {
    Init,
    Start,
    Yield,
    Stop,
    Wait,
}

#[repr(C, packed)]
pub struct VPECtrl {
    pub opcode: u64,
    pub vpe_sel: u64,
    pub op: u64,
    pub arg: u64,
}

#[repr(C, packed)]
pub struct VPECtrlReply {
    pub error: u64,
    pub exitcode: u64,
}

#[repr(C, packed)]
pub struct DeriveMem {
    pub opcode: u64,
    pub dst_sel: u64,
    pub src_sel: u64,
    pub offset: u64,
    pub size: u64,
    pub perms: u64,
}

#[repr(C, packed)]
pub struct Exchange {
    pub opcode: u64,
    pub vpe_sel: u64,
    pub own_crd: u64,
    pub other_sel: u64,
    pub obtain: u64,
}

#[repr(C, packed)]
pub struct ExchangeSess {
    pub opcode: u64,
    pub sess_sel: u64,
    pub crd: u64,
    pub argcount: u64,
    pub args: [u64; MAX_EXCHG_ARGS],
}

#[repr(C, packed)]
pub struct ExchangeSessReply {
    pub error: u64,
    pub argcount: u64,
    pub args: [u64; MAX_EXCHG_ARGS],
}

#[repr(C, packed)]
pub struct Revoke {
    pub opcode: u64,
    pub vpe_sel: u64,
    pub crd: u64,
    pub own: u64,
}

#[repr(C, packed)]
pub struct ForwardMsg {
    pub opcode: u64,
    pub sgate_sel: u64,
    pub rgate_sel: u64,
    pub len: u64,
    pub rlabel: u64,
    pub event: u64,
    pub msg: [u8; MAX_MSG_SIZE],
}

pub enum ForwardMemFlags {
    NoPf    = 1,
    Write   = 2,
}

#[repr(C, packed)]
pub struct ForwardMem {
    pub opcode: u64,
    pub mgate_sel: u64,
    pub len: u64,
    pub offset: u64,
    pub flags: u64,
    pub event: u64,
    pub data: [u8; MAX_MSG_SIZE],
}

#[repr(C, packed)]
pub struct ForwardMemReply {
    pub error: u64,
    pub data: [u8; MAX_MSG_SIZE],
}

#[repr(C, packed)]
pub struct ForwardReply {
    pub opcode: u64,
    pub rgate_sel: u64,
    pub msgaddr: u64,
    pub len: u64,
    pub event: u64,
    pub msg: [u8; MAX_MSG_SIZE],
}

#[repr(C, packed)]
pub struct Noop {
    pub opcode: u64,
}
