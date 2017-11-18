use boxed::Box;
use core::cmp::Ordering;
use core::fmt;
use core::mem;
use core::num::Wrapping;
use core::ptr::Shared;

struct Node<K, V> {
    left: Option<Shared<Node<K, V>>>,
    right: Option<Shared<Node<K, V>>>,
    prio: Wrapping<u32>,
    key: K,
    value: V,
}

impl<K : Copy + PartialOrd + Ord, V> Node<K, V> {
    fn new(key: K, value: V, prio: Wrapping<u32>) -> Self {
        Node {
            left: None,
            right: None,
            prio: prio,
            key: key,
            value: value,
        }
    }

    fn into_value(self: Box<Self>) -> V {
        self.value
    }
}

/// A treap is a combination of a binary tree and a heap. So the child-node on the left has a
/// smaller key than the parent and the child on the right has a bigger key.
/// Additionally the root-node has the smallest priority and it increases when walking towards the
/// leafs. The priority is "randomized" by fibonacci-hashing. This way, the tree is well balanced
/// in most cases.
///
/// The idea and parts of the implementation are taken from the MMIX simulator, written by
/// Donald Knuth (http://mmix.cs.hm.edu/)
#[derive(Default)]
pub struct Treap<K : Copy + PartialOrd + Ord, V> {
    root: Option<Shared<Node<K, V>>>,
    prio: Wrapping<u32>,
}

impl<K : Copy + PartialOrd + Ord, V> Treap<K, V> {
    pub fn new() -> Self {
        Treap {
            root: None,
            prio: Wrapping(314159265),
        }
    }

    pub fn is_empty(&self) -> bool {
        self.root.is_none()
    }

    pub fn clear(&mut self) {
        mem::replace(&mut self.root, None).map(|r| unsafe {
            Self::remove_rec(r);
            // destroy the node
            Box::from_raw(r.as_ptr());
        });

        self.prio = Wrapping(314159265);
    }

    fn remove_rec(node: Shared<Node<K, V>>) {
        unsafe {
            (*node.as_ptr()).left.map(|l| {
                Self::remove_rec(l);
                Box::from_raw(l.as_ptr());
            });
            (*node.as_ptr()).right.map(|r| {
                Self::remove_rec(r);
                Box::from_raw(r.as_ptr());
            });
        }
    }

    pub fn get<P: Fn(&K) -> Ordering>(&self, pred: P) -> Option<&V> {
        self.get_node(pred).map(|n| unsafe {
            &(*n.as_ptr()).value
        })
    }

    pub fn get_mut<P: Fn(&K) -> Ordering>(&mut self, pred: P) -> Option<&mut V> {
        self.get_node(pred).map(|n| unsafe {
            &mut (*n.as_ptr()).value
        })
    }

    fn get_node<P: Fn(&K) -> Ordering>(&self, pred: P) -> Option<Shared<Node<K, V>>> {
        let mut node = self.root;
        loop {
            match node {
                Some(n) => unsafe {
                    match pred(&(*n.as_ptr()).key) {
                        Ordering::Less      => node = (*n.as_ptr()).left,
                        Ordering::Greater   => node = (*n.as_ptr()).right,
                        Ordering::Equal     => return Some(n),
                    }
                },
                None    => return None,
            }
        }
    }

