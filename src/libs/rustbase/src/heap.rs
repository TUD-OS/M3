//! Contains the malloc implementation

use arch::cfg;
use core::intrinsics;
use libc;
use io;
use util;

#[repr(C, packed)]
pub struct HeapArea {
    pub next: u64,    /* HEAP_USED_BITS set = used */
    pub prev: u64,
    _pad: [u8; 64 - 16],
}

extern {
    fn heap_set_alloc_callback(cb: extern fn(p: *const u8, size: usize));
    fn heap_set_free_callback(cb: extern fn(p: *const u8));
    fn heap_set_oom_callback(cb: extern fn(size: usize) -> bool);
    fn heap_set_dblfree_callback(cb: extern fn(p: *const u8));

    /// Allocates `size` bytes on the heap
    pub fn heap_alloc(size: usize) -> *mut libc::c_void;

    /// Allocates `n * size` on the heap and initializes it to 0
    pub fn heap_calloc(n: usize, size: usize) -> *mut libc::c_void;

    /// Reallocates `n` to be `size` bytes large
    ///
    /// This implementation might increase the size of the area or shink it. It might also free the
    /// current area and allocate a new area of `size` bytes.
    pub fn heap_realloc(p: *mut libc::c_void, size: usize) -> *mut libc::c_void;

    /// Frees the area at `p`
    pub fn heap_free(p: *mut libc::c_void);

    fn heap_append(pages: usize);

    fn heap_free_memory() -> usize;
    fn heap_used_end() -> usize;
}

extern {
    static _bss_end: u8;
    static mut heap_begin: *mut HeapArea;
    static mut heap_end: *mut HeapArea;
}

#[cfg(target_os = "none")]
fn init_heap() {
    use arch;
    use kif::PEDesc;

    unsafe {
        let begin = &_bss_end as *const u8;
        heap_begin = util::round_up(begin as usize, util::size_of::<HeapArea>()) as *mut HeapArea;

        let env = arch::envdata::get();
        let end = if env.heap_size == 0 {
            PEDesc::new_from(env.pe_desc).mem_size() - cfg::RECVBUF_SIZE_SPM
        }
        else {
            util::round_up(begin as usize, cfg::PAGE_SIZE) + env.heap_size as usize
        };

        heap_end = (end as *mut HeapArea).offset(-1);
    }
}

#[cfg(target_os = "linux")]
fn init_heap() {
    use core::ptr;

    unsafe {
        let addr = libc::mmap(
            ptr::null_mut(),
            cfg::APP_HEAP_SIZE,
            libc::PROT_READ | libc::PROT_WRITE,
            libc::MAP_ANON | libc::MAP_PRIVATE,
            -1,
            0
        );
        assert!(addr != libc::MAP_FAILED);
        heap_begin = addr as *mut HeapArea;
        heap_end = ((addr as usize + cfg::APP_HEAP_SIZE) as *mut HeapArea).offset(-1);
    }
}

pub fn init() {
    init_heap();

    unsafe {
        let num_areas = heap_begin.offset_to(heap_end).unwrap() as i64;
        let space = num_areas * util::size_of::<HeapArea>() as i64;

        log!(HEAP, "Heap has {} bytes", space);

        (*heap_end).next = 0;
        (*heap_end).prev = space as u64;

        (*heap_begin).next = space as u64;
        (*heap_begin).prev = 0;

        if io::log::HEAP {
            heap_set_alloc_callback(heap_alloc_callback);
            heap_set_free_callback(heap_free_callback);
        }
        heap_set_dblfree_callback(heap_dblfree_callback);
        heap_set_oom_callback(heap_oom_callback);
    }
}

pub fn append(pages: usize) {
    unsafe {
        heap_append(pages);
    }
}

/// Returns the number of free bytes on the heap
pub fn free_memory() -> usize {
    unsafe {
        heap_free_memory()
    }
}

/// Returns the end of used part of the heap
pub fn used_end() -> usize {
    unsafe {
        heap_used_end()
    }
}

extern fn heap_alloc_callback(p: *const u8, size: usize) {
    log!(HEAP, "alloc({}) -> {:?}", size, p);
}

extern fn heap_free_callback(p: *const u8) {
    log!(HEAP, "free({:?})", p);
}

extern fn heap_dblfree_callback(p: *const u8) {
    panic!("Used bits not set for {:?}; double free?", p);
}

extern fn heap_oom_callback(size: usize) -> bool {
    panic!("Unable to allocate {} bytes on the heap: out of memory", size);
}

#[no_mangle]
pub unsafe extern fn __rdl_alloc(size: usize,
                                 _align: usize,
                                 _err: *mut u8) -> *mut libc::c_void {
    heap_alloc(size)
}

#[no_mangle]
pub unsafe extern fn __rdl_dealloc(ptr: *mut libc::c_void,
                                   _size: usize,
                                   _align: usize) {
    heap_free(ptr);
}

#[no_mangle]
pub unsafe extern fn __rdl_realloc(ptr: *mut libc::c_void,
                                   _old_size: usize,
                                   _old_align: usize,
                                   new_size: usize,
                                   _new_align: usize,
                                   _err: *mut u8) -> *mut libc::c_void {
    heap_realloc(ptr, new_size)
}

#[no_mangle]
pub unsafe extern fn __rdl_alloc_zeroed(size: usize,
                                        _align: usize,
                                        _err: *mut u8) -> *mut libc::c_void {
    heap_calloc(size, 1)
}

#[no_mangle]
pub unsafe extern fn __rdl_oom(_err: *const u8) -> ! {
    intrinsics::abort();
}

#[no_mangle]
pub unsafe extern fn __rdl_usable_size(_layout: *const u8,
                                       _min: *mut usize,
                                       _max: *mut usize) {
    // TODO implement me
}

#[no_mangle]
pub unsafe extern fn __rdl_alloc_excess(size: usize,
                                        _align: usize,
                                        _excess: *mut usize,
                                        _err: *mut u8) -> *mut libc::c_void {
    // TODO is that correct?
    heap_alloc(size)
}

#[no_mangle]
pub unsafe extern fn __rdl_realloc_excess(ptr: *mut libc::c_void,
                                          _old_size: usize,
                                          _old_align: usize,
                                          new_size: usize,
                                          _new_align: usize,
                                          _excess: *mut usize,
                                          _err: *mut u8) -> *mut libc::c_void {
    // TODO is that correct?
    heap_realloc(ptr, new_size)
}

#[no_mangle]
pub unsafe extern fn __rdl_grow_in_place(_ptr: *mut libc::c_void,
                                         _old_size: usize,
                                         _old_align: usize,
                                         _new_size: usize,
                                         _new_align: usize) -> u8 {
    // TODO implement me
    0
}

#[no_mangle]
pub unsafe extern fn __rdl_shrink_in_place(_ptr: *mut libc::c_void,
                                           _old_size: usize,
                                           _old_align: usize,
                                           _new_size: usize,
                                           _new_align: usize) -> u8 {
    // TODO implement me
    0
}
