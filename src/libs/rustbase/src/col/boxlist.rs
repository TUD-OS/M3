use core::fmt;
use core::intrinsics;
use core::marker::PhantomData;
use core::ptr::Shared;

use boxed::Box;

pub type BoxRef<T> = Shared<T>;

pub trait BoxItem {
    type T : BoxItem;

    fn next(&self) -> Option<BoxRef<Self::T>>;
    fn set_next(&mut self, next: Option<BoxRef<Self::T>>);

    fn prev(&self) -> Option<BoxRef<Self::T>>;
    fn set_prev(&mut self, prev: Option<BoxRef<Self::T>>);
}

#[macro_export]
macro_rules! impl_boxitem {
    ($t:ty) => (
        impl $crate::col::BoxItem for $t {
            type T = $t;

            fn next(&self) -> Option<$crate::col::BoxRef<$t>>                { self.next }
            fn set_next(&mut self, next: Option<$crate::col::BoxRef<$t>>)    { self.next = next; }

            fn prev(&self) -> Option<$crate::col::BoxRef<$t>>                { self.prev }
            fn set_prev(&mut self, prev: Option<$crate::col::BoxRef<$t>>)    { self.prev = prev; }
        }
    )
}

pub struct Iter<'a, T: 'a> {
    head: Option<BoxRef<T>>,
    marker: PhantomData<&'a T>,
}

impl<'a, T: BoxItem> Iterator for Iter<'a, T> {
    type Item = &'a T;

    fn next(&mut self) -> Option<&'a T> {
        self.head.map(|item| unsafe {
            let item = &*item.as_ptr();
            self.head = intrinsics::transmute(item.next());
            item
        })
    }
}

pub struct IterMut<'a, T: 'a + BoxItem> {
    list: &'a mut BoxList<T>,
    head: Option<BoxRef<T>>,
}

impl<'a, T: BoxItem> Iterator for IterMut<'a, T> {
    type Item = &'a mut T;

    fn next(&mut self) -> Option<&'a mut T> {
        self.head.map(|item| unsafe {
            let item = &mut *item.as_ptr();
            self.head = intrinsics::transmute(item.next());
            item
        })
    }
}

impl<'a, T: BoxItem> IterMut<'a, T> {
    ///
    /// init state: 1 2 3 4 5
    ///               ^
    /// after rem : 1 3 4 5
    ///             ^
    pub fn remove(&mut self) -> Option<Box<T>> {
        match self.head {
            // if we already walked at the list-end, remove the last element
            None            => return self.list.pop_back(),

            // otherwise, check if there is a current (=prev) element to remove
            Some(mut head)  => unsafe {
                head.as_ref().prev().map(|prev| {
                    let prev = prev.as_ptr();
                    match (*prev).prev() {
                        None            => {
                            self.list.head = Some(head);
                            head.as_mut().set_prev(None);
                        },
                        Some(mut pp)    => {
                            pp.as_mut().set_next(Some(intrinsics::transmute(head)));
                            head.as_mut().set_prev(Some(intrinsics::transmute(pp)));
                        },
                    }

                    self.list.len -= 1;
                    Box::from_raw(intrinsics::transmute(prev))
                })
            },
        }
    }
}

pub struct IntoIter<T : BoxItem> {
    list: BoxList<T>,
}
pub struct BoxList<T : BoxItem> {
    head: Option<BoxRef<T>>,
    tail: Option<BoxRef<T>>,
    len: usize,
}

impl<T : BoxItem> BoxList<T> {
    pub fn new() -> Self {
        BoxList {
            head: None,
            tail: None,
            len: 0,
        }
    }

    pub fn len(&self) -> usize {
        self.len
    }
    pub fn is_empty(&self) -> bool {
        self.head.is_none()
    }

    pub fn clear(&mut self) {
        *self = Self::new();
    }

    pub fn front(&self) -> Option<&T> {
        unsafe {
            self.head.map(|n| &(*n.as_ptr()))
        }
    }
    pub fn front_mut(&mut self) -> Option<&mut T> {
        unsafe {
            self.head.map(|n| &mut (*n.as_ptr()))
        }
    }

