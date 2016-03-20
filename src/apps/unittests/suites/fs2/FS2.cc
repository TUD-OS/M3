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
#include <base/stream/OStringStream.h>
#include <base/stream/IStringStream.h>
#include <base/Log.h>

#include <m3/stream/FStream.h>
#include <m3/vfs/VFS.h>
#include <m3/vfs/FileRef.h>
#include <m3/vfs/Dir.h>

#include <vector>
#include <algorithm>

#include "FS2.h"

using namespace m3;

alignas(DTU_PKG_SIZE) static uint8_t largebuf[1024];

void FS2TestSuite::WriteFileTestCase::check_content(const char *filename, size_t size) {
    FileRef file(filename, FILE_R);
    if(Errors::occurred())
        PANIC("open of " << filename << " failed (" << Errors::last << ")");

    ssize_t count, pos = 0;
    while((count = file->read(largebuf, sizeof(largebuf))) > 0) {
        assert_int(count, sizeof(largebuf));
        for(ssize_t i = 0; i < count; ++i)
            assert_int(largebuf[i], pos++ & 0xFF);
    }
    assert_int(pos, size);

    FileInfo info;
    if(file->stat(info) != 0)
        PANIC("stat of '" << filename << "' failed");
    assert_int(info.size, size);
}

void FS2TestSuite::WriteFileTestCase::run() {
    const char *filename = "/test.txt";

    Serial::get() << "-- Extending a small file --\n";
    {
        FileRef file(filename, FILE_W);
        if(Errors::occurred())
            PANIC("open of " << filename << " failed (" << Errors::last << ")");

        for(size_t i = 0; i < sizeof(largebuf); ++i)
            largebuf[i] = i & 0xFF;

        for(int i = 0; i < 129; ++i) {
            ssize_t count = file->write(largebuf, sizeof(largebuf));
            assert_int(count, sizeof(largebuf));
        }
    }

    check_content(filename, sizeof(largebuf) * 129);

    Serial::get() << "-- Test a small write at the beginning --\n";
    {
        FileRef file(filename, FILE_W);
        if(Errors::occurred())
            PANIC("open of " << filename << " failed (" << Errors::last << ")");

        for(size_t i = 0; i < sizeof(largebuf); ++i)
            largebuf[i] = i & 0xFF;

        for(int i = 0; i < 3; ++i) {
            ssize_t count = file->write(largebuf, sizeof(largebuf));
            assert_int(count, sizeof(largebuf));
        }
    }

    check_content(filename, sizeof(largebuf) * 129);

    Serial::get() << "-- Test truncate --\n";
    {
        FileRef file(filename, FILE_W | FILE_TRUNC);
        if(Errors::occurred())
            PANIC("open of " << filename << " failed (" << Errors::last << ")");

        for(size_t i = 0; i < sizeof(largebuf); ++i)
            largebuf[i] = i & 0xFF;

        for(int i = 0; i < 2; ++i) {
            ssize_t count = file->write(largebuf, sizeof(largebuf));
            assert_int(count, sizeof(largebuf));
        }
    }

    check_content(filename, sizeof(largebuf) * 2);

    Serial::get() << "-- Test append --\n";
    {
        FileRef file(filename, FILE_W | FILE_APPEND);
        if(Errors::occurred())
            PANIC("open of " << filename << " failed (" << Errors::last << ")");

        for(size_t i = 0; i < sizeof(largebuf); ++i)
            largebuf[i] = i & 0xFF;

        for(int i = 0; i < 2; ++i) {
            ssize_t count = file->write(largebuf, sizeof(largebuf));
            assert_int(count, sizeof(largebuf));
        }
    }

    check_content(filename, sizeof(largebuf) * 4);

    Serial::get() << "-- Test append with read --\n";
    {
        FileRef file(filename, FILE_RW | FILE_TRUNC | FILE_CREATE);
        if(Errors::occurred())
            PANIC("open of " << filename << " failed (" << Errors::last << ")");

        for(size_t i = 0; i < sizeof(largebuf); ++i)
            largebuf[i] = i & 0xFF;

        for(int i = 0; i < 2; ++i) {
            ssize_t count = file->write(largebuf, sizeof(largebuf));
            assert_int(count, sizeof(largebuf));
        }

        // there is nothing to read now
        assert_int(file->read(largebuf, sizeof(largebuf)), 0);

        // seek beyond the end
        assert_int(file->seek(sizeof(largebuf) * 4, SEEK_SET), sizeof(largebuf) * 4);
        // seek back
        assert_int(file->seek(sizeof(largebuf) * 2, SEEK_SET), sizeof(largebuf) * 2);
        // now reading should work
        assert_int(file->read(largebuf, sizeof(largebuf)), sizeof(largebuf));
    }

    check_content(filename, sizeof(largebuf) * 4);
}

void FS2TestSuite::MetaFileTestCase::run() {
    assert_int(VFS::mkdir("/example", 0755), Errors::NO_ERROR);
    assert_int(VFS::mkdir("/example", 0755), Errors::EXISTS);
    assert_int(VFS::mkdir("/example/foo/bar", 0755), Errors::NO_SUCH_FILE);

    {
        FStream f("/example/myfile", FILE_W | FILE_CREATE);
        f << "test\n";
    }

    {
        assert_int(VFS::mount("/fs/", new M3FS("m3fs")), Errors::NO_ERROR);
        assert_int(VFS::link("/example/myfile", "/fs/foo"), Errors::XFS_LINK);
        VFS::unmount("/fs");
    }

    assert_int(VFS::rmdir("/example/foo/bar"), Errors::NO_SUCH_FILE);
    assert_int(VFS::rmdir("/example/myfile"), Errors::IS_NO_DIR);
    assert_int(VFS::rmdir("/example"), Errors::DIR_NOT_EMPTY);

    assert_int(VFS::link("/example", "/newpath"), Errors::IS_DIR);
    assert_int(VFS::link("/example/myfile", "/newpath"), Errors::NO_ERROR);

    assert_int(VFS::unlink("/example"), Errors::IS_DIR);
    assert_int(VFS::unlink("/example/foo"), Errors::NO_SUCH_FILE);
    assert_int(VFS::unlink("/example/myfile"), Errors::NO_ERROR);

    assert_int(VFS::rmdir("/example"), Errors::NO_ERROR);

    assert_int(VFS::unlink("/newpath"), Errors::NO_ERROR);
}
