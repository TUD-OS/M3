/*
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#pragma once

#include <m3/Common.h>
#include <m3/util/SList.h>
#include <test/Testable.h>
#include <test/TestCase.h>

namespace test {

class TestSuite : public m3::SListItem, public Testable {
public:
	explicit TestSuite(const m3::String& name)
		: m3::SListItem(), Testable(name), _cases() {
	}
	~TestSuite() {
		for(auto it = _cases.begin(); it != _cases.end(); ) {
			auto old = it++;
			delete &*old;
		}
	}

	void add(TestCase* tc) {
		_cases.append(tc);
	}
	void run();

private:
	m3::SList<TestCase> _cases;
};

}
