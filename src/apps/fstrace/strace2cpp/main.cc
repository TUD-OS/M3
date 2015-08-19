// vim:ft=cpp
/*
 * (c) 2007-2013 Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS, which is distributed under the terms of the
 * GNU General Public License 2. Please see the COPYING-GPL-2 file for details.
 */

#include "tracerecorder.h"
#include "exceptions.h"
#include "platform_common.h"

/*
 * *************************************************************************
 */

void Platform::init(int argc, const char * const argv[]) {

    // nothing to do here
    (void)argc; (void)argv;
}


void Platform::log(const std::string &msg) {

    std::cerr << msg << std::endl;
}

/*
 * *************************************************************************
 */

int main(int argc, char **argv) {

    (void)argc; (void)argv;

    TraceRecorder rec;

    try {
        rec.import();
        rec.print();
    }

    catch (Exception &e) {
        e.complain();
        return 1;
    }

    return 0;
}
