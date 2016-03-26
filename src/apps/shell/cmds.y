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
    Command *cmd;
    const char *str;
}

%token T_STRING
%left '|'

%type <str> T_STRING arg
%type <arglist> args
%type <cmd> cmd
%type <cmdlist> cmds start

%destructor { free((void*)$$); } <str>
%destructor { ast_cmds_destroy($$); } <cmdlist>
%destructor { ast_args_destroy($$); } <arglist>
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

cmd:        args                                    { $$ = ast_cmd_create($1); }
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
