pub const PAGE_BITS: usize          = 12;
pub const PAGE_SIZE: usize          = 1 << PAGE_BITS;
pub const PAGE_MASK: usize          = PAGE_SIZE - 1;

pub const RECVBUF_SPACE: usize      = 0x3FC00000;
pub const RECVBUF_SIZE: usize       = 4 * PAGE_SIZE;
pub const RECVBUF_SIZE_SPM: usize   = 16384;
pub const MAX_RB_SIZE: usize        = 32;

pub const MEM_CAP_END: usize        = RECVBUF_SPACE;

pub const RT_START: usize           = 0x6000;
pub const RT_SIZE: usize            = 0x2000;
pub const STACK_SIZE: usize         = 0x8000;
pub const STACK_BOTTOM: usize       = RT_START + RT_SIZE + PAGE_SIZE;
pub const STACK_TOP: usize          = STACK_BOTTOM + STACK_SIZE;

pub const APP_HEAP_SIZE: usize      = 64 * 1024 * 1024;
pub const MOD_HEAP_SIZE: usize      = 512 * 1024;

pub const SYSC_RBUF_ORD: i32        = 9;
pub const UPCALL_RBUF_ORD: i32      = 9;
pub const DEF_RBUF_ORD: i32         = 8;

pub const SYSC_RBUF_SIZE: usize     = 1 << SYSC_RBUF_ORD;
pub const UPCALL_RBUF_SIZE: usize   = 1 << UPCALL_RBUF_ORD;
pub const DEF_RBUF_SIZE: usize      = 1 << DEF_RBUF_ORD;
