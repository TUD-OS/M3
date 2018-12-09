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
