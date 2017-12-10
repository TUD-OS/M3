//! Contains unittest utilities

/// Runs the tests
pub trait Tester {
    /// Runs the given test suite
    fn run_suite(&mut self, name: &str, f: &Fn(&mut Tester));
    /// Runs the given test
    fn run_test(&mut self, name: &str, f: &Fn());
}

/// Convenience macro that calls `Tester::run_suite` and uses the function name as suite name
#[macro_export]
macro_rules! run_suite {
    ($t:expr, $func:path) => (
        $t.run_suite(stringify!($func), &$func)
    );
}

/// Convenience macro that calls `Tester::run_test` and uses the function name as test name
#[macro_export]
macro_rules! run_test {
    ($t:expr, $func:path) => (
        $t.run_test(stringify!($func), &$func)
    );
}

/// Convenience macro that tests whether the argument is `Ok`, returns the inner value if so, and
/// panics otherwise
#[macro_export]
macro_rules! assert_ok {
    ($res:expr) => ({
        match $res {
            Ok(r)   => r,
            Err(e)  => panic!("received error: {:?}", e)
        }
    });
}

/// Convenience macro that tests whether the argument is `Err` with the given error code
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
