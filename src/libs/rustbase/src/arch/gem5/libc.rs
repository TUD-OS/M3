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
    pub fn strlen(s: *const i8) -> usize;

    #[link_name = "heap_alloc"]
    pub fn malloc(size: usize) -> *mut c_void;
    #[link_name = "heap_calloc"]
    pub fn calloc(n: usize, size: usize) -> *mut c_void;
    #[link_name = "heap_realloc"]
    pub fn realloc(p: *mut c_void, size: usize) -> *mut c_void;
    #[link_name = "heap_free"]
    pub fn free(p: *mut c_void);
}
