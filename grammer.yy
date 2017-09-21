%skeleton "lalr1.cc"
%require "3.0.4"
%defines

%code
{
int yylex();
}

%locations
%define parse.error	verbose
%define api.value.type	variant

%token ID
%token STR
%token INT
%token TYPENAME
%token FUNC_ID
%token KW_CCALL
%token KW_SYSCALL
%token KW_VOID
%token KW_FUNC
%token KW_RETURN

%right '='
%left ',' 
%left '+' '-'
%left UMINUS

%start module	
%%
module: /* empty */
	| module function_definition
	| module ccall_declaration
	| module syscall_definition
	| module toplv_statement
	;

function_definition: KW_FUNC return_def FUNC_ID parameters ')' block
	;

return_def: /* empty */
	| return_types
	| return_values
	;

return_types: TYPENAME
	| return_types TYPENAME
	;

return_values: return_value
	| return_values ',' return_value
	| return_values ',' ID
	;

return_value: TYPENAME ID
	;

parameters: /* empty */
	| TYPENAME ID
	| parameters ',' parameter
	;

parameter: TYPENAME ID
	| TYPENAME ID '=' default_value
	| ID
	| ID '=' default_value
	;

default_value: ID
	| INT
	| STR
	;

ccall_declaration: KW_CCALL single_return FUNC_ID parameters ')' ';'
	;

syscall_definition: KW_SYSCALL INT ':' single_return FUNC_ID parameters ')' ';'
	;

single_return: /* empty */
	| TYPENAME
	;

toplv_statement: st_expression ';'
	| declarations ';'
	| declarations '=' expressions ';'
	| toplv_block
	;
	
toplv_block: '{' toplv_statement '}'
	;

block: '{' statements '}'
	;

statements:	/* empty */ { }
	| statements statement
	;

statement: st_expression ';'
	| declarations ';'
	| declarations '=' expressions ';'
	| return_stmt ';'
	| block
	;

st_expression: expression
	| assignment
	;

expressions: expression
	| expressions ',' expression
	;

expression:
	func_call
	| expression '+' expression
	| expression '-' expression
	| '(' assignment ')'
	| '-' expression %prec UMINUS
	| term
	;

func_call: FUNC_ID arguments ')'
	;

arguments: argument
	| arguments ',' argument
	;

argument: /* empty */ // ToDo: replace default
	| expression
	;

lvals:  ID
	| lvals ',' ID
	;

term: INT
	| STR
	| ID
	| '(' expression ')'
	;
	
assignment: lvals '=' expressions
	;
	
declarations: declaration
	| declarations ',' subdeclaration 
	| declarations ',' declaration 
	;

declaration: TYPENAME ID
	;

subdeclaration: ID
	;

return_stmt: KW_RETURN
	| KW_RETURN expressions
	;

%%

