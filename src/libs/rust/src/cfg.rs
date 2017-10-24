pub const PAGE_SIZE: usize          = 0x1000;

pub const RT_START: usize           = 0x6000;
pub const STACK_TOP: usize          = 0xC000;
pub const APP_HEAP_SIZE: usize      = 64 * 1024 * 1024;

pub const RECVBUF_SPACE: usize      = 0x3FC00000;
pub const RECVBUF_SIZE: usize       = 4 * PAGE_SIZE;
pub const RECVBUF_SIZE_SPM: usize   = 16384;

pub const SYSC_RBUF_SIZE: usize     = 1 << 9;
pub const UPCALL_RBUF_SIZE: usize   = 1 << 9;
pub const DEF_RBUF_SIZE: usize      = 1 << 8;
