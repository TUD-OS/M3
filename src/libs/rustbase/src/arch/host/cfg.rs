pub const PAGE_BITS: usize          = 12;
pub const PAGE_SIZE: usize          = 1 << PAGE_BITS;
pub const PAGE_MASK: usize          = PAGE_SIZE - 1;

pub const MAX_RB_SIZE: usize        = usize::max_value();

pub const MEM_CAP_END: usize        = 0xFFFF_FFFF_FFFF_FFFF;

pub const PE_COUNT: usize           = 18;
pub const MEM_SIZE: usize           = 512 * 1024 * 1024;
pub const FS_MAX_SIZE: usize        = 256 * 1024 * 1024;

pub const STACK_SIZE: usize         = 0x8000;
pub const APP_HEAP_SIZE: usize      = 64 * 1024 * 1024;

pub const SYSC_RBUF_ORD: i32        = 9;
pub const UPCALL_RBUF_ORD: i32      = 9;
pub const DEF_RBUF_ORD: i32         = 8;

pub const SYSC_RBUF_SIZE: usize     = 1 << SYSC_RBUF_ORD;
pub const UPCALL_RBUF_SIZE: usize   = 1 << UPCALL_RBUF_ORD;
pub const DEF_RBUF_SIZE: usize      = 1 << DEF_RBUF_ORD;
