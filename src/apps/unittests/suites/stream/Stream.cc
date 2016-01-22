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

#include <m3/Common.h>
#include <m3/stream/IStringStream.h>
#include <m3/stream/FStream.h>
#include <m3/stream/Serial.h>
#include <m3/util/Math.h>

#include "Stream.h"

using namespace m3;

void StreamTestSuite::IStreamTestCase::run() {
    int a,b;
    uint d;
    float f;

    {
        String in("1 2 0xAfd2");
        IStringStream is(in);
        is >> a >> b >> d;
        assert_int(a, 1);
        assert_int(b, 2);
        assert_int(d, 0xAfd2);
    }

    {
        String in("  -1\t+2\n\n0XA");
        IStringStream is(in);
        is >> a >> b >> d;
        assert_int(a, -1);
        assert_int(b, 2);
        assert_int(d, 0XA);
    }

    {
        String str;
        String in("  1\tabc\n\n12.4");
        IStringStream is(in);
        is >> d >> str >> f;
        assert_int(d, 1);
        assert_str(str.c_str(), "abc");
        assert_float(f, 12.4);
    }

    {
        char buf[16];
        size_t res;
        String in(" 1234 55 test\n\nfoo\n012345678901234567");
        IStringStream is(in);
        assert_true(is.good());

        res = is.getline(buf, sizeof(buf));
        assert_size(res, 13);
        assert_str(buf, " 1234 55 test");

        res = is.getline(buf, sizeof(buf));
        assert_size(res, 0);
        assert_str(buf, "");

        res = is.getline(buf, sizeof(buf));
        assert_size(res, 3);
        assert_str(buf, "foo");

        res = is.getline(buf, sizeof(buf));
        assert_size(res, 15);
        assert_str(buf, "012345678901234");

        res = is.getline(buf, sizeof(buf));
        assert_size(res, 3);
        assert_str(buf, "567");

        assert_true(is.eof());
    }

    struct TestItem {
        const char *str;
        float res;
    };
    struct TestItem tests[] = {
        {"1234",         1234.f},
        {" 12.34",       12.34f},
        {".5",           .5f},
        {"\t +6.0e2\n",  6.0e2f},
        {"-12.35E5",     -12.35E5f},
    };
    for(size_t i = 0; i < ARRAY_SIZE(tests); i++) {
        String in(tests[i].str);
        IStringStream is(in);
        is >> f;
        assert_float(f, tests[i].res);
    }
}

#define STREAM_CHECK(expr, expstr) do {                                                     \
        OStringStream __os(str, sizeof(str));                                               \
        __os << expr;                                                                       \
        assert_str(str, expstr);                                                             \
    } while(0)

void StreamTestSuite::OStreamTestCase::run() {
    char str[200];

    STREAM_CHECK(1 << 2 << 3,
        "123");

    STREAM_CHECK(0x12345678 << "  " << 1.2f << ' ' << '4' << "\n",
        "305419896  1.200 4\n");

    STREAM_CHECK(fmt(1, 2) << ' ' << fmt(123, "0", 10) << ' ' << fmt(0xA23, "#0x", 8),
        " 1 0000000123 0x00000a23");

    STREAM_CHECK(fmt(-123, "+") << ' ' << fmt(123, "+") << ' ' << fmt(444, " ") << ' ' << fmt(-3, " "),
        "-123 +123  444 -3");

    STREAM_CHECK(fmt(-123, "-", 5) << ' ' << fmt(0755, "0o", 5) << ' ' << fmt(0xFF0, "b"),
        "-123  00755 111111110000");

    STREAM_CHECK(fmt(0xDEAD, "#0X", 5) << ' ' << fmt("test", 5, 3) << ' ' << fmt("foo", "-", 4),
        "0X0DEAD   tes foo ");

    OStringStream os(str, sizeof(str));
    os << fmt(0xdeadbeef, "p") << ", " << fmt(0x12345678, "x");
    if(sizeof(uintptr_t) == 4)
        assert_str(str, "dead:beef, 12345678");
    else if(sizeof(uintptr_t) == 8)
        assert_str(str, "0000:0000:dead:beef, 12345678");
    else
        assert_false(true);

    STREAM_CHECK(0.f << ", " << 1.f << ", " << -1.f << ", " << 0.f << ", " << 0.4f << ", " << 18.4f,
                 "0.000, 1.000, -1.000, 0.000, 0.400, 18.399");
    STREAM_CHECK(-1.231f << ", " << 999.999f << ", " << 1234.5678f << ", " << 100189380608.f,
                 "-1.230, 999.999, 1234.567, 100189380608.000");

    STREAM_CHECK(Math::inf() << ", " << -Math::inf() << ", " << Math::nan(),
        "inf, -inf, nan");
}

void StreamTestSuite::FStreamTestCase::run() {
    int totala = 0, totalb = 0;
    float totalc = 0;
    FStream f("/mat.txt", FILE_R);
    while(!f.eof()) {
        int a, b;
        float c;
        f >> a >> b >> c;
        totala += a;
        totalb += b;
        totalc += c;
    }
    assert_int(totala, 52184);
    assert_int(totalb, 52184);
    // unittests with floats are really bad. the results are slightly different on x86 and Xtensa.
    // thus, we only require that the integer value is correct. this gives us at least some degree
    // of correctness here
    assert_int((int)totalc, 1107);
}
