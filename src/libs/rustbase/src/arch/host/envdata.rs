use kif::PEDesc;

pub struct EnvData {
    pe_id: u64,
    pe_desc: u32,
    argc: u32,
    argv: u64,
}

impl EnvData {
    pub fn new(pe_id: u64, pe_desc: PEDesc, argc: i32, argv: *const *const i8) -> Self {
        EnvData {
            pe_id: pe_id,
            pe_desc: pe_desc.value(),
            argc: argc as u32,
            argv: argv as u64,
        }
    }

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
}

static mut ENV_DATA: Option<EnvData> = None;

pub fn get() -> &'static mut EnvData {
    unsafe {
        ENV_DATA.as_mut().unwrap()
    }
}

pub fn set(data: EnvData) {
    unsafe {
        ENV_DATA = Some(data)
    }
}
