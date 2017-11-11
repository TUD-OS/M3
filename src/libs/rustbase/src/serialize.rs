use col::String;

pub trait Marshallable {
    fn marshall(&self, s: &mut Sink);
}

pub trait Unmarshallable : Sized {
    fn unmarshall(s: &mut Source) -> Self;
}

pub trait Sink {
    fn size(&self) -> usize;
    fn words(&self) -> &[u64];
    fn push(&mut self, item: &Marshallable);
    fn push_word(&mut self, word: u64);
    fn push_str(&mut self, b: &str);
}

pub trait Source {
    fn pop_word(&mut self) -> u64;
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
