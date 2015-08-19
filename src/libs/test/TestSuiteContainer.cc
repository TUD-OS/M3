/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <test/TestSuiteContainer.h>
#include <m3/Log.h>

namespace test {

int TestSuiteContainer::run() {
	size_t suites_succ = 0;
	size_t suites_failed = 0;
	size_t cases_succ = 0;
	size_t cases_failed = 0;
	for(auto &s : _suites) {
		LOG(DEF, "\033[1mTestsuite \"" << s.get_name() << "\"...\033[0m");

		s.run();

        if(s.get_failed() == 0)
            suites_succ++;
        else
            suites_failed++;

		LOG(DEF, "  " << (s.get_failed() == 0 ? "\033[0;32m" : "\033[0;31m") << s.get_succeeded()
		        << "\033[0m of " << (s.get_failed() + s.get_succeeded()) << " testcases successfull");
		cases_succ += s.get_succeeded();
		cases_failed += s.get_failed();
	}

	LOG(DEF, "");
	LOG(DEF, "\033[1mAll tests done:\033[0m");
	LOG(DEF, (suites_succ == _suites.length() ? "\033[0;32m" : "\033[0;31m")
	        << suites_succ << "\033[0m of " << _suites.length() << " testsuites successfull");
	LOG(DEF, (cases_failed == 0 ? "\033[0;32m" : "\033[0;31m") << cases_succ << "\033[0m of "
	        << (cases_succ + cases_failed) << " testcases successfull");
	return ((suites_succ + suites_failed) << 16) | suites_succ;
}

}
