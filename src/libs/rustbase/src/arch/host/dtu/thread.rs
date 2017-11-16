use arch::dtu::*;
use errors::{Code, Error};
use kif;
use io;

pub(crate) struct Buffer {
    pub header: Header,
    pub data: [u8; MAX_MSG_SIZE],
}

impl Buffer {
    const fn new() -> Buffer {
        Buffer {
            header: Header::new(),
            data: [0u8; MAX_MSG_SIZE],
        }
    }

    fn as_words(&self) -> &[u64] {
        unsafe {
            util::slice_for(self.data.as_ptr() as *const u64, MAX_MSG_SIZE / util::size_of::<u64>())
        }
    }
    fn as_words_mut(&mut self) -> &mut [u64] {
        unsafe {
            util::slice_for_mut(self.data.as_mut_ptr() as *mut u64, MAX_MSG_SIZE / util::size_of::<u64>())
        }
    }
}

static mut LOG: Option<io::log::Log> = None;
static mut BUFFER: Buffer = Buffer::new();

fn log() -> &'static mut io::log::Log {
    unsafe { LOG.as_mut().unwrap() }
}

fn buffer() -> &'static mut Buffer {
    unsafe { &mut BUFFER }
}

macro_rules! log_impl {
    ($($args:tt)*) => ({
        if $crate::io::log::DTU {
            #[allow(unused_imports)]
            use $crate::io::Write;
            $crate::arch::dtu::thread::log().write_fmt(format_args!($($args)*)).unwrap();
        }
    })
}

macro_rules! log_dtu {
    ($fmt:expr)              => (log_impl!(concat!($fmt, "\n")));
    ($fmt:expr, $($arg:tt)*) => (log_impl!(concat!($fmt, "\n"), $($arg)*));
}

fn is_bit_set(mask: Reg, idx: u64) -> bool {
    (mask & (1 << idx)) != 0
}

fn set_bit(mask: Reg, idx: u64, val: bool) -> Reg {
    if val {
        mask | (1 << idx)
    }
    else {
        mask & !(1 << idx)
    }
}

fn prepare_send(ep: EpId) -> Result<(PEId, EpId), Error> {
    let msg = DTU::get_cmd(CmdReg::ADDR);
    let msg_size = DTU::get_cmd(CmdReg::SIZE) as usize;
    let credits = DTU::get_ep(ep, EpReg::CREDITS) as usize;

    // check if we have enough credits
    if credits != !0 {
        let needed = 1 << DTU::get_ep(ep, EpReg::MSGORDER);
        if needed > credits {
            log_dtu!("DTU-error: insufficient credits on ep {} (have {:#x}, need {:#x})",
                ep, credits, needed);
            return Err(Error::new(Code::MissCredits));
        }

        DTU::set_ep(ep, EpReg::CREDITS, (credits - needed) as Reg);
    }

    let buf = buffer();
    buf.header.credits = 0;
    buf.header.label = DTU::get_ep(ep, EpReg::LABEL);

    // message
    buf.header.length = msg_size;
    unsafe {
        &buf.data[0..msg_size].copy_from_slice(util::slice_for(msg as *const u8, msg_size));
    }

    Ok((DTU::get_ep(ep, EpReg::PE_ID) as PEId, DTU::get_ep(ep, EpReg::EP_ID) as EpId))
}

fn prepare_reply(ep: EpId) -> Result<(PEId, EpId), Error> {
    let src = DTU::get_cmd(CmdReg::ADDR);
    let size = DTU::get_cmd(CmdReg::SIZE) as usize;
    let reply = DTU::get_cmd(CmdReg::OFFSET) as usize;
    let buf_addr = DTU::get_ep(ep, EpReg::BUF_ADDR) as usize;
    let ord = DTU::get_ep(ep, EpReg::BUF_ORDER);
    let msg_ord = DTU::get_ep(ep, EpReg::BUF_MSGORDER);

    let idx = (reply - buf_addr) >> msg_ord;
    if idx >= (1 << (ord - msg_ord)) {
        log_dtu!("DTU-error: EP{}: invalid message addr {:#x}", ep, reply);
        return Err(Error::new(Code::InvArgs));
    }

    let reply_header: &Header = unsafe { intrinsics::transmute(reply) };
    if reply_header.has_replycap == 0 {
        log_dtu!("DTU-error: EP{}: double-reply for msg {:#x}?", ep, reply);
        return Err(Error::new(Code::InvArgs));
    }

    let buf = buffer();
    buf.header.label = reply_header.reply_label;
    buf.header.credits = 1;
    buf.header.crd_ep = reply_header.snd_ep;
    // invalidate message for replying
    buf.header.has_replycap = 0;

    // message
    buf.header.length = size;
    unsafe {
        &buf.data[0..size].copy_from_slice(util::slice_for(src as *const u8, size));
    }

    Ok((reply_header.pe as PEId, reply_header.rpl_ep as EpId))
}

