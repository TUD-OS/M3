bitflags! {
    /// The permission bitmap that is used for memory and mapping capabilities.
    pub struct Perm : u8 {
        /// Read permission
        const R = 1;
        /// Write permission
        const W = 2;
        /// Execute permission
        const X = 4;
        /// Read + write permission
        const RW = Self::R.bits | Self::W.bits;
        /// Read + write + execute permission
        const RWX = Self::R.bits | Self::W.bits | Self::X.bits;
    }
}
