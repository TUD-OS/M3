#[repr(u8)]
#[allow(non_camel_case_types)]
pub enum c_void {
    // Two dummy variants so the #[repr] attribute can be used.
    #[doc(hidden)]
    __variant1,
    #[doc(hidden)]
    __variant2,
}

extern {
    pub fn memcpy(dst: *mut c_void, src: *const c_void, len: usize) -> *mut c_void;
    pub fn memset(dst: *mut c_void, val: i32, len: usize) -> *mut c_void;
    pub fn strlen(s: *const i8) -> usize;
}
