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
#include <base/Config.h>

#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>
#include <m3/vfs/FileRef.h>

#include <libgen.h>

using namespace m3;

alignas(DTU_PKG_SIZE) static char buffer[DRAM_FILENAME_LEN];

int main(int argc, char **argv) {
    if(argc < 2)
        exitmsg("Usage: " << argv[0] << " <file>...");

    MemGate mem = MemGate::create_global_for(DRAM_FILE_AREA, DRAM_FILE_AREA_LEN, MemGate::RW);
    if(Errors::last != Errors::NONE)
        exitmsg("Unable to request DRAM_BLOCKNO memory");

    for(int i = 1; i < argc; ++i) {
        if(strlen(argv[i]) + 1 > DRAM_FILENAME_LEN) {
            cout << "Filename too long: " << argv[i] << "\n";
            continue;
        }

        FileInfo info;
        if(VFS::stat(argv[i], info) != Errors::NONE) {
            cout << "Unable to stat '" << argv[i] << "': "
                << Errors::to_string(Errors::last) << "\n";
            continue;
        }

        if(info.extents != 1) {
            cout << info.extents << " extents are not supported.\n";
            continue;
        }

        cout << "Sending '" << argv[i] << "' to host...\n";

        memset(buffer, 0, sizeof(buffer));
        strcpy(buffer, basename(argv[i]));
        mem.write(buffer, DRAM_FILENAME_LEN, DRAM_FILENAME);

        uint64_t size = info.size;
        mem.write(&size, sizeof(size), DRAM_FILESIZE);

        uint64_t bno = info.firstblock;
        mem.write(&bno, sizeof(bno), DRAM_BLOCKNO);
        while(bno != 0)
            mem.read(&bno, sizeof(bno), DRAM_BLOCKNO);

        cout << "Done!\n";
    }
    return 0;
}
