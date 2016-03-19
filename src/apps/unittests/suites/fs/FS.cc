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

#include <m3/vfs/VFS.h>
#include <m3/vfs/FileRef.h>
#include <m3/vfs/Dir.h>
#include <m3/stream/FStream.h>

#include <vector>
#include <algorithm>

#include "FS.h"

using namespace m3;

alignas(DTU_PKG_SIZE) static uint8_t largebuf[1024];

void FSTestSuite::DirTestCase::run() {
    // read a dir with known content
    const char *dirname = "/largedir";
    Dir dir(dirname);
    if(Errors::occurred())
        PANIC("open of " << dirname << " failed (" << Errors::last << ")");

    Dir::Entry e;
    std::vector<Dir::Entry> entries;
    while(dir.readdir(e))
        entries.push_back(e);
    assert_size(entries.size(), 82);

    // we don't know the order because it is determined by the host OS. thus, sort it first.
    std::sort(entries.begin(), entries.end(), [] (const Dir::Entry &a, const Dir::Entry &b) -> bool {
        bool aspec = strcmp(a.name, ".") == 0 || strcmp(a.name, "..") == 0;
        bool bspec = strcmp(b.name, ".") == 0 || strcmp(b.name, "..") == 0;
        if(aspec && bspec)
            return strcmp(a.name, b.name) < 0;
        if(aspec)
            return true;
        if(bspec)
            return false;
        return IStringStream::read_from<int>(a.name) < IStringStream::read_from<int>(b.name);
    });

    // now check file names
    assert_str(entries[0].name, ".");
    assert_str(entries[1].name, "..");
    for(int i = 0; i < 80; ++i) {
        char tmp[16];
        OStringStream os(tmp, sizeof(tmp));
        os << i << ".txt";
        assert_str(entries[i + 2].name, os.str());
    }
}

