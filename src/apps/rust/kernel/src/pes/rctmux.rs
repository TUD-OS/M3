use base::goff;

pub const ENTRY_ADDR: goff = 0x1000;
pub const YIELD_ADDR: goff = 0x5FF0;
pub const FLAGS_ADDR: goff = 0x5FF8;

bitflags! {
    pub struct Flags : u64 {
        const STORE       = 1 << 0; // store operation required
        const RESTORE     = 1 << 1; // restore operation required
        const WAITING     = 1 << 2; // set by the kernel if a signal is required
        const SIGNAL      = 1 << 3; // used to signal completion to the kernel
    }
}
