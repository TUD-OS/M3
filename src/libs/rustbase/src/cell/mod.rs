//! Shareable mutable containers

use core::fmt;
use core::marker::Sync;
use core::mem;
use core::ops::Deref;

pub use core::cell::{Cell, Ref, RefCell, RefMut, UnsafeCell};

/// A cell that allows to mutate a static immutable object in single threaded environments
///
/// Since M3 does not support multiple threads within one address space, mutable static objects
/// are perfectly fine. Thus, use this wrapper to convince rust of that.
pub struct StaticCell<T: Sized> {
    inner: UnsafeCell<T>,
}

unsafe impl<T: Sized> Sync for StaticCell<T> {}

impl<T: Sized> StaticCell<T> {
    /// Creates a new static cell with given value
    pub const fn new(val: T) -> Self {
        StaticCell {
            inner: UnsafeCell::new(val),
        }
    }

    /// Returns a reference to the inner value
    pub fn get(&self) -> &T {
        unsafe {
            &*self.inner.get()
        }
    }
    /// Returns a mutable reference to the inner value
    pub fn get_mut(&self) -> &mut T {
        unsafe {
            &mut *self.inner.get()
        }
    }

    /// Sets the inner value to `val` and returns the old value
    pub fn set(&self, val: T) -> T {
        mem::replace(self.get_mut(), val)
    }
}

impl<T: Sized> Deref for StaticCell<T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.get()
    }
}

impl<T: fmt::Debug> fmt::Debug for StaticCell<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.get().fmt(f)
    }
}