fn check_rdwr(ep: EpId, read: bool) -> Result<(), Error> {
    let op = if read { 0 } else { 1 };
    let label = DTU::get_ep(ep, EpReg::LABEL);
    let credits = DTU::get_ep(ep, EpReg::CREDITS);
    let offset = DTU::get_cmd(CmdReg::OFFSET);
    let length = DTU::get_cmd(CmdReg::LENGTH);

    let perms = label & (kif::Perm::RWX.bits() as Label);
    if (perms & (1 << op)) == 0 {
        log_dtu!("DTU-error: EP{}: operation not permitted (perms={}, op={})", ep, perms, op);
        Err(Error::new(Code::InvEP))
    }
    else if offset >= credits || offset + length < offset || offset + length > credits {
        log_dtu!(
            "DTU-error: EP{}: invalid parameters (credits={}, offset={}, datalen={})",
            ep, credits, offset, length
        );
        Err(Error::new(Code::InvEP))
    }
    else {
        Ok(())
    }
}

fn prepare_read(ep: EpId) -> Result<(PEId, EpId), Error> {
    check_rdwr(ep, true)?;

    let buf = buffer();

    buf.header.credits = 0;
    buf.header.label = DTU::get_ep(ep, EpReg::LABEL);
    buf.header.length = 3 * util::size_of::<u64>();

    let data = buf.as_words_mut();
    data[0] = DTU::get_cmd(CmdReg::OFFSET);
    data[1] = DTU::get_cmd(CmdReg::LENGTH);
    data[2] = DTU::get_cmd(CmdReg::ADDR);

    Ok((DTU::get_ep(ep, EpReg::PE_ID) as PEId, DTU::get_ep(ep, EpReg::EP_ID) as EpId))
}

fn prepare_write(ep: EpId) -> Result<(PEId, EpId), Error> {
    check_rdwr(ep, false)?;

    let buf = buffer();
    let src = DTU::get_cmd(CmdReg::ADDR);
    let size = DTU::get_cmd(CmdReg::SIZE) as usize;

    buf.header.credits = 0;
    buf.header.label = DTU::get_ep(ep, EpReg::LABEL);
    buf.header.length = size + 2 * util::size_of::<u64>();

    let data = buf.as_words_mut();
    data[0] = DTU::get_cmd(CmdReg::OFFSET);
    data[1] = size as u64;

    unsafe {
        libc::memcpy(
            data[2..].as_mut_ptr() as *mut libc::c_void,
            src as *const libc::c_void,
            size as usize
        );
    }

    Ok((DTU::get_ep(ep, EpReg::PE_ID) as PEId, DTU::get_ep(ep, EpReg::EP_ID) as EpId))
}

fn prepare_ack(ep: EpId) -> Result<(PEId, EpId), Error> {
    let addr = DTU::get_cmd(CmdReg::OFFSET);
    let buf_addr = DTU::get_ep(ep, EpReg::BUF_ADDR);
    let msg_ord = DTU::get_ep(ep, EpReg::BUF_MSGORDER);
    let ord = DTU::get_ep(ep, EpReg::BUF_ORDER);

    let idx = (addr - buf_addr) >> msg_ord;
    if idx >= (1 << (ord - msg_ord)) {
        log_dtu!("DTU-error: EP{}: invalid message addr {:#x}", ep, addr);
        return Err(Error::new(Code::InvArgs));
    }

    let mut occupied = DTU::get_ep(ep, EpReg::BUF_OCCUPIED);
    assert!(is_bit_set(occupied, idx));
    occupied = set_bit(occupied, idx, false);
    DTU::set_ep(ep, EpReg::BUF_OCCUPIED, occupied);

    log_dtu!("EP{}: acked message at index {}", ep, idx);

    Ok((0, EP_COUNT))
}