void FSTestSuite::FileTestCase::run() {
    Serial::get() << "-- Test errors --\n";
    {
        const char *filename = "/subdir/subsubdir/testfile.txt";

        alignas(DTU_PKG_SIZE) char buf[DTU_PKG_SIZE];
        {
            FileRef file(filename, FILE_R);
            if(Errors::occurred())
                PANIC("open of " << filename << " failed (" << Errors::last << ")");
            assert_long(file->write(buf, sizeof(buf)), Errors::NO_PERM);
        }

        {
            FileRef file(filename, FILE_W);
            if(Errors::occurred())
                PANIC("open of " << filename << " failed (" << Errors::last << ")");
            assert_long(file->read(buf, sizeof(buf)), Errors::NO_PERM);
        }
    }

#if DTU_PKG_SIZE == 8
    Serial::get() << "-- Read a file with known content at once --\n";
    {
        const char *filename = "/subdir/subsubdir/testfile.txt";
        const char content[] = "This is a test!\n";
        static_assert(((sizeof(content) - 1) % DTU_PKG_SIZE) == 0, "Wrong size");

        FileRef file(filename, FILE_R);
        if(Errors::occurred())
            PANIC("open of " << filename << " failed (" << Errors::last << ")");

        alignas(DTU_PKG_SIZE) char buf[sizeof(content)];
        assert_size(file->read(buf, sizeof(buf) - 1), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        assert_str(buf, content);
    }
#endif

    Serial::get() << "-- Read a file in 64 byte steps --\n";
    {
        const char *filename = "/pat.bin";

        FileRef file(filename, FILE_R);
        if(Errors::occurred())
            PANIC("open of " << filename << " failed (" << Errors::last << ")");

        alignas(DTU_PKG_SIZE) uint8_t buf[64];
        ssize_t count, pos = 0;
        while((count = file->read(buf, sizeof(buf))) > 0) {
            for(ssize_t i = 0; i < count; ++i)
                assert_int(buf[i], pos++ & 0xFF);
        }
    }

    Serial::get() << "-- Read file in steps larger than block size --\n";
    {
        const char *filename = "/pat.bin";

        FileRef file(filename, FILE_R);
        if(Errors::occurred())
            PANIC("open of " << filename << " failed (" << Errors::last << ")");

        alignas(DTU_PKG_SIZE) static uint8_t buf[1024 * 3];
        ssize_t count, pos = 0;
        while((count = file->read(buf, sizeof(buf))) > 0) {
            for(ssize_t i = 0; i < count; ++i)
                assert_int(buf[i], pos++ & 0xFF);
        }
    }

    Serial::get() << "-- Write to a file and read it again --\n";
    {
        alignas(DTU_PKG_SIZE) char content[64] = "Foobar, a test and more and more and more!";
        const char *filename = "/pat.bin";
        const size_t contentsz = (strlen(content) + DTU_PKG_SIZE - 1) & ~(DTU_PKG_SIZE - 1);

        FileRef file(filename, FILE_RW);
        if(Errors::occurred())
            PANIC("open of " << filename << " failed (" << Errors::last << ")");

        ssize_t count = file->write(content, contentsz);
        assert_long(count, contentsz);

        assert_long(file->seek(0, SEEK_CUR), contentsz);
        assert_long(file->seek(0, SEEK_SET), 0);

        alignas(DTU_PKG_SIZE) char buf[contentsz];
        count = file->read(buf, sizeof(buf));
        assert_long(count, sizeof(buf));
        assert_str(buf, content);

        // undo the write
        file->seek(0, SEEK_SET);
        for(uint8_t i = 0; i < contentsz; ++i)
            content[i] = i;
        file->write(content, contentsz);
    }
}

void FSTestSuite::BufferedFileTestCase::run() {
    const char *filename = "/pat.bin";

    Serial::get() << "-- Read it until the end --\n";
    {
        FStream file(filename, FILE_R, 256);
        if(Errors::occurred())
            PANIC("open of " << filename << " failed (" << Errors::last << ")");

        uint8_t buf[16];
        ssize_t count, pos = 0;
        while((count = file.read(buf, sizeof(buf))) > 0) {
            for(ssize_t i = 0; i < count; ++i)
                assert_int(buf[i], pos++ & 0xFF);
        }
        assert_true(file.eof() && !file.error());
    }

    Serial::get() << "-- Read it with seek in between --\n";
    {
        FStream file(filename, FILE_R, 200);
        if(Errors::occurred())
            PANIC("open of " << filename << " failed (" << Errors::last << ")");

        uint8_t buf[32];
        ssize_t count, pos = 0;
        for(int i = 0; i < 10; ++i) {
            count = file.read(buf, sizeof(buf));
            assert_size(count, 32);
            for(ssize_t i = 0; i < count; ++i)
                assert_int(buf[i], pos++ & 0xFF);
        }

        // we are at pos 320, i.e. we have 200..399 in our buffer
        pos = 220;
        file.seek(pos, SEEK_SET);

        count = file.read(buf, sizeof(buf));
        assert_size(count, 32);
        for(ssize_t i = 0; i < count; ++i)
            assert_int(buf[i], pos++ & 0xFF);

        pos = 405;
        file.seek(pos, SEEK_SET);

        while((count = file.read(buf, sizeof(buf))) > 0) {
            for(ssize_t i = 0; i < count; ++i)
                assert_int(buf[i], pos++ & 0xFF);
        }
        assert_true(file.eof() && !file.error());
    }

    Serial::get() << "-- Read with large buffer size --\n";
    {
        FStream file(filename, FILE_R, 256);
        if(Errors::occurred())
            PANIC("open of " << filename << " failed (" << Errors::last << ")");

        ssize_t count, pos = 0;
        while((count = file.read(largebuf, sizeof(largebuf))) > 0) {
            for(ssize_t i = 0; i < count; ++i)
                assert_int(largebuf[i], pos++ & 0xFF);
        }
        assert_true(file.eof() && !file.error());
    }

    alignas(DTU_PKG_SIZE) char rbuf[600];
    alignas(DTU_PKG_SIZE) char wbuf[256];

    Serial::get() << "-- Read and write --\n";
    {
        FStream file(filename, rbuf, sizeof(rbuf), wbuf, sizeof(wbuf), FILE_RW);
        if(Errors::occurred())
            PANIC("open of " << filename << " failed (" << Errors::last << ")");

        size_t size = file.seek(0, SEEK_END);
        file.seek(0, SEEK_SET);

        // overwrite it
        uint8_t val = size - 1;
        for(size_t i = 0; i < size; ++i, --val)
            assert_long(file.write(&val, sizeof(val)), sizeof(val));

        // read it again and check content
        file.seek(0, SEEK_SET);
        val = size - 1;
        for(size_t i = 0; i < size; ++i, --val) {
            uint8_t check;
            assert_long(file.read(&check, sizeof(check)), sizeof(check));
            assert_int(check, val);
        }

        // restore old content
        file.seek(0, SEEK_SET);
        val = 0;
        for(size_t i = 0; i < size; ++i, ++val)
            assert_long(file.write(&val, sizeof(val)), sizeof(val));
        assert_true(file.good());
    }

    Serial::get() << "-- Write only --\n";
    {
        FStream file(filename, rbuf, sizeof(rbuf), wbuf, sizeof(wbuf), FILE_W);
        if(Errors::occurred())
            PANIC("open of " << filename << " failed (" << Errors::last << ")");

        // require a read by performing an unaligned write
        file.seek(DTU_PKG_SIZE * 10, SEEK_SET);
        file.write("foobar", DTU_PKG_SIZE - 2);
        file.flush();
        assert_true(file.good());
    }

#if DTU_PKG_SIZE == 8
    Serial::get() << "-- Write with seek --\n";
    {
        static_assert(DTU_PKG_SIZE == 8, "Unexpected DTU_PKG_SIZE");
        FStream file(filename, rbuf, sizeof(rbuf), wbuf, sizeof(wbuf), FILE_RW);
        if(Errors::occurred())
            PANIC("open of " << filename << " failed (" << Errors::last << ")");

        file.seek(2, SEEK_SET);
        file.write("test", 4);

        file.seek(8, SEEK_SET);
        file.write("foobar", 6);

        file.seek(11, SEEK_SET);
        file.write("foo", 3);

        file.seek(1, SEEK_SET);
        char buf[16];
        file.read(buf, 16);
        buf[15] = '\0';
        assert_true(file.good());

        char exp[] = {1,'t','e','s','t',6,7,'f','o','o','f','o','o',14,15,0};
        assert_str(buf, exp);
    }
#endif
}

void FSTestSuite::WriteFileTestCase::check_content(const char *filename, size_t size) {
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

void FSTestSuite::WriteFileTestCase::run() {
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

void FSTestSuite::MetaFileTestCase::run() {
    assert_int(VFS::mkdir("/example", 0755), Errors::NO_ERROR);
    assert_int(VFS::mkdir("/example", 0755), Errors::EXISTS);
    assert_int(VFS::mkdir("/example/foo/bar", 0755), Errors::NO_SUCH_FILE);

    {
        FStream f("/example/myfile", FILE_W | FILE_CREATE);
        f << "test\n";
    }

    {
        assert_int(VFS::mount("/fs/", new M3FS("m3fs")), 0);
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
