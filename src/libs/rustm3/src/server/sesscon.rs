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
