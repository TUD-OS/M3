use col::BTreeMap;

pub type SessId = u64;

pub struct SessionContainer<S> {
    con: BTreeMap<SessId, S>,
    next_id: SessId,
}

impl<S> SessionContainer<S> {
    pub fn new() -> Self {
        SessionContainer {
            con: BTreeMap::new(),
            next_id: 0,
        }
    }

    pub fn next_id(&self) -> SessId {
        self.next_id
    }

    pub fn get(&self, sid: SessId) -> Option<&S> {
        self.con.get(&sid)
    }

    pub fn add(&mut self, sess: S) -> SessId {
        self.con.insert(self.next_id, sess);
        self.next_id += 1;
        self.next_id - 1
    }

    pub fn remove(&mut self, sid: SessId) {
        self.con.remove(&sid);
    }
}
