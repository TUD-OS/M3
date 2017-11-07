pub trait Tester {
    fn run_suite(&mut self, name: &str, f: &Fn(&mut Tester));
    fn run_test(&mut self, name: &str, f: &Fn());
}

#[macro_export]
macro_rules! run_suite {
    ($t:expr, $func:path) => (
        $t.run_suite(stringify!($func), &$func)
    );
}

#[macro_export]
macro_rules! run_test {
    ($t:expr, $func:path) => (
        $t.run_test(stringify!($func), &$func)
    );
}

#[macro_export]
macro_rules! assert_ok {
    ($res:expr) => ({
        match $res {
            Ok(r)   => r,
            Err(e)  => panic!("received error: {:?}", e)
        }
    });
}

#[macro_export]
macro_rules! assert_err {
    ($res:expr, $err:expr) => ({
        match $res {
            Ok(r)                           => panic!("received okay: {:?}", r),
            Err(ref e) if e.code() != $err  => panic!("received error {:?}, expected {:?}", e, $err),
            Err(_)                          => ()
        }
    });
}
