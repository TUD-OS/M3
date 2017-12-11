//! Shareable mutable containers

use core::marker::Sync;
use core::mem;
use core::ops::Deref;

pub use core::cell::{Cell, Ref, RefCell, RefMut, UnsafeCell};

pub struct StaticCell<T: Sized> {
    inner: UnsafeCell<T>,
}

unsafe impl<T: Sized> Sync for StaticCell<T> {}

impl<T: Sized> StaticCell<T> {
    pub const fn new(val: T) -> Self {
        StaticCell {
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

impl<T: Sized> Deref for StaticCell<T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.get()
    }
}
