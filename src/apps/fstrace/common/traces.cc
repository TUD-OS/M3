/*
 * Copyright (C) 2016, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include "traces.h"

#include <string.h>

#define ARRAY_SIZE(a) (sizeof((a)) / sizeof((a)[0]))

extern trace_op_t trace_ops_find[];
extern trace_op_t trace_ops_leveldb[];
extern trace_op_t trace_ops_nginx[];
extern trace_op_t trace_ops_sha256sum[];
extern trace_op_t trace_ops_sort[];
extern trace_op_t trace_ops_sqlite[];
extern trace_op_t trace_ops_tar[];
extern trace_op_t trace_ops_untar[];
extern trace_op_t trace_ops_cat_awk_cat[];
extern trace_op_t trace_ops_cat_awk_awk[];
extern trace_op_t trace_ops_cat_wc_cat[];
extern trace_op_t trace_ops_cat_wc_wc[];
extern trace_op_t trace_ops_grep_awk_grep[];
extern trace_op_t trace_ops_grep_awk_awk[];
extern trace_op_t trace_ops_grep_wc_grep[];
extern trace_op_t trace_ops_grep_wc_wc[];

Trace Traces::traces[] = {
    {"find",            trace_ops_find},
    {"leveldb",         trace_ops_leveldb},
    {"nginx",           trace_ops_nginx},
    {"sha256sum",       trace_ops_sha256sum},
    {"sort",            trace_ops_sort},
    {"sqlite",          trace_ops_sqlite},
    {"tar",             trace_ops_tar},
    {"untar",           trace_ops_untar},
    {"cat_awk_cat",     trace_ops_cat_awk_cat},
    {"cat_awk_awk",     trace_ops_cat_awk_awk},
    {"cat_wc_cat",      trace_ops_cat_wc_cat},
    {"cat_wc_wc",       trace_ops_cat_wc_wc},
    {"grep_awk_grep",   trace_ops_grep_awk_grep},
    {"grep_awk_awk",    trace_ops_grep_awk_awk},
    {"grep_wc_grep",    trace_ops_grep_wc_grep},
    {"grep_wc_wc",      trace_ops_grep_wc_wc},
};

Trace *Traces::get(const char *name) {
    for(size_t i = 0; i < ARRAY_SIZE(traces); ++i) {
        if(strcmp(traces[i].name, name) == 0)
            return traces + i;
    }
    return nullptr;
}
