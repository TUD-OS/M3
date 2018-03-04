use time;

fn rdtsc() -> time::Time {
    let u: u32;
    let l: u32;
    unsafe {
        asm!(
            "rdtsc"
            : "={rax}"(l), "={rdx}"(u)
        );
    }
    (u as time::Time) << 32 | (l as time::Time)
}

pub fn start(_msg: usize) -> time::Time {
    rdtsc()
}

pub fn stop(_msg: usize) -> time::Time {
    rdtsc()
}
