use m3::com::{RecvGate, RGateArgs};
use m3::errors::Error;
use m3::test;

pub fn run(t: &mut test::Tester) {
    run_test!(t, create);
}

fn create() {
    assert_err!(RecvGate::new(8, 9), Error::InvArgs);
    assert_err!(RecvGate::new_with(RGateArgs::new().sel(1)), Error::InvArgs);
}