fn prepare_fetch(ep: EpId) -> Result<(PEId, EpId), Error> {
    let msgs = DTU::get_ep(ep, EpReg::BUF_MSG_CNT);
    if msgs == 0 {
        return Ok((0, EP_COUNT));
    }

    let unread = DTU::get_ep(ep, EpReg::BUF_UNREAD);
    let roff = DTU::get_ep(ep, EpReg::BUF_ROFF);
    let ord = DTU::get_ep(ep, EpReg::BUF_ORDER);
    let msg_ord = DTU::get_ep(ep, EpReg::BUF_MSGORDER);
    let size = 1 << (ord - msg_ord);

    let recv_msg = |idx| {
        assert!(is_bit_set(DTU::get_ep(ep, EpReg::BUF_OCCUPIED), idx));

        let unread = set_bit(unread, idx, false);
        let msgs = msgs - 1;
        assert!(unread.count_ones() == msgs as u32);

        log_dtu!("EP{}: fetched msg at index {} (count={})", ep, idx, msgs);

        DTU::set_ep(ep, EpReg::BUF_UNREAD, unread);
        DTU::set_ep(ep, EpReg::BUF_ROFF, idx + 1);
        DTU::set_ep(ep, EpReg::BUF_MSG_CNT, msgs);

        let addr = DTU::get_ep(ep, EpReg::BUF_ADDR);
        DTU::set_cmd(CmdReg::OFFSET, addr + idx * (1 << msg_ord));

        Ok((0, EP_COUNT))
    };

    for i in roff..size {
        if is_bit_set(unread, i) {
            return recv_msg(i);
        }
    }
    for i in 0..roff {
        if is_bit_set(unread, i) {
            return recv_msg(i);
        }
    }

    unreachable!();
}

fn handle_msg(ep: EpId, len: usize) {
    let msg_ord = DTU::get_ep(ep, EpReg::BUF_MSGORDER);
    let msg_size = 1 << msg_ord;
    if len > msg_size {
        log_dtu!(
            "DTU-error: dropping msg due to insufficient space (required: {}, available: {})",
            len, msg_size
        );
        return;
    }

    let occupied = DTU::get_ep(ep, EpReg::BUF_OCCUPIED);
    let woff = DTU::get_ep(ep, EpReg::BUF_WOFF);
    let ord = DTU::get_ep(ep, EpReg::BUF_ORDER);
    let size = 1 << (ord - msg_ord);

    let place_msg = |idx, occupied| {
        let unread = DTU::get_ep(ep, EpReg::BUF_UNREAD);
        let msgs = DTU::get_ep(ep, EpReg::BUF_MSG_CNT);

        let occupied = set_bit(occupied, idx, true);
        let unread = set_bit(unread, idx, true);
        let msgs = msgs + 1;
        assert!(unread.count_ones() == msgs as u32);

        log_dtu!("EP{}: put msg at index {} (count={})", ep, idx, msgs);

        DTU::set_ep(ep, EpReg::BUF_OCCUPIED, occupied);
        DTU::set_ep(ep, EpReg::BUF_UNREAD, unread);
        DTU::set_ep(ep, EpReg::BUF_MSG_CNT, msgs);
        DTU::set_ep(ep, EpReg::BUF_WOFF, idx + 1);

        let addr = DTU::get_ep(ep, EpReg::BUF_ADDR);
        let dst = (addr + idx * (1 << msg_ord)) as *mut u8;
        let src = &buffer().header as *const Header as *const u8;
        unsafe {
            util::slice_for_mut(dst, len).copy_from_slice(util::slice_for(src, len));
        }
    };

    for i in woff..size {
        if !is_bit_set(occupied, i) {
            place_msg(i, occupied);
            return;
        }
    }
    for i in 0..woff {
        if !is_bit_set(occupied, i) {
            place_msg(i, occupied);
            return;
        }
    }

    log_dtu!("DTU-error: EP{}: dropping msg because no slot is free", ep);
}

