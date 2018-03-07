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

#include <m3/stream/FStream.h>
#include <m3/stream/Standard.h>
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
        exitmsg("open of " << filename << " failed");

    size_t pos = 0;
    ssize_t count;
    while((count = file->read(largebuf, sizeof(largebuf))) > 0) {
        assert_size(static_cast<size_t>(count), sizeof(largebuf));
        for(ssize_t i = 0; i < count; ++i)
            assert_int(largebuf[i], pos++ & 0xFF);
    }
    assert_size(pos, size);

    FileInfo info;
    if(file->stat(info) != 0)
        exitmsg("stat of '" << filename << "' failed");
    assert_size(info.size, size);
}

void FS2TestSuite::WriteFileTestCase::run() {
    const char *filename = "/test.txt";

    cout << "-- Extending a small file --\n";
    {
        FileRef file(filename, FILE_W);
        if(Errors::occurred())
            exitmsg("open of " << filename << " failed");

        for(size_t i = 0; i < sizeof(largebuf); ++i)
            largebuf[i] = i & 0xFF;

        for(int i = 0; i < 129; ++i) {
            ssize_t count = file->write(largebuf, sizeof(largebuf));
            assert_ssize(count, sizeof(largebuf));
        }
    }

    check_content(filename, sizeof(largebuf) * 129);

    cout << "-- Test a small write at the beginning --\n";
    {
        FileRef file(filename, FILE_W);
        if(Errors::occurred())
            exitmsg("open of " << filename << " failed");

        for(size_t i = 0; i < sizeof(largebuf); ++i)
            largebuf[i] = i & 0xFF;

        for(int i = 0; i < 3; ++i) {
            ssize_t count = file->write(largebuf, sizeof(largebuf));
            assert_ssize(count, sizeof(largebuf));
        }
    }

    check_content(filename, sizeof(largebuf) * 129);

    cout << "-- Test truncate --\n";
    {
        FileRef file(filename, FILE_W | FILE_TRUNC);
        if(Errors::occurred())
            exitmsg("open of " << filename << " failed");

        for(size_t i = 0; i < sizeof(largebuf); ++i)
            largebuf[i] = i & 0xFF;

        for(int i = 0; i < 2; ++i) {
            ssize_t count = file->write(largebuf, sizeof(largebuf));
            assert_ssize(count, sizeof(largebuf));
        }
    }

    check_content(filename, sizeof(largebuf) * 2);

    cout << "-- Test append --\n";
    {
        FileRef file(filename, FILE_W | FILE_APPEND);
        if(Errors::occurred())
            exitmsg("open of " << filename << " failed");

        for(size_t i = 0; i < sizeof(largebuf); ++i)
            largebuf[i] = i & 0xFF;

        for(int i = 0; i < 2; ++i) {
            ssize_t count = file->write(largebuf, sizeof(largebuf));
            assert_ssize(count, sizeof(largebuf));
        }
    }

    check_content(filename, sizeof(largebuf) * 4);

    cout << "-- Test append with read --\n";
    {
        FileRef file(filename, FILE_RW | FILE_TRUNC | FILE_CREATE);
        if(Errors::occurred())
            exitmsg("open of " << filename << " failed");

        for(size_t i = 0; i < sizeof(largebuf); ++i)
            largebuf[i] = i & 0xFF;

        for(int i = 0; i < 2; ++i) {
            ssize_t count = file->write(largebuf, sizeof(largebuf));
            assert_ssize(count, sizeof(largebuf));
        }

        // there is nothing to read now
        assert_ssize(file->read(largebuf, sizeof(largebuf)), 0);

        // seek back
        assert_ssize(file->seek(sizeof(largebuf) * 1, M3FS_SEEK_SET), sizeof(largebuf) * 1);
        // now reading should work
        assert_ssize(file->read(largebuf, sizeof(largebuf)), sizeof(largebuf));
    }

    check_content(filename, sizeof(largebuf) * 2);
}

void FS2TestSuite::MetaFileTestCase::run() {
    assert_int(VFS::mkdir("/example", 0755), Errors::NONE);
    assert_int(VFS::mkdir("/example", 0755), Errors::EXISTS);
    assert_int(VFS::mkdir("/example/foo/bar", 0755), Errors::NO_SUCH_FILE);

    {
        FStream f("/example/myfile", FILE_W | FILE_CREATE);
        f << "test\n";
    }

    {
        assert_int(VFS::mount("/fs/", "m3fs"), Errors::NONE);
        assert_int(VFS::link("/example/myfile", "/fs/foo"), Errors::XFS_LINK);
        VFS::unmount("/fs");
    }

    assert_int(VFS::rmdir("/example/foo/bar"), Errors::NO_SUCH_FILE);
    assert_int(VFS::rmdir("/example/myfile"), Errors::IS_NO_DIR);
    assert_int(VFS::rmdir("/example"), Errors::DIR_NOT_EMPTY);

    assert_int(VFS::link("/example", "/newpath"), Errors::IS_DIR);
    assert_int(VFS::link("/example/myfile", "/newpath"), Errors::NONE);

    assert_int(VFS::unlink("/example"), Errors::IS_DIR);
    assert_int(VFS::unlink("/example/foo"), Errors::NO_SUCH_FILE);
    assert_int(VFS::unlink("/example/myfile"), Errors::NONE);

    assert_int(VFS::rmdir("/example"), Errors::NONE);

    assert_int(VFS::unlink("/newpath"), Errors::NONE);
}
