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

#include <m3/arch/host/Backtrace.h>
#include <m3/util/Util.h>
#include <m3/Log.h>
#include <m3/Config.h>
#include <csignal>
#include <cstdio>
#include <ostream>
#include <algorithm>
#include <execinfo.h>
#include <unistd.h>

#if !defined(NDEBUG)
#   define SUFFIX   "debug"
#else
#   define SUFFIX   "release"
#endif

namespace m3 {

OStream &operator<<(OStream &os, const Backtrace &bt) {
    // to prevent a fork-bomb, better disable the hijacking of functions ;)
    bool in_recursion = getenv("M3_HIJACK") != nullptr;
    setenv("M3_HIJACK", "0", 1);
    size_t count = bt._count;
    char **strs = backtrace_symbols(bt._trace, count);
    if(strs == nullptr)
        PANIC("backtrace_symbols");
    os << "Backtrace:\n";
    for(size_t i = 0; i < count; ++i) {
        os << ">> " << i << ": " << strs[i];
        if(!in_recursion) {
            char cmd[256];
            if(strncmp(Config::executable_path(), "/bin/", 5) == 0) {
                snprintf(cmd, sizeof(cmd), "addr2line %p -e build/host-sim-%s%s",
                    bt._trace[i], SUFFIX, Config::executable_path());
            }
            else
                snprintf(cmd, sizeof(cmd), "addr2line %p -e %s", bt._trace[i], Config::executable_path());
            FILE *f = popen(cmd, "r");
            if(f) {
                os << " (";
                char c;
                while((c = fgetc(f)) != '\n' && c != EOF)
                    os << c;
                os << ")";
                pclose(f);
            }
        }
        os << "\n";
    }
    free(strs);
    if(!in_recursion)
        unsetenv("M3_HIJACK");
    return os;
}

}
