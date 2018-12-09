/*
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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
            if(c == '|' || c == ';' || c == '>' || c == '<'  || c == '=' || c == '$') {
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
        yylval.str = static_cast<char*>(Heap::alloc(i + 1));
        strcpy(const_cast<char*>(yylval.str), buf);
        return T_STRING;
    }
    return -1;
}

Expr *ast_expr_create(const char *name, int is_var) {
    Expr *e = new Expr;
    e->is_var = is_var;
    e->name_val = name;
    return e;
}

void ast_expr_destroy(Expr *e) {
    Heap::free(const_cast<char*>(e->name_val));
    delete e;
}

Command *ast_cmd_create(VarList *vars, ArgList *args, RedirList *redirs) {
    Command *cmd = new Command;
    cmd->vars = vars;
    cmd->args = args;
    cmd->redirs = redirs;
    return cmd;
}

void ast_cmd_destroy(Command *cmd) {
    if(cmd) {
        ast_vars_destroy(cmd->vars);
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
        Heap::free(const_cast<char*>(list->fds[fd]));
    list->fds[fd] = file;
}

void ast_redirs_destroy(RedirList *list) {
    Heap::free(const_cast<char*>(list->fds[STDIN_FD]));
    Heap::free(const_cast<char*>(list->fds[STDOUT_FD]));
}

ArgList *ast_args_create() {
    ArgList *list = new ArgList;
    list->count = 0;
    return list;
}

void ast_args_append(ArgList *list, Expr *arg) {
    if(list->count == MAX_ARGS)
        return;

    list->args[list->count++] = arg;
}

void ast_args_destroy(ArgList *list) {
    if(list) {
        for(size_t i = 0; i < list->count; ++i)
            ast_expr_destroy(list->args[i]);
        delete list;
    }
}

VarList *ast_vars_create(void) {
    VarList *list = new VarList;
    list->count = 0;
    return list;
}

void ast_vars_set(VarList *list, const char *name, Expr *value) {
    if(list->count == MAX_VARS)
        return;

    list->vars[list->count].name = name;
    list->vars[list->count].value = value;
    list->count++;
}

void ast_vars_destroy(VarList *list) {
    for(size_t i = 0; i < list->count; ++i) {
        Heap::free(const_cast<char*>(list->vars[i].name));
        ast_expr_destroy(list->vars[i].value);
    }
}

CmdList *get_command(IStream *stream) {
    eof = false;
    curcmd = nullptr;
    in = stream;
    yyparse();
    return curcmd;
}
