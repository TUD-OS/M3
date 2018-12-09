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

#pragma once

#include <base/Compiler.h>

#define MAX_CMDS    8
#define MAX_ARGS   32
#define MAX_VARS    4

typedef struct {
    int is_var;
    const char *name_val;
} Expr;

typedef struct {
    unsigned count;
    Expr *args[MAX_ARGS];
} ArgList;

typedef struct {
    const char *fds[2];
} RedirList;

typedef struct {
    const char *name;
    Expr *value;
} Var;

typedef struct {
    unsigned count;
    Var vars[MAX_VARS];
} VarList;

typedef struct {
    VarList *vars;
    ArgList *args;
    RedirList *redirs;
} Command;

typedef struct {
    unsigned count;
    Command *cmds[MAX_CMDS];
} CmdList;

EXTERN_C Expr *ast_expr_create(const char *name, int is_var);
EXTERN_C void ast_expr_destroy(Expr *e);

EXTERN_C Command *ast_cmd_create(VarList *vars, ArgList *args, RedirList *redirs);
EXTERN_C void ast_cmd_destroy(Command *cmd);

EXTERN_C RedirList *ast_redirs_create(void);
EXTERN_C void ast_redirs_set(RedirList *list, int fd, const char *file);
EXTERN_C void ast_redirs_destroy(RedirList *list);

EXTERN_C CmdList *ast_cmds_create(void);
EXTERN_C void ast_cmds_append(CmdList *list, Command *cmd);
EXTERN_C void ast_cmds_destroy(CmdList *list);

EXTERN_C ArgList *ast_args_create(void);
EXTERN_C void ast_args_append(ArgList *list, Expr *arg);
EXTERN_C void ast_args_destroy(ArgList *list);

EXTERN_C VarList *ast_vars_create(void);
EXTERN_C void ast_vars_set(VarList *list, const char *name, Expr *value);
EXTERN_C void ast_vars_destroy(VarList *list);

#if defined(__cplusplus)
#   include <base/stream/IStream.h>

CmdList *get_command(m3::IStream *stream);
#endif
