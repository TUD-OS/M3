#![no_std]

#[macro_use]
extern crate m3;

use m3::syscalls;
use m3::time;
use m3::env;
use m3::collections::*;
use m3::com::*;
use m3::session::*;

#[no_mangle]
pub fn main() -> i32 {
    {
        let mut pipe = Pipe::new("pipe", 64 * 1024).expect("unable to create pipe");
        pipe.attach(true).expect("unable to attach");
        let mut wr_pos = pipe.request_write(8192, 0).expect("request_write failed");
        println!("Got {}", wr_pos);
        wr_pos = pipe.request_write(4096, 8192).expect("request_write failed");
        println!("Got {}", wr_pos);
        let (rd_pos, amount) = pipe.request_read(4096).expect("request_read failed");
        println!("Got {} {}", rd_pos, amount);
    }

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
        let mut rgate = RecvGate::new(12, 8).unwrap();
        rgate.activate().unwrap();

        let mut sgate = SendGate::new_with(
            SGateArgs::new(&rgate).credits((1 << 8) * 10).label(0x1234)
        ).unwrap();
        sgate.activate().unwrap();

        let mut total = 0;
        for _ in 0..10 {
            let start = time::start(0xDEADBEEF);
            send_vmsg!(&mut sgate, RecvGate::def(), 23, 42, "foobar_test_asd").unwrap();

            let (a1, a2, a3) = recv_vmsg!(&mut rgate, i32, i32, String).unwrap();

            let end = time::stop(0xDEADBEEF);

            total += end - start;

            println!("msg: {} {} {}", a1, a2, a3);
        }

        println!("Time: {}", total / 10);
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
