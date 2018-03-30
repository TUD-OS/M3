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

#include <base/Common.h>
#include <base/Panic.h>

#include <m3/session/M3FS.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/Dir.h>
#include <m3/vfs/VFS.h>

#include "common/traceplayer.h"
#include "platform.h"

using namespace m3;

static const size_t MAX_TMP_FILES   = 16;
static const bool VERBOSE           = 0;

static void remove_rec(const char *path) {
    if(VERBOSE) cout << "Unlinking " << path << "\n";
    if(VFS::unlink(path) == Errors::IS_DIR) {
        Dir::Entry e;
        char tmp[128];
        Dir dir(path);
        while(dir.readdir(e)) {
            if(strcmp(e.name, ".") == 0 || strcmp(e.name, "..") == 0)
                continue;

            OStringStream file(tmp, sizeof(tmp));
            file << path << "/" << e.name;
            remove_rec(file.str());
        }
        VFS::rmdir(path);
    }
}

static void cleanup() {
    Dir dir("/tmp");
    if(Errors::occurred())
        return;

    size_t x = 0;
    String *entries[MAX_TMP_FILES];

    if(VERBOSE) cout << "Collecting files in /tmp\n";

    // remove all entries; we assume here that they are files
    Dir::Entry e;
    char path[128];
    while(dir.readdir(e)) {
        if(strcmp(e.name, ".") == 0 || strcmp(e.name, "..") == 0)
            continue;

        OStringStream file(path, sizeof(path));
        file << "/tmp/" << e.name;
        if(x > ARRAY_SIZE(entries))
            PANIC("Too few entries");
        entries[x++] = new String(file.str());
    }

    for(; x > 0; --x) {
        remove_rec(entries[x - 1]->c_str());
        delete entries[x - 1];
    }
}

int main(int argc, char **argv) {
    Platform::init(argc, argv);

    VFS::mount("/", "m3fs");

    // defaults
    int num_iterations  = 8;
    bool keep_time      = true;
    bool make_ckpt      = false;
    bool wait           = false;

    // playback / revert to init-trace contents
    const char *prefix = "";
    if(argc > 1) {
        prefix = argv[1];
        if(*prefix)
            VFS::mkdir(prefix, 0755);
    }

    TracePlayer player(prefix);

    // print parameters for reference
    cout << "VPFS trace_bench started ["
         << "n=" << num_iterations << ","
         << "keeptime=" << (keep_time   ? "yes" : "no")
         << "]\n";

    for(int i = 0; i < num_iterations; ++i) {
        player.play(wait, keep_time, make_ckpt);
        cleanup();
    }

    cout << "VPFS trace_bench benchmark terminated\n";

    // done
    Platform::shutdown();
    return 0;
}
