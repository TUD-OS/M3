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

#include <m3/stream/Standard.h>

extern int succeeded;
extern int failed;

template<class T>
static inline void check_equal(const T &expected, const T &actual, const char *file, int line) {
    if(expected != actual) {
        m3::cout << "  \033[0;31mAssert failed\033[0m in " << file
                 << ", line " << line
                 << ": expected '" << expected << "', got '" << actual << "'\n";
        failed++;
    }
    else
        succeeded++;
}

#if defined(__host__)
void tdtu();
#endif
void tfsmeta();
void tfs();
void tbitfield();
void theap();
void tstream();

#define assert_int(actual, expected) \
    check_equal<int>((expected), (actual), __FILE__, __LINE__)
#define assert_long(actual, expected) \
    check_equal<long>((expected), (actual), __FILE__, __LINE__)
#define assert_uint(actual, expected) \
    check_equal<unsigned int>((expected), (actual), __FILE__, __LINE__)
#define assert_ulong(actual, expected) \
    check_equal<unsigned long>((expected), (actual), __FILE__, __LINE__)
#define assert_size(actual, expected) \
    check_equal<size_t>((expected), (actual), __FILE__, __LINE__)
#define assert_ssize(actual, expected) \
    check_equal<ssize_t>((expected), (actual), __FILE__, __LINE__)
#define assert_word(actual, expected) \
    check_equal<word_t>((expected), (actual), __FILE__, __LINE__)
#define assert_xfer(actual, expected) \
    check_equal<xfer_t>((expected), (actual), __FILE__, __LINE__)
#define assert_str(actual, expected) \
    check_equal<m3::String>((expected), (actual), __FILE__, __LINE__)
#define assert_true(expected) \
    check_equal<bool>((expected), true, __FILE__, __LINE__)
#define assert_false(expected) \
    check_equal<bool>((expected), false, __FILE__, __LINE__)
#define assert_float(actual, expected) \
    check_equal<float>((expected), (actual), __FILE__, __LINE__)

#define RUN_SUITE(name)                                             \
    m3::cout << "Running testsuite " << #name << " ...\n";    \
    name();                                                         \
    m3::cout << "Done\n\n";

#define RUN_TEST(name)                                             \
    m3::cout << "-- Running testcase " << #name << " ...\n";          \
    name();                                                         \
    m3::cout << "-- Done\n";
