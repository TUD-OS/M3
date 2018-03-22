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

#pragma once

#include <base/Common.h>

namespace m3 {

class CmdArgs {
    CmdArgs() = delete;

public:
    enum {
        NONE,
        REQUIRED,
    };

    /**
     * Describes a long option
     */
    struct Option {
        const char *name;
        int has_arg;
        int *flag;  /* unsupported */
        int val;
    };

    /**
     * Points to the argument of an option or is NULL if there is none
     */
    static char *arg;
    /**
     * The next index in argv that is considered
     */
    static int ind;
    /**
     * Whether an error message should be printed to stderr
     */
    static int err;
    /**
     * The character that was considered in case '?' is returned
     */
    static int opt;

    /**
     * Parses the given arguments for given options.
     *
     * <optstring> is used for short options ("-o") and consists of the option characters. A ':' after
     * an option character specifies that the option has an argument.
     * If an option is found, the character is returned. If it has an argument, <optarg> points to the
     * argument (either the text after the option character or the text of the next item in argv).
     *
     * <longopts> can optionally be specified for long options ("--option"). The last array element
     * needs to consist of zeros. <name> specifies the option name ("option"), <has_arg> specifies
     * whether it has an argument (*_argument) and <val> the value to return if that option is found.
     *
     * @param argc the number of args, as received in main
     * @param argv the arguments, as received in main
     * @param optstring the short options specification
     * @param longopts the long options specification
     * @param longindex if non-NULL, optind will be set to *longindex
     * @return the option or -1 if no option has been found
     */
    static int get_long(int argc, char *const argv[], const char *optstring,
                        const Option *longopts, int *longindex);
    static int get(int argc, char *const argv[], const char *optstring) {
        return get_long(argc, argv, optstring, nullptr, nullptr);
    }

    /**
     * Converts the given string to a number, supporting the suffixes K, M, and G (lower and upper case)
     *
     * @param str the string
     * @return the number
     */
    static size_t to_size(const char *str);

    /**
     * Checks whether the given arguments may be a kind of help-request. That means one of:
     * <prog> --help
     * <prog> -h
     * <prog> -?
     *
     * @param argc the number of arguments
     * @param argv the arguments
     * @return true if it is a help-request
     */
    static bool is_help(int argc, char **argv);

private:
    static int has_arg(char c, const char *optstring);
    static int has_longarg(const char **arg, const Option *longopts, int *val);
};

}
