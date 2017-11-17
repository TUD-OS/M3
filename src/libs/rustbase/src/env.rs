use arch;
use boxed::{Box, FnBox};
use core::iter;
use core::mem;
use util;

pub struct Closure {
    func: Option<Box<FnBox() -> i32 + Send>>,
}

impl Closure {
    pub fn new<F>(func: Box<F>) -> Self
                  where F: FnBox() -> i32, F: Send + 'static {
        Closure {
            func: Some(func),
        }
    }

    pub fn call(&mut self) -> i32 {
        let old = mem::replace(&mut self.func, None);
        old.unwrap().call_box(())
    }
}

pub struct Args {
    pos: isize,
}

impl Args {
    fn arg(&self, idx: isize) -> &'static str {
        unsafe {
            let args = arch::envdata::get().argv as *const *const i8;
            let arg = *args.offset(idx);
            util::cstr_to_str(arg)
        }
    }

    pub fn len(&self) -> usize {
        arch::envdata::get().argc as usize
    }
}

impl iter::Iterator for Args {
    type Item = &'static str;

    fn next(&mut self) -> Option<Self::Item> {
        if self.pos < arch::envdata::get().argc as isize {
            let arg = self.arg(self.pos);
            self.pos += 1;
            Some(arg)
        }
        else {
            None
        }
    }
}

pub fn args() -> Args {
    Args {
        pos: 0,
    }
}
