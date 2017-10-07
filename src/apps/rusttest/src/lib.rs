#![no_std]

#[macro_use]
extern crate m3;

use m3::syscalls;
use m3::time;
use m3::env;
use m3::collections::*;
use m3::com::*;

#[no_mangle]
pub fn main() -> i32 {
    let vec = vec![1, 42, 23];
    println!("my vec:");
    for v in vec {
        println!("  {}", v);
    }

    let mut s: String = format!("my float is {:.3} and my args are", 12.5);
    for a in env::args() {
        s += " ";
        s += a;
    }
    println!("here: {}", s);

    for (i, a) in env::args().enumerate() {
        println!("arg {}: {}", i, a);
    }

    let args: Vec<&'static str> = env::args().collect();
    println!("arg0: {}, arg1: {}", args[0], args[1]);

    {
        let mgate = MemGate::new(0x1000, Perm::RW).unwrap();
        let mut mgate2 = mgate.derive(0x100, 0x100, Perm::RW).unwrap();

        let mut data: [u8; 16] = [12; 16];
        mgate2.write(&data, 0).unwrap();
        mgate2.read(&mut data, 0).unwrap();
        println!("data: {:?}", data);

        MemGate::new(0x1000, Perm::RW).err();
    }

    {
        let mut rgate = RecvGate::new(10, 8).unwrap();
        rgate.activate().unwrap();

        let mut sgate = SendGate::new_with(
            SGateArgs::new(&rgate).credits(0x100).label(0x1234)
        ).unwrap();

        let msg: [u8; 8] = [0xFF; 8];
        sgate.send(&msg).unwrap();
    }

    let mut total = 0;
    for _ in 0..10 {
        let start = time::start(0);
        syscalls::noop().unwrap();
        let end = time::stop(0);
        total += end - start;
    }
    assert!(total > 10);

    println!("per call: {}", total / 10);

    0
}
