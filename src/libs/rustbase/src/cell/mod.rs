use core::marker::Sync;
use core::mem;

pub use core::cell::{Cell, Ref, RefCell, RefMut, UnsafeCell};

pub struct MutCell<T: Sized> {
    inner: UnsafeCell<T>,
}

unsafe impl<T: Sized> Sync for MutCell<T> {}

impl<T: Sized> MutCell<T> {
    pub const fn new(val: T) -> Self {
        MutCell {
            inner: UnsafeCell::new(val),
        }
    }

    pub fn get(&self) -> &T {
        unsafe {
            &*self.inner.get()
        }
    }
    pub fn get_mut(&self) -> &mut T {
        unsafe {
            &mut *self.inner.get()
        }
    }

    pub fn set(&self, val: T) {
        mem::replace(self.get_mut(), val);
    }
}
