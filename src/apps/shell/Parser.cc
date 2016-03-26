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

#include <m3/stream/FStream.h>
#include <m3/stream/Standard.h>

#include "Parser.h"
#include "parser.tab.h"

using namespace m3;

static bool eof = false;
static IStream *in;
CmdList *curcmd;
extern YYSTYPE yylval;

EXTERN_C int yyparse(void);

EXTERN_C void yyerror(char const *s) {
    cerr << s << "\n";
    cerr.flush();
}

EXTERN_C int yylex() {
    static char buf[256];
    size_t i = 0;
    buf[i] = '\0';

    char c;
    if(!eof) {
        while((c = in->read()) > 0) {
            if(c == '|' || c == ';' || c == '>' || c == '<') {
                if(i == 0)
                    return c;
                in->putback(c);
                break;
            }

            if(c == '\n') {
                eof = true;
                break;
            }
            if(c == ' ' || c == '\t') {
                if(i > 0)
                    break;
            }
            else
                buf[i++] = c;
        }
    }

    if(i > 0) {
        buf[i] = '\0';
        yylval.str = (char*)Heap::alloc(i + 1);
        strcpy((char*)yylval.str, buf);
        return T_STRING;
    }
    return -1;
}

Command *ast_cmd_create(ArgList *args, RedirList *redirs) {
    Command *cmd = new Command;
    cmd->args = args;
    cmd->redirs = redirs;
    return cmd;
}

void ast_cmd_destroy(Command *cmd) {
    if(cmd) {
        ast_redirs_destroy(cmd->redirs);
        ast_args_destroy(cmd->args);
        delete cmd;
    }
}

CmdList *ast_cmds_create() {
    CmdList *list = new CmdList;
    list->count = 0;
    return list;
}

void ast_cmds_append(CmdList *list, Command *cmd) {
    if(list->count == MAX_CMDS)
        return;

    list->cmds[list->count++] = cmd;
}

void ast_cmds_destroy(CmdList *list) {
    if(list) {
        for(size_t i = 0; i < list->count; ++i)
            ast_cmd_destroy(list->cmds[i]);
        delete list;
    }
}

RedirList *ast_redirs_create(void) {
    RedirList *list = new RedirList;
    list->fds[STDIN_FD] = nullptr;
    list->fds[STDOUT_FD] = nullptr;
    return list;
}

void ast_redirs_set(RedirList *list, int fd, const char *file) {
    assert(fd == STDIN_FD || fd == STDOUT_FD);
    if(list->fds[fd])
        Heap::free((void*)list->fds[fd]);
    list->fds[fd] = file;
}

void ast_redirs_destroy(RedirList *list) {
    Heap::free((void*)list->fds[STDIN_FD]);
    Heap::free((void*)list->fds[STDOUT_FD]);
}

ArgList *ast_args_create() {
    ArgList *list = new ArgList;
    list->count = 0;
    return list;
}

void ast_args_append(ArgList *list, const char *arg) {
    if(list->count == MAX_ARGS)
        return;

    list->args[list->count++] = arg;
}

void ast_args_destroy(ArgList *list) {
    if(list) {
        for(size_t i = 0; i < list->count; ++i)
            Heap::free((void*)list->args[i]);
        delete list;
    }
}

CmdList *get_command(IStream *stream) {
    eof = false;
    curcmd = nullptr;
    in = stream;
    yyparse();
    return curcmd;
}
