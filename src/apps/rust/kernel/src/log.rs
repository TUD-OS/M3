pub static DEF: bool    = true;
pub static EPS: bool    = true;
pub static SYSC: bool   = true;
pub static KENV: bool   = true;
pub static MEM: bool    = true;

#[macro_export]
macro_rules! klog {
    ($type:tt, $fmt:expr)              => (log_impl!($crate::log::$type, concat!($fmt, "\n")));
    ($type:tt, $fmt:expr, $($arg:tt)*) => (log_impl!($crate::log::$type, concat!($fmt, "\n"), $($arg)*));
}
