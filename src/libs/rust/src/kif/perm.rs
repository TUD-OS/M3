bitflags! {
    pub struct Perm : u8 {
        const R = 1;
        const W = 2;
        const X = 4;
        const RW = Self::R.bits | Self::W.bits;
        const RWX = Self::R.bits | Self::W.bits | Self::X.bits;
    }
}
