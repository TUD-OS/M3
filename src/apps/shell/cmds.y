%{
    #include <stdlib.h>

    #include "Parser.h"

    int yylex (void);
    void yyerror (char const *);

    extern void *curcmd;
%}

%union {
    ArgList *arglist;
    CmdList *cmdlist;
    RedirList *redirlist;
    Command *cmd;
    const char *str;
}

%token T_STRING
%left '|' '<' '>'

%type <str> T_STRING arg
%type <arglist> args
%type <cmd> cmd
%type <cmdlist> cmds start
%type <redirlist> redirs

%destructor { free((void*)$$); } <str>
%destructor { ast_cmds_destroy($$); } <cmdlist>
%destructor { ast_args_destroy($$); } <arglist>
%destructor { ast_redirs_destroy($$); } <redirlist>
%destructor { ast_cmd_destroy($$); } <cmd>

%%

start:
            cmds                                    {
                                                        curcmd = $1;
                                                        $$ = NULL;
                                                    }

cmds:       cmd                                     {
                                                        $$ = ast_cmds_create();
                                                        ast_cmds_append($$, $1);
                                                    }
            | cmds '|' cmd                          {
                                                        $$ = $1;
                                                        ast_cmds_append($$, $3);
                                                    }
;

cmd:        args redirs                             { $$ = ast_cmd_create($1, $2); }
;

redirs:
            /* empty */                             { $$ = ast_redirs_create(); }
            | redirs '<' T_STRING                   { $$ = $1; ast_redirs_set($1, 0, $3); }
            | redirs '>' T_STRING                   { $$ = $1; ast_redirs_set($1, 1, $3); }
;

args:       arg                                     {
                                                        $$ = ast_args_create();
                                                        ast_args_append($$, $1);
                                                    }
            | args arg                              {
                                                        $$ = $1;
                                                        ast_args_append($1, $2);
                                                    }

arg:        T_STRING                                { $$ = $1; }
;

%%
