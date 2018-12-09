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

//! Contains the serializing basics, which is used for IPC

use col::String;

/// For types that can be marshalled into a [`Sink`](trait.Sink.html).
pub trait Marshallable {
    /// Writes this object into the given sink
    fn marshall(&self, s: &mut Sink);
}

/// For types that can be unmarshalled from a [`Source`](trait.Source.html).
pub trait Unmarshallable : Sized {
    /// Reads an object from the given source and returns it
    fn unmarshall(s: &mut Source) -> Self;
}

/// A sink allows to push objects into it
pub trait Sink {
    /// Returns the number of bytes in the sink
    fn size(&self) -> usize;
    /// Returns the content as a u64-slice
    fn words(&self) -> &[u64];
    /// Pushes the given marshallable object into this sink
    fn push(&mut self, item: &Marshallable);
    /// Pushes the given word into this sink
    fn push_word(&mut self, word: u64);
    /// Pushes the given string into this sink
    fn push_str(&mut self, b: &str);
}

/// A source allows to pop objects from it
pub trait Source {
    /// Pops a word from this source
    fn pop_word(&mut self) -> u64;
    /// Pops a string from this source
    fn pop_str(&mut self) -> String;
}

macro_rules! impl_xfer_prim {
    ( $t:ty ) => (
        impl Marshallable for $t {
            fn marshall(&self, s: &mut Sink) {
                s.push_word(*self as u64);
            }
        }
        impl Unmarshallable for $t {
            fn unmarshall(s: &mut Source) -> Self {
                s.pop_word() as $t
            }
        }
    )
}

impl_xfer_prim!(u8);
impl_xfer_prim!(i8);
impl_xfer_prim!(u16);
impl_xfer_prim!(i16);
impl_xfer_prim!(u32);
impl_xfer_prim!(i32);
impl_xfer_prim!(u64);
impl_xfer_prim!(i64);
impl_xfer_prim!(usize);
impl_xfer_prim!(isize);

impl Marshallable for bool {
    fn marshall(&self, s: &mut Sink) {
        s.push_word(*self as u64);
    }
}
impl Unmarshallable for bool {
    fn unmarshall(s: &mut Source) -> Self {
        s.pop_word() == 1
    }
}

impl<'a> Marshallable for &'a str {
    fn marshall(&self, s: &mut Sink) {
        s.push_str(self);
    }
}

impl Marshallable for String {
    fn marshall(&self, s: &mut Sink) {
        s.push_str(self.as_str());
    }
}
impl Unmarshallable for String {
    fn unmarshall(s: &mut Source) -> Self {
        s.pop_str()
    }
}
