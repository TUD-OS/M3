pub static DEF: bool    = true;
pub static ERR: bool    = true;
pub static EPS: bool    = false;
pub static SYSC: bool   = false;
pub static KENV: bool   = false;
pub static MEM: bool    = false;
pub static SERV: bool   = false;
pub static SQUEUE: bool = false;
pub static PTES: bool   = false;
pub static VPES: bool   = false;

#[macro_export]
macro_rules! klog {
    ($type:tt, $fmt:expr)              => (log_impl!($crate::log::$type, concat!($fmt, "\n")));
    ($type:tt, $fmt:expr, $($arg:tt)*) => (log_impl!($crate::log::$type, concat!($fmt, "\n"), $($arg)*));
}
