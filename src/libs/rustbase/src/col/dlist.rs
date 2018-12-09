/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
 *
 * M3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * M3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

use boxed::Box;
use core::marker::PhantomData;
use core::ptr::NonNull;
use core::fmt;

struct Node<T> {
    next: Option<NonNull<Node<T>>>,
    prev: Option<NonNull<Node<T>>>,
    data: T,
}

impl<T> Node<T> {
    fn new(data: T) -> Self {
        Node {
            next: None,
            prev: None,
            data: data,
        }
    }

    fn into_data(self: Box<Self>) -> T {
        self.data
    }
}

/// The iterator for `DList`
pub struct DListIter<'a, T: 'a> {
    head: Option<NonNull<Node<T>>>,
    marker: PhantomData<&'a Node<T>>,
}

impl<'a, T> Iterator for DListIter<'a, T> {
    type Item = &'a T;

    fn next(&mut self) -> Option<&'a T> {
        self.head.map(|node| unsafe {
            let node = &*node.as_ptr();
            self.head = node.next;
            &node.data
        })
    }
}

/// The mutable iterator for `DList`
pub struct DListIterMut<'a, T: 'a> {
    list: &'a mut DList<T>,
    head: Option<NonNull<Node<T>>>,
}

impl<'a, T> Iterator for DListIterMut<'a, T> {
    type Item = &'a mut T;

    fn next(&mut self) -> Option<&'a mut T> {
        self.head.map(|node| unsafe {
            let node = &mut *node.as_ptr();
            self.head = node.next;
            &mut node.data
        })
    }
}

impl<'a, T> DListIterMut<'a, T> {
    /// Returns a mutable reference to the previous element
    pub fn peek_prev(&mut self) -> Option<&mut T> {
        unsafe {
            let cur = match self.head {
                None            => self.list.tail,
                Some(mut head)  => head.as_mut().prev,
            };
            cur.and_then(|mut p| { p.as_mut().prev })
               .map(|pp| &mut (*pp.as_ptr()).data)
        }
    }

    /// Inserts the given element before the current element
    ///
    /// # Examples
    ///
    /// ```
    /// before insert: 1 3 4 5
    ///                  ^
    /// after insert : 1 2 3 4 5
    ///                    ^
    /// ```
    pub fn insert_before(&mut self, data: T) {
        if self.list.len < 2 {
            return self.list.push_front(data);
        }

        match self.head {
            None => {
                let tail = self.list.tail.unwrap();
                self.insert_before_node(tail, data)
            },
            Some(mut head) => unsafe {
                match head.as_mut().prev {
                    None        => self.list.push_front(data),
                    Some(phead) => self.insert_before_node(phead, data),
                }
            },
        }
    }

    /// Inserts the given element after the current element
    ///
    /// Note that the element will not be visible during the iteration, because the iterator will
    /// return the following element.
    ///
    /// # Examples
    ///
    /// ```
    /// before insert: 1 2 4 5
    ///                  ^
    /// after insert : 1 2 3 4 5
    ///                    ^
    /// ```
    pub fn insert_after(&mut self, data: T) {
        match self.head {
            None        => self.list.push_back(data),
            Some(head)  => self.insert_before_node(head, data),
        }
    }

    fn insert_before_node(&mut self, mut node: NonNull<Node<T>>, data: T) {
        unsafe {
            let mut prev = match node.as_ref().prev {
                None => return self.list.push_front(data),
                Some(prev) => prev,
            };

            let new = Some(NonNull::from(Box::into_raw_non_null(Box::new(Node {
                next: Some(node),
                prev: Some(prev),
                data,
            }))));

            prev.as_mut().next = new;
            node.as_mut().prev = new;

            self.list.len += 1;
        }
    }

    /// Removes the current element from the list and returns it
    ///
    /// # Examples
    ///
    /// ```
    /// before remove: 1 2 3 4 5
    ///                  ^
    /// after remove : 1 3 4 5
    ///                ^
    /// ```
    pub fn remove(&mut self) -> Option<T> {
        match self.head {
            // if we already walked at the list-end, remove the last element
            None            => return self.list.pop_back(),

            // otherwise, check if there is a current (=prev) element to remove
            Some(mut head)  => unsafe {
                head.as_ref().prev.map(|prev| {
                    let prev = Box::from_raw(prev.as_ptr());
                    match prev.prev {
                        None            => {
                            self.list.head = Some(head);
                            head.as_mut().prev = None;
                        },
                        Some(mut pp)    => {
                            pp.as_mut().next = Some(head);
                            head.as_mut().prev = Some(pp);
                        },
                    }

                    self.list.len -= 1;
                    prev.into_data()
                })
            },
        }
    }
}

/// An owning iterator over the elements of a DList
pub struct DListIntoIter<T> {
    list: DList<T>,
}

/// A doubly-linked list
///
/// In contrast to `col::LinkedList`, it supports the insertion (before and after the current
/// element) and removal of elements during iteration.
pub struct DList<T> {
    head: Option<NonNull<Node<T>>>,
    tail: Option<NonNull<Node<T>>>,
    len: usize,
    marker: PhantomData<Box<Node<T>>>,
}

