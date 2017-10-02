extern {
    // pub fn gem5_shutdown(delay: u64);
    pub fn gem5_writefile(src: *const u8, len: u64, offset: u64, file: u64);
    // pub fn gem5_readfile(dst: *mut u8, max: u64, offset: u64) -> i64;

    pub fn strlen(s: *const u8) -> usize;
}
