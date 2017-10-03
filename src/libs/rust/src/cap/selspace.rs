use cap::Selector;

pub struct SelSpace {
    next: Selector,
}

static mut SEL_SPACE: SelSpace = SelSpace::new();

impl SelSpace {
    const fn new() -> SelSpace {
        SelSpace {
            // 0 and 1 are reserved for VPE cap and mem cap
            next: 2,
        }
    }

    pub fn get() -> &'static mut SelSpace {
        unsafe {
            &mut SEL_SPACE
        }
    }

    pub fn alloc(&mut self) -> Selector {
        self.next += 1;
        self.next - 1
    }

    pub fn free(&mut self, _sel: Selector) {
    }
}
