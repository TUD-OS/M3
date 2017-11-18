#![no_std]

#[macro_use]
extern crate m3;

#[no_mangle]
pub fn main() -> i32 {
    println!("Hello world!");

    let mgate1 = m3::com::MemGate::new(0x1000, m3::kif::Perm::RW).unwrap();
    println!("{:?}", mgate1);

    let start = m3::time::start(0x666);
    let mgate = m3::com::MemGate::new(0x1000, m3::kif::Perm::RW).unwrap();
    let end = m3::time::stop(0x666);
    println!("Time: {} for {:?}", end - start, mgate);

    0
}