    pub fn insert(&mut self, key: K, value: V) -> &mut V {
        unsafe {
            let mut q = &mut self.root;
            loop {
                match *q {
                    None                                        => break,
                    Some(n) if (*n.as_ptr()).prio >= self.prio  => break,
                    Some(n) => {
                        if key < (*n.as_ptr()).key {
                            q = &mut (*n.as_ptr()).left;
                        }
                        else {
                            q = &mut (*n.as_ptr()).right;
                        }
                    },
                }
            }

            let mut prev = *q;
            let mut node = Node::new(key, value, self.prio);

            {
                // At this point we want to split the binary search tree p into two parts based on the
                // given key, forming the left and right subtrees of the new node q. The effect will be
                // as if key had been inserted before all of p’s nodes.
                let mut l = &mut node.left;
                let mut r = &mut node.right;
                loop {
                    match prev {
                        None                                => break,
                        Some(p) if key < (*p.as_ptr()).key  => {
                            *r = Some(p);
                            r = &mut (*p.as_ptr()).left;
                            prev = *r;
                        },
                        Some(p)                             => {
                            *l = Some(p);
                            l = &mut (*p.as_ptr()).right;
                            prev = *l;
                        },
                    }
                }
                *l = None;
                *r = None;
            }

            *q = Some(Shared::from(Box::into_unique(Box::new(node))));

            // fibonacci hashing to spread the priorities very even in the 32-bit room
            self.prio += Wrapping(0x9e3779b9);    // floor(2^32 / phi), with phi = golden ratio

            &mut (*q.unwrap().as_ptr()).value
        }
    }

    pub fn remove_root(&mut self) -> Option<V> {
        self.root.map(|r| {
            Self::remove_from(&mut self.root, r);
            unsafe {
                Box::from_raw(r.as_ptr()).into_value()
            }
        })
    }

    pub fn remove<P: Fn(&K) -> Ordering>(&mut self, pred: P) -> Option<V> {
        let mut p = &mut self.root;
        loop {
            match *p {
                Some(n) => unsafe {
                    match pred(&(*n.as_ptr()).key) {
                        Ordering::Less      => p = &mut (*n.as_ptr()).left,
                        Ordering::Greater   => p = &mut (*n.as_ptr()).right,
                        Ordering::Equal     => break,
                    }
                },
                None    => return None,
            }
        }

        let node = (*p).unwrap();
        Self::remove_from(p, node);
        unsafe {
            Some(Box::from_raw(node.as_ptr()).into_value())
        }
    }

    fn remove_from(p: &mut Option<Shared<Node<K, V>>>, node: Shared<Node<K, V>>) {
        unsafe {
            match ((*node.as_ptr()).left, (*node.as_ptr()).right) {
                // two childs
                (Some(l), Some(r)) => {
                    // rotate with left
                    if (*l.as_ptr()).prio < (*r.as_ptr()).prio {
                        (*node.as_ptr()).left = (*l.as_ptr()).right;
                        (*l.as_ptr()).right = Some(node);
                        *p = Some(l);
                        Self::remove_from(&mut (*l.as_ptr()).right, node);
                    }
                    // rotate with right
                    else {
                        (*node.as_ptr()).right = (*r.as_ptr()).left;
                        (*r.as_ptr()).left = Some(node);
                        *p = Some(r);
                        Self::remove_from(&mut (*r.as_ptr()).left, node);
                    }
                },
                // one child: replace us with our child
                (Some(l), None) => {
                    *p = Some(l);
                },
                (None, Some(r)) => {
                    *p = Some(r);
                },
                // no child: simply remove us from parent
                (None, None) => {
                    *p = None;
                },
            }
        }
    }
}

impl<K : Copy + PartialOrd + Ord, V> Drop for Treap<K, V> {
    fn drop(&mut self) {
        self.clear();
    }
}

fn print_rec<K, V>(node: Shared<Node<K, V>>, f: &mut fmt::Formatter) -> fmt::Result
                   where K : Copy + PartialOrd + Ord + fmt::Debug, V: fmt::Debug {
    let node_ptr = node.as_ptr();
    unsafe {
        write!(f, "  {:?} -> {:?}\n", (*node_ptr).key, (*node_ptr).value)?;
        if let Some(l) = (*node_ptr).left {
            print_rec(l, f)?;
        }
        if let Some(r) = (*node_ptr).right {
            print_rec(r, f)?;
        }
        Ok(())
    }
}

impl<K : Copy + PartialOrd + Ord + fmt::Debug, V: fmt::Debug> fmt::Debug for Treap<K, V> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self.root {
            Some(r) => print_rec(r, f),
            None    => Ok(())
        }
    }
}
