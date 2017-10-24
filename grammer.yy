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

%token INT
%token UINT
%token STR
%token ID
%token TYPENAME
%token FUNC_ID
%token KW_FUNC
%token KW_CCALL
%token KW_SYSCALL
%token KW_RETURN
%token DBL_LESS

%right '='
%left ',' 
%left '+' '-'
%left '*' '/' '%'
%left UMINUS

%start module	
%%
module: /* empty */
	| module function_definition
	| module ccall_declaration
	| module syscall_definition
	| module toplv_statement
	;

function_definition: KW_FUNC return_def FUNC_ID parameter_def ')' block
	;

return_def: /* empty */
	| return_types
	| return_values
	;

return_types: type_def
	| return_types type_def
	;

return_values: return_value
	| return_values ',' return_value
	| return_values ',' ID
	;

return_value: type_def ID
	;

parameter_def: /* empty */
	| parameters
	;

parameters: parameter
	| parameters ',' parameter
	| parameters ',' move_owner_suffix ID default_value
	;

parameter: type_def move_owner_suffix ID default_value
	;

move_owner_suffix: /* empty */
	| DBL_LESS
	;

default_value: /* empty */
	| '=' ID
	| '=' INT
	| '=' UINT
	| '=' STR
	;

ccall_declaration: KW_CCALL single_return FUNC_ID parameter_def ')' ';'
	;

syscall_definition: KW_SYSCALL INT ':' single_return FUNC_ID parameter_def ')' ';'
	;

single_return: /* empty */
	| type_def
	;

toplv_statement: basic_statement
	| toplv_block
	;

basic_statement: st_expression ';'
	| declarations ';'
	| declarations '=' expression ';'
	;
	
toplv_block: '{' toplv_statements '}'
	;

toplv_statements:	/* empty */
	| toplv_statements toplv_statement
	;

block: '{' statements '}'
	;

statements:	/* empty */ { }
	| statements statement
	;

statement: basic_statement
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
	| expression '*' expression
	| expression '/' expression
	| expression '%' expression
	| '(' assignment ')'
	| '-' expression %prec UMINUS
	| term
	;

func_call: FUNC_ID arguments ')'
	;

arguments: argument
	| arguments ',' argument
	;

argument: /* empty */
	| move_owner_suffix expression
	;

lvals: lval
	| lvals ',' lval
	;

lval: ID move_owner_suffix;

term: INT
	| UINT
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

declaration: type_def ID
	;

subdeclaration: ID
	;

return_stmt: KW_RETURN
	| KW_RETURN expressions
	;

type_def: TYPENAME
	| type_def array_def
	| type_def '@'
	;
	
array_def: '[' array_sizes ']'
	;

array_sizes: array_size
	| array_sizes ',' array_size
	;

array_size: /* empty */
	| INT | ID
	;

%%

