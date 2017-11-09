use m3::col::String;
use m3::com::{recv_msg, recv_msg_from, RecvGate, SendGate, SGateArgs};
use m3::errors::Code;
use m3::test;
use m3::util;

pub fn run(t: &mut test::Tester) {
    run_test!(t, create);
    run_test!(t, send_recv);
    run_test!(t, send_reply);
}

fn create() {
    let rgate = assert_ok!(RecvGate::new(util::next_log2(512), util::next_log2(256)));
    assert_err!(SendGate::new_with(SGateArgs::new(&rgate).sel(1)), Code::InvArgs);
}

fn send_recv() {
    let mut rgate = assert_ok!(RecvGate::new(util::next_log2(512), util::next_log2(256)));
    let sgate = assert_ok!(SendGate::new_with(
        SGateArgs::new(&rgate).credits(512).label(0x1234)
    ));
    assert!(sgate.ep().is_none());
    assert_ok!(rgate.activate());

    let data = [0u8; 16];
    assert_ok!(sgate.send(&data, RecvGate::def()));
    assert!(sgate.ep().is_some());
    assert_ok!(sgate.send(&data, RecvGate::def()));
    assert_err!(sgate.send(&data, RecvGate::def()), Code::MissCredits);

    {
        let is = assert_ok!(rgate.wait(Some(&sgate)));
        assert_eq!(is.label(), 0x1234);
    }

    {
        let is = assert_ok!(rgate.wait(Some(&sgate)));
        assert_eq!(is.label(), 0x1234);
    }
}

fn send_reply() {
    let reply_gate = RecvGate::def();
    let mut rgate = assert_ok!(RecvGate::new(util::next_log2(64), util::next_log2(64)));
    let sgate = assert_ok!(SendGate::new_with(
        SGateArgs::new(&rgate).credits(64).label(0x1234)
    ));
    assert!(sgate.ep().is_none());
    assert_ok!(rgate.activate());

    assert_ok!(send_vmsg!(&sgate, &reply_gate, 0x123, 12, "test"));

    // sgate -> rgate
    {
        let mut msg = assert_ok!(recv_msg(&rgate));
        let (i1, i2, s): (i32, i32, String) = (msg.pop(), msg.pop(), msg.pop());
        assert_eq!(i1, 0x123);
        assert_eq!(i2, 12);
        assert_eq!(s, "test");

        assert_ok!(reply_vmsg!(msg, 44, 3));
    }

    // rgate -> reply_gate
    {
        let mut reply = assert_ok!(recv_msg_from(&reply_gate, Some(&sgate)));
        let (i1, i2): (i32, i32) = (reply.pop(), reply.pop());
        assert_eq!(i1, 44);
        assert_eq!(i2, 3);
    }
}