    pub fn back(&self) -> Option<&T> {
        unsafe {
            self.tail.map(|n| &(*n.as_ptr()))
        }
    }
    pub fn back_mut(&mut self) -> Option<&mut T> {
        unsafe {
            self.tail.map(|n| &mut (*n.as_ptr()))
        }
    }

    pub fn iter<'a>(&'a self) -> Iter<'a, T> {
        Iter {
            head: self.head,
            marker: PhantomData,
        }
    }

    pub fn iter_mut<'a>(&'a mut self) -> IterMut<'a, T> {
        IterMut {
            head: self.head,
            list: self,
        }
    }

    pub fn push_front(&mut self, mut item: Box<T>) {
        unsafe {
            item.set_next(intrinsics::transmute(self.head));
            item.set_prev(None);

            let item_ptr = Some(Shared::new_unchecked(Box::into_raw(item)));

            match self.head {
                None => self.tail = item_ptr,
                Some(mut head) => head.as_mut().set_prev(intrinsics::transmute(item_ptr)),
            }

            self.head = item_ptr;
            self.len += 1;
        }
    }

    pub fn push_back(&mut self, mut item: Box<T>) {
        unsafe {
            item.set_next(None);
            item.set_prev(intrinsics::transmute(self.tail));

            let item_ptr = Some(Shared::new_unchecked(Box::into_raw(item)));

            match self.tail {
                None            => self.head = item_ptr,
                Some(mut tail)  => tail.as_mut().set_next(intrinsics::transmute(item_ptr)),
            }

            self.tail = item_ptr;
            self.len += 1;
        }
    }

    pub fn pop_front(&mut self) -> Option<Box<T>> {
        self.head.map(|item| unsafe {
            let item = item.as_ptr();
            self.head = intrinsics::transmute((*item).next());

            match self.head {
                None => self.tail = None,
                Some(mut head) => head.as_mut().set_prev(None),
            }

            self.len -= 1;
            Box::from_raw(item)
        })
    }

    pub fn pop_back(&mut self) -> Option<Box<T>> {
        self.tail.map(|item| unsafe {
            let item = item.as_ptr();
            self.tail = intrinsics::transmute((*item).prev());

            match self.tail {
                None => self.head = None,
                Some(mut tail) => tail.as_mut().set_next(None),
            }

            self.len -= 1;
            Box::from_raw(item)
        })
    }
}

impl<T : BoxItem> Drop for BoxList<T> {
    fn drop(&mut self) {
        while let Some(_) = self.pop_front() {
        }
    }
}

impl<T : BoxItem> Default for BoxList<T> {
    fn default() -> Self {
        Self::new()
    }
}

impl<T : BoxItem + fmt::Debug> fmt::Debug for BoxList<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_list().entries(self).finish()
    }
}

impl<T : BoxItem> Iterator for IntoIter<T> {
    type Item = Box<T>;

    fn next(&mut self) -> Option<Box<T>> {
        self.list.pop_front()
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.list.len, Some(self.list.len))
    }
}

impl<T : BoxItem> IntoIterator for BoxList<T> {
    type Item = Box<T>;
    type IntoIter = IntoIter<T>;

    fn into_iter(self) -> IntoIter<T> {
        IntoIter { list: self }
    }
}

impl<'a, T : BoxItem> IntoIterator for &'a BoxList<T> {
    type Item = &'a T;
    type IntoIter = Iter<'a, T>;

    fn into_iter(self) -> Iter<'a, T> {
        self.iter()
    }
}

impl<'a, T : BoxItem> IntoIterator for &'a mut BoxList<T> {
    type Item = &'a mut T;
    type IntoIter = IterMut<'a, T>;

    fn into_iter(self) -> IterMut<'a, T> {
        self.iter_mut()
    }
}

impl<T: BoxItem + PartialEq> PartialEq for BoxList<T> {
    fn eq(&self, other: &Self) -> bool {
        self.len() == other.len() && self.iter().eq(other)
    }

    fn ne(&self, other: &Self) -> bool {
        self.len() != other.len() || self.iter().ne(other)
    }
}

impl<T: BoxItem + Eq> Eq for BoxList<T> {}
