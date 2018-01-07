use cell::StaticCell;
use kif::PEDesc;

pub struct EnvData {
    pub pe_id: u64,
    pub pe_desc: u32,
    pub argc: u32,
    pub argv: u64,
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
}

static ENV_DATA: StaticCell<Option<EnvData>> = StaticCell::new(None);

pub fn get() -> &'static mut EnvData {
    ENV_DATA.get_mut().as_mut().unwrap()
}

pub fn set(data: EnvData) {
    ENV_DATA.set(Some(data));
}
