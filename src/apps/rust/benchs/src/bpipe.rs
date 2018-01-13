use m3::boxed::Box;
use m3::com::MemGate;
use m3::io;
use m3::kif;
use m3::profile;
use m3::test;
use m3::vfs::IndirectPipe;
use m3::vpe::{Activity, VPE, VPEArgs};

const DATA_SIZE: usize  = 2 * 1024 * 1024;
const BUF_SIZE: usize   = 8 * 1024;

pub fn run(t: &mut test::Tester) {
    run_test!(t, child_to_parent);
    run_test!(t, parent_to_child);
}

fn child_to_parent() {
    let mut prof = profile::Profiler::new().repeats(2).warmup(1);

    println!("c->p: {} KiB transfer with {} KiB buf: {}", DATA_SIZE / 1024, BUF_SIZE / 1024,
        prof.run_with_id(|| {
            let pipe_mem = assert_ok!(MemGate::new(0x10000, kif::Perm::RW));
            let pipe = assert_ok!(IndirectPipe::new(&pipe_mem, 0x10000));

            let mut vpe = assert_ok!(VPE::new_with(VPEArgs::new("writer")));
            vpe.files().set(io::STDOUT_FILENO, VPE::cur().files().get(pipe.writer_fd()).unwrap());
            assert_ok!(vpe.obtain_fds());

            let act = assert_ok!(vpe.run(Box::new(|| {
                let buf = vec![0u8; BUF_SIZE];
                let output = VPE::cur().files().get(io::STDOUT_FILENO).unwrap();
                let mut rem = DATA_SIZE;
                while rem > 0 {
                    assert_ok!(output.borrow_mut().write(&buf));
                    rem -= BUF_SIZE;
                }
                0
            })));

            pipe.close_writer();

            let mut buf = vec![0u8; BUF_SIZE];
            let input = VPE::cur().files().get(pipe.reader_fd()).unwrap();
            while assert_ok!(input.borrow_mut().read(&mut buf)) > 0 {
            }

            assert_eq!(act.wait(), Ok(0));
        }, 0x90)
    );
}

fn parent_to_child() {
    let mut prof = profile::Profiler::new().repeats(2).warmup(1);

    println!("p->c: {} KiB transfer with {} KiB buf: {}", DATA_SIZE / 1024, BUF_SIZE / 1024,
        prof.run_with_id(|| {
            let pipe_mem = assert_ok!(MemGate::new(0x10000, kif::Perm::RW));
            let pipe = assert_ok!(IndirectPipe::new(&pipe_mem, 0x10000));

            let mut vpe = assert_ok!(VPE::new_with(VPEArgs::new("reader")));
            vpe.files().set(io::STDIN_FILENO, VPE::cur().files().get(pipe.reader_fd()).unwrap());
            assert_ok!(vpe.obtain_fds());

            let act = assert_ok!(vpe.run(Box::new(|| {
                let mut buf = vec![0u8; BUF_SIZE];
                let input = VPE::cur().files().get(io::STDIN_FILENO).unwrap();
                while assert_ok!(input.borrow_mut().read(&mut buf)) > 0 {
                }
                0
            })));

            pipe.close_reader();

            let buf = vec![0u8; BUF_SIZE];
            let output = VPE::cur().files().get(pipe.writer_fd()).unwrap();
            let mut rem = DATA_SIZE;
            while rem > 0 {
                assert_ok!(output.borrow_mut().write(&buf));
                rem -= BUF_SIZE;
            }

            pipe.close_writer();

            assert_eq!(act.wait(), Ok(0));
        }, 0x91)
    );
}
