/*
 * Copyright (C) 2017-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/util/Chars.h>
#include <base/CmdArgs.h>
#include <base/Panic.h>

#include <string.h>

namespace m3 {

int CmdArgs::ind = 1;
char *CmdArgs::arg = nullptr;
int CmdArgs::err = 1;
int CmdArgs::opt = 0;

int CmdArgs::has_arg(char c, const char *optstring) {
    for(size_t i = 0; optstring[i]; ++i) {
        if(optstring[i] == c)
            return optstring[i + 1] == ':' ? REQUIRED : NONE;
    }
    return -1;
}

int CmdArgs::has_longarg(const char **arg, const Option *longopts, int *val) {
    for(size_t i = 0; longopts[i].name; ++i) {
        size_t optlen = strlen(longopts[i].name);
        if(strncmp(*arg, longopts[i].name, optlen) == 0) {
            *arg += optlen;
            *val = longopts[i].val;
            return longopts[i].has_arg;
        }
    }
    return -1;
}

bool CmdArgs::is_help(int argc, char **argv) {
    if(argc <= 1)
        return false;

    return strcmp(argv[1], "-h") == 0 ||
        strcmp(argv[1], "--help") == 0 ||
        strcmp(argv[1], "-?") == 0;
}

size_t CmdArgs::to_size(const char *str) {
    size_t val = 0;
    while(Chars::isdigit(*str))
        val = val * 10 + static_cast<size_t>(*str++ - '0');
    switch(*str) {
        case 'K':
        case 'k':
            val *= 1024;
            break;
        case 'M':
        case 'm':
            val *= 1024 * 1024;
            break;
        case 'G':
        case 'g':
            val *= 1024 * 1024 * 1024;
            break;
    }
    return val;
}

int CmdArgs::get_long(int argc, char *const argv[], const char *optstring,
                      const Option *longopts, int *longindex) {
    static size_t nextchar = 0;
    arg = nullptr;

    if(longindex)
        ind = *longindex;

    while(ind < argc) {
        const char *optel = argv[ind];
        if(nextchar == 0) {
            /* the first non-option stops the search */
            if(optel[0] != '-' || optel[1] == '\0' || (optel[1] == '-' && optel[2] == '\0'))
                return -1;
            nextchar = 1;
        }

        int val = 0, argtype;
        const char *optend;
        if(optel[1] == '-') {
            if(!longopts)
                return -1;
            optend = optel + 2;
            argtype = has_longarg(&optend, longopts, &val);
            if(*optend == '=')
                optend++;
        }
        else {
            /* done with this option element? */
            if(optel[nextchar] == '\0') {
                ind++;
                nextchar = 0;
                continue;
            }
            val = optel[nextchar++];
            argtype = has_arg(val, optstring);
            optend = optel + nextchar;
        }

        /* unknown option? */
        if(argtype == -1) {
            /* short option? */
            if(optel[1] != '-') {
                if(err)
                    PANIC("Unrecognized option '" << val << "' in argv[" << ind << "]=" << optel << "\n");
                opt = val;
            }
            else if(err)
                PANIC("Unrecognized argument argv[" << ind << "]=" << optel << "\n");
            val = '?';
        }

        /* option with arg? */
        if(argtype == REQUIRED) {
            /* the argument is the text following the option or the next cmdline argument */
            if(*optend != '\0') {
                arg = (char*)optend;
                ind++;
                nextchar = 0;
            }
            else if(ind + 1 >= argc) {
                if(err)
                    PANIC("Missing argument to option argv[" << ind << "]=" << optel << "\n");
                return '?';
            }
            else {
                arg = argv[ind + 1];
                ind += 2;
                nextchar = 0;
            }
        }
        else if(optel[1] == '-' || *optend == '\0') {
            nextchar = 0;
            ind++;
        }
        return val;
    }
    return -1;
}

}