fn handle_write_cmd() {
    let buf = buffer();
    let data = buf.as_words();
    let base = buf.header.label & !(kif::Perm::RWX.bits() as Label);
    let offset = base + data[0];
    let length = data[1];

    log_dtu!("(write) {} bytes to {:#x}+{:#x}", length, base, offset - base);
    assert!(length as usize <= MAX_MSG_SIZE - 2 * util::size_of::<u64>());

    unsafe {
        libc::memcpy(
            offset as *mut libc::c_void,
            data[2..].as_ptr() as *const libc::c_void,
            length as usize
        );
    }
}

fn handle_read_cmd(backend: &backend::SocketBackend, ep: EpId) {
    let buf = buffer();
    let base = buf.header.label & !(kif::Perm::RWX.bits() as Label);

    let (offset, length, dest) = {
        let data = buf.as_words();
        (base + data[0], data[1], data[2])
    };

    log_dtu!("(read) {} bytes from {:#x}+{:#x} -> {:#x}", length, base, offset - base, dest);
    assert!(length as usize <= MAX_MSG_SIZE - 3 * util::size_of::<u64>());

    let dst_pe = buf.header.pe as PEId;
    let dst_ep = buf.header.rpl_ep as EpId;

    buf.header.opcode = Command::RESP.val as u8;
    buf.header.credits = 0;
    buf.header.label = 0;
    buf.header.length = length as usize + 3 * util::size_of::<u64>();

    let data = buf.as_words_mut();
    data[0] = dest;
    data[1] = length;
    data[2] = 0;

    unsafe {
        libc::memcpy(
            data[3..].as_mut_ptr() as *mut libc::c_void,
            offset as *const libc::c_void,
            length as usize
        );
    }

    send_msg(backend, ep, dst_pe, dst_ep);
}

fn handle_resp_cmd() {
    let buf = buffer();
    let data = buf.as_words();
    let base = buf.header.label & !(kif::Perm::RWX.bits() as Label);
    let offset = base + data[0];
    let length = data[1];
    let resp = data[2];

    log_dtu!("(resp) {} bytes to {:#x}+{:#x} -> {:#x}", length, base, offset - base, resp);
    assert!(length as usize <= MAX_MSG_SIZE - 3 * util::size_of::<usize>());

    unsafe {
        libc::memcpy(
            offset as *mut libc::c_void,
            data[3..].as_ptr() as *const libc::c_void,
            length as usize
        );
    }

    // provide feedback to SW
    DTU::set_cmd(CmdReg::CTRL, resp << 16);
}

fn send_msg(backend: &backend::SocketBackend, ep: EpId, dst_pe: PEId, dst_ep: EpId) {
    let buf = buffer();

    log_dtu!(
        "{} {:3}b lbl={:#016x} over {} to pe:ep={}:{} (crd={:#x} rep={})",
        if buf.header.opcode == Command::REPLY.val as u8 { ">>" } else { "->" },
        buf.header.length,
        buf.header.label,
        buf.header.snd_ep,
        dst_pe, dst_ep,
        DTU::get_ep(ep, EpReg::CREDITS),
        buf.header.rpl_ep
    );

    backend.send(dst_pe, dst_ep, buf);
}

