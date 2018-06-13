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

#include <fs/internal.h>

/*
 * *************************************************************************
 */

void Platform::init(int argc, const char * const argv[], const char *loadgen) {

    // nothing to do here
    (void)argc; (void)argv; (void)loadgen;
}


void Platform::log(const char *msg) {

    std::cerr << msg << std::endl;
}

/*
 * *************************************************************************
 */

FILE *file;
m3::SuperBlock sb;

int main(int argc, char **argv) {
    if(argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <name>\n";
        exit(1);
    }

    const char *name = argv[1];

    TraceRecorder rec;

    try {
        rec.import();
        rec.print(name);
    }
    catch (Exception &e) {
        e.complain();
        return 1;
    }

    return 0;
}
