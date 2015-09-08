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

#include <m3/Common.h>
#include <m3/Config.h>
#include <m3/vfs/VFS.h>
#include <m3/vfs/FileRef.h>
#include <m3/Log.h>
#include <libgen.h>

using namespace m3;

alignas(DTU_PKG_SIZE) static char buffer[DRAM_FILENAME_LEN];

int main(int argc, char **argv) {
    if(argc < 2) {
        Serial::get() << "Usage: " << argv[0] << " <file>...\n";
        return 1;
    }

    MemGate mem = MemGate::create_global_for(DRAM_FILE_AREA, DRAM_FILE_AREA_LEN, MemGate::RW);
    if(Errors::last != Errors::NO_ERROR)
        PANIC("Unable to request DRAM_BLOCKNO memory");

    for(int i = 1; i < argc; ++i) {
        if(strlen(argv[i]) + 1 > DRAM_FILENAME_LEN) {
            Serial::get() << "Filename too long: " << argv[i] << "\n";
            continue;
        }

        FileInfo info;
        if(VFS::stat(argv[i], info) != Errors::NO_ERROR) {
            Serial::get() << "Unable to stat '" << argv[i] << "': "
                << Errors::to_string(Errors::last) << "\n";
            continue;
        }

        if(info.extents != 1) {
            Serial::get() << info.extents << " extents are not supported.\n";
            continue;
        }

        Serial::get() << "Sending '" << argv[i] << "' to host...\n";

        memset(buffer, 0, sizeof(buffer));
        strcpy(buffer, basename(argv[i]));
        mem.write_sync(buffer, DRAM_FILENAME_LEN, DRAM_FILENAME);

        uint64_t size = info.size;
        mem.write_sync(&size, sizeof(size), DRAM_FILESIZE);

        uint64_t bno = info.firstblock;
        mem.write_sync(&bno, sizeof(bno), DRAM_BLOCKNO);
        while(bno != 0)
            mem.read_sync(&bno, sizeof(bno), DRAM_BLOCKNO);

        Serial::get() << "Done!\n";
    }
    return 0;
}