impl<T> DList<T> {
    /// Creates an empty DList
    pub fn new() -> Self {
        DList {
            head: None,
            tail: None,
            len: 0,
            marker: PhantomData,
        }
    }

    /// Returns the number of elements
    pub fn len(&self) -> usize {
        self.len
    }
    /// Returns true if the list is empty
    pub fn is_empty(&self) -> bool {
        self.head.is_none()
    }

    /// Removes all elements from the list
    pub fn clear(&mut self) {
        *self = Self::new();
    }

    /// Returns a reference to the first element
    pub fn front(&self) -> Option<&T> {
        unsafe {
            self.head.map(|n| &(*n.as_ptr()).data)
        }
    }
    /// Returns a mutable reference to the first element
    pub fn front_mut(&mut self) -> Option<&mut T> {
        unsafe {
            self.head.map(|n| &mut (*n.as_ptr()).data)
        }
    }

    /// Returns a reference to the last element
    pub fn back(&self) -> Option<&T> {
        unsafe {
            self.tail.map(|n| &(*n.as_ptr()).data)
        }
    }
    /// Returns a mutable reference to the last element
    pub fn back_mut(&mut self) -> Option<&mut T> {
        unsafe {
            self.tail.map(|n| &mut (*n.as_ptr()).data)
        }
    }

    /// Returns an iterator for the list
    pub fn iter<'a>(&'a self) -> DListIter<'a, T> {
        DListIter {
            head: self.head,
            marker: PhantomData,
        }
    }

    /// Returns a mutable iterator for the list
    pub fn iter_mut<'a>(&'a mut self) -> DListIterMut<'a, T> {
        DListIterMut {
            head: self.head,
            list: self,
        }
    }

    /// Inserts the given element at the front of the list
    pub fn push_front(&mut self, data: T) {
        unsafe {
            let mut node = Box::new(Node::new(data));
            node.next = self.head;
            node.prev = None;
            let node = Some(NonNull::from(Box::into_raw_non_null(node)));

            match self.head {
                None => self.tail = node,
                Some(mut head) => head.as_mut().prev = node,
            }

            self.head = node;
            self.len += 1;
        }
    }

    /// Removes the first element of the list and returns it
    pub fn pop_front(&mut self) -> Option<T> {
        self.head.map(|node| unsafe {
            let node = Box::from_raw(node.as_ptr());
            self.head = node.next;

            match self.head {
                None => self.tail = None,
                Some(mut head) => head.as_mut().prev = None,
            }

            self.len -= 1;
            node.into_data()
        })
    }

    /// Inserts the given element at the end of the list
    pub fn push_back(&mut self, data: T) {
        unsafe {
            let mut node = Box::new(Node::new(data));
            node.next = None;
            node.prev = self.tail;
            let node = Some(NonNull::from(Box::into_raw_non_null(node)));

            match self.tail {
                None            => self.head = node,
                Some(mut tail)  => tail.as_mut().next = node,
            }

            self.tail = node;
            self.len += 1;
        }
    }

    /// Removes the last element of the list and returns it
    pub fn pop_back(&mut self) -> Option<T> {
        self.tail.map(|node| unsafe {
            let node = Box::from_raw(node.as_ptr());
            self.tail = node.prev;

            match self.tail {
                None => self.head = None,
                Some(mut tail) => tail.as_mut().next = None,
            }

            self.len -= 1;
            node.into_data()
        })
    }
}

impl<T> Drop for DList<T> {
    fn drop(&mut self) {
        while let Some(_) = self.pop_front() {
        }
    }
}

impl<T> Default for DList<T> {
    fn default() -> Self {
        Self::new()
    }
}

impl<T: fmt::Debug> fmt::Debug for DList<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_list().entries(self).finish()
    }
}

impl<T> Iterator for DListIntoIter<T> {
    type Item = T;

    fn next(&mut self) -> Option<T> {
        self.list.pop_front()
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.list.len, Some(self.list.len))
    }
}

impl<T> IntoIterator for DList<T> {
    type Item = T;
    type IntoIter = DListIntoIter<T>;

    fn into_iter(self) -> DListIntoIter<T> {
        DListIntoIter { list: self }
    }
}

impl<'a, T> IntoIterator for &'a DList<T> {
    type Item = &'a T;
    type IntoIter = DListIter<'a, T>;

    fn into_iter(self) -> DListIter<'a, T> {
        self.iter()
    }
}

impl<'a, T> IntoIterator for &'a mut DList<T> {
    type Item = &'a mut T;
    type IntoIter = DListIterMut<'a, T>;

    fn into_iter(self) -> DListIterMut<'a, T> {
        self.iter_mut()
    }
}

impl<T: PartialEq> PartialEq for DList<T> {
    fn eq(&self, other: &Self) -> bool {
        self.len() == other.len() && self.iter().eq(other)
    }

    fn ne(&self, other: &Self) -> bool {
        self.len() != other.len() || self.iter().ne(other)
    }
}

impl<T: Eq> Eq for DList<T> {}
