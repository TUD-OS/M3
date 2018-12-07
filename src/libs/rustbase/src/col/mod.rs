//! Contains various collections

mod dlist;
mod boxlist;
mod treap;

pub use self::dlist::{DList, DListIter, DListIterMut};
pub use self::boxlist::{BoxItem, BoxList, BoxListIter, BoxListIterMut, BoxRef};
pub use self::treap::{KeyOrd, Treap};

pub use ::alloc::collections::binary_heap::BinaryHeap;
pub use ::alloc::collections::btree_map::BTreeMap;
pub use ::alloc::collections::btree_set::BTreeSet;
pub use ::alloc::collections::linked_list::LinkedList;
pub use ::alloc::collections::vec_deque::VecDeque;

pub use ::alloc::string::{String, ToString};
pub use ::alloc::vec::Vec;
