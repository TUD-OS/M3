pub const PAGE_SIZE: usize          = 0x1000;

pub const RT_START: usize           = 0x6000;
pub const RT_SIZE: usize            = 0x2000;
pub const STACK_SIZE: usize         = 0x8000;
pub const STACK_BOTTOM: usize       = RT_START + RT_SIZE + PAGE_SIZE;
pub const STACK_TOP: usize          = STACK_BOTTOM + STACK_SIZE;
pub const APP_HEAP_SIZE: usize      = 64 * 1024 * 1024;
