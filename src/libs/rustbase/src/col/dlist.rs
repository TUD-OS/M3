use boxed::Box;
use core::marker::PhantomData;
use core::ptr::Shared;
use core::fmt;

struct Node<T> {
    next: Option<Shared<Node<T>>>,
    prev: Option<Shared<Node<T>>>,
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

pub struct Iter<'a, T: 'a> {
    head: Option<Shared<Node<T>>>,
    marker: PhantomData<&'a Node<T>>,
}

impl<'a, T> Iterator for Iter<'a, T> {
    type Item = &'a T;

    fn next(&mut self) -> Option<&'a T> {
        self.head.map(|node| unsafe {
            let node = &*node.as_ptr();
            self.head = node.next;
            &node.data
        })
    }
}

pub struct IterMut<'a, T: 'a> {
    list: &'a mut DList<T>,
    head: Option<Shared<Node<T>>>,
}

impl<'a, T> Iterator for IterMut<'a, T> {
    type Item = &'a mut T;

    fn next(&mut self) -> Option<&'a mut T> {
        self.head.map(|node| unsafe {
            let node = &mut *node.as_ptr();
            self.head = node.next;
            &mut node.data
        })
    }
}

impl<'a, T> IterMut<'a, T> {
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

    ///
    /// init state: 1 3 4 5
    ///               ^
    /// after ins : 1 2 3 4 5
    ///                 ^
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

    ///
    /// init state: 1 2 4 5
    ///               ^
    /// after ins : 1 2 3 4 5
    ///                 ^
    pub fn insert_after(&mut self, data: T) {
        match self.head {
            None        => self.list.push_back(data),
            Some(head)  => self.insert_before_node(head, data),
        }
    }

    fn insert_before_node(&mut self, mut node: Shared<Node<T>>, data: T) {
        unsafe {
            let mut prev = match node.as_ref().prev {
                None => return self.list.push_front(data),
                Some(prev) => prev,
            };

            let new = Some(Shared::from(Box::into_unique(Box::new(Node {
                next: Some(node),
                prev: Some(prev),
                data,
            }))));

            prev.as_mut().next = new;
            node.as_mut().prev = new;

            self.list.len += 1;
        }
    }

    ///
    /// init state: 1 2 3 4 5
    ///               ^
    /// after rem : 1 3 4 5
    ///             ^
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

pub struct IntoIter<T> {
    list: DList<T>,
}
pub struct DList<T> {
    head: Option<Shared<Node<T>>>,
    tail: Option<Shared<Node<T>>>,
    len: usize,
    marker: PhantomData<Box<Node<T>>>,
}

impl<T> DList<T> {
    pub fn new() -> Self {
        DList {
            head: None,
            tail: None,
            len: 0,
            marker: PhantomData,
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
            self.head.map(|n| &(*n.as_ptr()).data)
        }
    }
    pub fn front_mut(&mut self) -> Option<&mut T> {
        unsafe {
            self.head.map(|n| &mut (*n.as_ptr()).data)
        }
    }

    pub fn back(&self) -> Option<&T> {
        unsafe {
            self.tail.map(|n| &(*n.as_ptr()).data)
        }
    }
    pub fn back_mut(&mut self) -> Option<&mut T> {
        unsafe {
            self.tail.map(|n| &mut (*n.as_ptr()).data)
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

    pub fn push_front(&mut self, data: T) {
        unsafe {
            let mut node = Box::new(Node::new(data));
            node.next = self.head;
            node.prev = None;
            let node = Some(Shared::from(Box::into_unique(node)));

            match self.head {
                None => self.tail = node,
                Some(mut head) => head.as_mut().prev = node,
            }

            self.head = node;
            self.len += 1;
        }
    }

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

    pub fn push_back(&mut self, data: T) {
        unsafe {
            let mut node = Box::new(Node::new(data));
            node.next = None;
            node.prev = self.tail;
            let node = Some(Shared::from(Box::into_unique(node)));

            match self.tail {
                None            => self.head = node,
                Some(mut tail)  => tail.as_mut().next = node,
            }

            self.tail = node;
            self.len += 1;
        }
    }

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

impl<T> Iterator for IntoIter<T> {
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
    type IntoIter = IntoIter<T>;

    fn into_iter(self) -> IntoIter<T> {
        IntoIter { list: self }
    }
}

impl<'a, T> IntoIterator for &'a DList<T> {
    type Item = &'a T;
    type IntoIter = Iter<'a, T>;

    fn into_iter(self) -> Iter<'a, T> {
        self.iter()
    }
}

impl<'a, T> IntoIterator for &'a mut DList<T> {
    type Item = &'a mut T;
    type IntoIter = IterMut<'a, T>;

    fn into_iter(self) -> IterMut<'a, T> {
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
