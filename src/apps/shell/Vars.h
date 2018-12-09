/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#pragma once

#include <base/Common.h>
#include <base/col/SList.h>
#include <string.h>

#include "Parser.h"

class Vars {
    struct Var : public m3::SListItem {
        char *name;
        char *value;
    };

    explicit Vars() : _vars() {
    }

public:
    static Vars &get() {
        return _inst;
    }

    const char *get(const char *name) {
        Var *v = get_var(name);
        return v ? v->value : "";
    }

    void set(const char *name, const char *value) {
        Var *v = get_var(name);
        if(!v) {
            v = new Var;
            v->name = new char[strlen(name) + 1];
            strcpy(v->name, name);
            _vars.append(v);
        }
        else
            delete[] v->value;

        v->value = new char[strlen(value) + 1];
        strcpy(v->value, value);
    }

private:
    Var *get_var(const char *name) {
        for(auto it = _vars.begin(); it != _vars.end(); ++it) {
            if(strcmp(it->name, name) == 0)
                return &*it;
        }
        return nullptr;
    }

    m3::SList<Var> _vars;
    static Vars _inst;
};

static inline const char *expr_value(Expr *e) {
    if(e->is_var)
        return Vars::get().get(e->name_val);
    return e->name_val;
}
