use m3::collections::{String, ToString};
use m3::com::*;
use m3::profile;
use m3::test;
use m3::util;

pub fn run(t: &mut test::Tester) {
    run_test!(t, pingpong);
}

fn pingpong() {
    let msg_size = 128;
    let msg_ord  = util::next_log2(msg_size);

    let reply_gate = RecvGate::def();
    let mut rgate = assert_ok!(RecvGate::new(msg_ord, msg_ord));
    assert_ok!(rgate.activate());
    let sgate = assert_ok!(SendGate::new_with(SGateArgs::new(&rgate).credits(msg_size as u64)));

    let mut prof = profile::Profiler::new();

    println!("pingpong with (1 * u64) msgs : {}", prof.run_with_id(|| {
        assert_ok!(send_vmsg!(&sgate, reply_gate, 0u64));

        let mut msg = assert_ok!(recv_msg(&rgate));
        assert_eq!(msg.pop::<u64>(), 0);
        assert_ok!(reply_vmsg!(msg, 0u64));

        let mut reply = assert_ok!(recv_msg(reply_gate));
        assert_eq!(reply.pop::<u64>(), 0);
    }, 0x0));

    println!("pingpong with (2 * u64) msgs : {}", prof.run_with_id(|| {
        assert_ok!(send_vmsg!(&sgate, reply_gate, 23u64, 42u64));

        let mut msg = assert_ok!(recv_msg(&rgate));
        assert_eq!(msg.pop::<u64>(), 23);
        assert_eq!(msg.pop::<u64>(), 42);
        assert_ok!(reply_vmsg!(msg, 5u64, 6u64));

        let mut reply = assert_ok!(recv_msg(reply_gate));
        assert_eq!(reply.pop::<u64>(), 5);
        assert_eq!(reply.pop::<u64>(), 6);
    }, 0x1));

    println!("pingpong with (4 * u64) msgs : {}", prof.run_with_id(|| {
        assert_ok!(send_vmsg!(&sgate, reply_gate, 23u64, 42u64, 10u64, 12u64));

        let mut msg = assert_ok!(recv_msg(&rgate));
        assert_eq!(msg.pop::<u64>(), 23);
        assert_eq!(msg.pop::<u64>(), 42);
        assert_eq!(msg.pop::<u64>(), 10);
        assert_eq!(msg.pop::<u64>(), 12);
        assert_ok!(reply_vmsg!(msg, 5u64, 6u64, 7u64, 8u64));

        let mut reply = assert_ok!(recv_msg(reply_gate));
        assert_eq!(reply.pop::<u64>(), 5);
        assert_eq!(reply.pop::<u64>(), 6);
        assert_eq!(reply.pop::<u64>(), 7);
        assert_eq!(reply.pop::<u64>(), 8);
    }, 0x2));

    println!("pingpong with (String) msgs  : {}", prof.run_with_id(|| {
        assert_ok!(send_vmsg!(&sgate, reply_gate, "test"));

        let mut msg = assert_ok!(recv_msg(&rgate));
        assert_eq!(msg.pop::<String>(), "test".to_string());
        assert_ok!(reply_vmsg!(msg, "foobar"));

        let mut reply = assert_ok!(recv_msg(reply_gate));
        assert_eq!(reply.pop::<String>(), "foobar".to_string());
    }, 0x3));
}