fn handle_command(backend: &backend::SocketBackend) {
    // clear error
    DTU::set_cmd(CmdReg::CTRL, DTU::get_cmd(CmdReg::CTRL) & 0xFFFF);

    let ep = DTU::get_cmd(CmdReg::EPID) as EpId;

    let res = if ep >= EP_COUNT {
        log_dtu!("DTU-error: invalid ep-id ({})", ep);
        Err(Error::new(Code::InvArgs))
    }
    else {
        let ctrl = DTU::get_cmd(CmdReg::CTRL);
        let op: Command = Command::from((ctrl >> 3) & 0xF);

        let res = match op {
            Command::SEND       => prepare_send(ep),
            Command::REPLY      => prepare_reply(ep),
            Command::READ       => prepare_read(ep),
            Command::WRITE      => prepare_write(ep),
            Command::FETCH_MSG  => prepare_fetch(ep),
            Command::ACK_MSG    => prepare_ack(ep),
            _                   => Err(Error::new(Code::NotSup)),
        };

        match res {
            Ok((dst_pe, dst_ep)) if dst_ep < EP_COUNT => {
                let buf = buffer();
                buf.header.opcode = op.val as u8;

                if op != Command::REPLY {
                    // reply cap
                    buf.header.has_replycap = 1;
                    buf.header.pe = arch::envdata::get().pe_id() as u16;
                    buf.header.snd_ep = ep as u8;
                    buf.header.rpl_ep = DTU::get_cmd(CmdReg::REPLY_EPID) as u8;
                    buf.header.reply_label = DTU::get_cmd(CmdReg::REPLY_LBL);
                }

                send_msg(backend, ep, dst_pe, dst_ep);

                if op == Command::READ {
                    // wait for the response
                    Ok(op.val << 3)
                }
                else {
                    Ok(0)
                }
            },
            Ok((_, _))  => Ok(0),
            Err(e)      => Err(e),
        }
    };

    match res {
        Ok(val) => DTU::set_cmd(CmdReg::CTRL, val),
        Err(e)  => DTU::set_cmd(CmdReg::CTRL, (e.code() as Reg) << 16),
    };
}

fn handle_receive(backend: &backend::SocketBackend, ep: EpId) {
    let buf = buffer();
    if let Some(size) = backend.receive(ep, buf) {
        match Command::from(buf.header.opcode) {
            Command::SEND | Command::REPLY  => handle_msg(ep, size),
            Command::READ                   => handle_read_cmd(backend, ep),
            Command::WRITE                  => handle_write_cmd(),
            Command::RESP                   => handle_resp_cmd(),
            _                               => panic!("Not supported!"),
        }

        // refill credits
        let crd_ep = buf.header.crd_ep as EpId;
        if crd_ep >= EP_COUNT {
            log_dtu!("DTU-error: should give credits to ep {}", crd_ep);
        }
        else {
            let msg_ord = DTU::get_ep(crd_ep, EpReg::MSGORDER);
            let credits = DTU::get_ep(crd_ep, EpReg::CREDITS);
            if buf.header.credits != 0 && credits != !0 {
                log_dtu!(
                    "Refilling credits of ep {} from {:#x} to {:#x}",
                    crd_ep, credits, credits + (1 << msg_ord)
                );
                DTU::set_ep(crd_ep, EpReg::CREDITS, credits + (1 << msg_ord));
            }
        }

        log_dtu!(
            "<- {:3}b lbl={:#016x} ep={} (cnt={:#x}, crd={:#x})",
            size - util::size_of::<Header>(),
            buf.header.label,
            ep,
            DTU::get_ep(ep, EpReg::BUF_MSG_CNT),
            DTU::get_ep(ep, EpReg::CREDITS),
        );
    }
}

static mut BACKEND: Option<backend::SocketBackend> = None;
static mut RUN: bool = true;
static mut TID: libc::pthread_t = 0;

extern "C" fn run(_arg: *mut libc::c_void) -> *mut libc::c_void {
    let backend = unsafe { BACKEND.as_ref().unwrap() };

    while unsafe { ptr::read_volatile(&mut RUN) } {
        if (DTU::get_cmd(CmdReg::CTRL) & Control::START.bits()) != 0 {
            handle_command(&backend);
        }

        for ep in 0..EP_COUNT {
            handle_receive(&backend, ep);
        }

        DTU::try_sleep(false, 0).unwrap();
    }

    ptr::null_mut()
}

pub fn init() {
    unsafe {
        LOG = Some(io::log::Log::new());
    }
    log().init();

    unsafe {
        BACKEND = Some(backend::SocketBackend::new());

        let res = libc::pthread_create(&mut TID, ptr::null(), run, ptr::null_mut());
        assert!(res == 0);
    }
}

pub fn deinit() {
    unsafe {
        RUN = false;
        assert!(libc::pthread_join(TID, ptr::null_mut()) == 0);

        BACKEND = None;
    }
}
