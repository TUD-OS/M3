use core::intrinsics;
use kif::PEDesc;

#[derive(Default, Copy, Clone)]
#[repr(C, packed)]
pub struct EnvData {
    pe_id: u64,
    pe_desc: u32,
    argc: u32,
    argv: u64,
    sp: u64,
    entry: u64,
    heap_size: u64,
    _kenv: u64,
}

impl EnvData {
    pub fn pe_id(&self) -> u64 {
        self.pe_id
    }
    pub fn set_peid(&mut self, id: u64) {
        self.pe_id = id;
    }

    pub fn pe_desc(&self) -> PEDesc {
        PEDesc::new_from(self.pe_desc)
    }
    pub fn set_pedesc(&mut self, pe: &PEDesc) {
        self.pe_desc = pe.value();
    }

    pub fn argc(&self) -> usize {
        self.argc as usize
    }
    pub fn argv(&self) -> *const *const i8 {
        self.argv as *const *const i8
    }
    pub fn set_argc(&mut self, argc: usize) {
        self.argc = argc as u32;
    }
    pub fn set_argv(&mut self, argv: usize) {
        self.argv = argv as u64;
    }

    pub fn sp(&self) -> usize {
        self.sp as usize
    }
    pub fn set_sp(&mut self, sp: usize) {
        self.sp = sp as u64;
    }

    pub fn set_entry(&mut self, entry: usize) {
        self.entry = entry as u64;
    }

    pub fn heap_size(&self) -> usize {
        self.heap_size as usize
    }
    pub fn set_heap_size(&mut self, size: usize) {
        self.heap_size = size as u64;
    }
}

pub fn get() -> &'static mut EnvData {
    unsafe {
        intrinsics::transmute(0x6000 as u64)
    }
}
