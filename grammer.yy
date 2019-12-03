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
%token FUNC_ID
%token KW_TYPE
%token KW_FUNC
%token KW_CCALL
%token KW_SYSCALL
%token KW_RETURN
%token KW_WHILE
%token KW_BREAK
%token KW_CONTINUE
%token KW_IF
%token KW_ELSE
%token KW_CONST
%token KW_AUTOTYPE
%token KW_VARLENARG
%token OPE_EQ
%token OPE_NE
%token OPE_LE
%token OPE_GE
%token OPE_AND
%token OPE_OR
%token DBL_LESS
%token DBL_GRTR
%token DBL_ARROW
%token ARROW
%token EQ_ARROW

%right '='
%left ARROW DBL_ARROW EQ_ARROW
%left ',' 
%left OPE_OR
%left OPE_AND
%left DBL_LESS DBL_GRTR
%left OPE_EQ OPE_NE
%left '<' '>' OPE_LE OPE_GE
%left '+' '-' '|'
%left '*' '/' '%' '&' '@'
%left UMINUS '!'

%start module	
%%
module: /* empty */
	| module statement
	;

palan_function_definition: KW_FUNC FUNC_ID '(' parameter_def ')' return_def block
	;

return_def: /* empty */
	| ARROW return_types
	| ARROW return_values
	;

return_types: return_type
	| return_types ',' return_type
	;

return_type: type
	;

return_values: return_value
	| return_values ',' return_value
	| return_values ',' ID
	;

return_value: type ID
	;

parameter_def: /* empty */
	| KW_VARLENARG
	| parameters ',' KW_VARLENARG
	| parameters
	;

parameters: parameter
	| parameters ',' parameter
	| parameters ',' ID default_value
	| parameters ',' pass_by ID default_value
	;

parameter: '@'
	| type ID default_value
	| type pass_by ID default_value
	;

out_parameter_def:
	| EQ_ARROW out_parameters
	| EQ_ARROW out_parameters ',' KW_VARLENARG
	| EQ_ARROW KW_VARLENARG
	;

out_parameters: type ID
	| out_parameters ',' type ID
	| out_parameters ',' ID
	;

move_owner: /* empty */
	| DBL_GRTR
	;

pass_by: DBL_GRTR
	;

default_value: /* empty */
	| '=' ID
	| '=' INT
	| '=' UINT
	| '=' STR
	;

ccall_declaration: KW_CCALL FUNC_ID '(' parameter_def out_parameter_def ')' single_return at_lib';'
	;

syscall_definition: KW_SYSCALL INT ':' FUNC_ID '(' parameter_def ')' single_return ';'
	;

single_return: /* empty */
	| ARROW type
	;

at_lib: /* empty */
	| ':' ID
	;

function_definition: palan_function_definition
	| ccall_declaration
	| syscall_definition
	;
	
block: '{' statements '}'
	| '{' statements semi_stmt '}'
	;

while_statement: KW_WHILE st_expression block
	;

break_stmt: KW_BREAK
	;

continue_stmt: KW_CONTINUE
	;

if_statement: KW_IF st_expression block else_statement
	;

else_statement:	/* empty */
	| KW_ELSE block
	| KW_ELSE if_statement
	;

statements:	/* empty */ { }
	| statements statement
	;

statement: semi_stmt ';'
	| while_statement
	| if_statement
	| function_definition
	| block
	;

semi_stmt: st_expression
	| type_def
	| declarations
	| declarations '=' expressions
	| ID take_owner1 '=' expression 
	| const_def
	| return_stmt
	| break_stmt
	| continue_stmt
	;

take_owner1: /* empty */
	| DBL_LESS
	;

st_expression: expression
	| assignment
	| chain_call
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
	| expression '|' expression
	| expression '&' expression
	| expression OPE_EQ expression
	| expression OPE_NE expression
	| expression '<' expression
	| expression '>' expression
	| expression OPE_LE expression
	| expression OPE_GE expression
	| expression OPE_AND expression
	| expression OPE_OR expression
	| '(' assignment ')'
	| '-' expression %prec UMINUS
	| '!' expression
	| term
	;

func_call: FUNC_ID '(' arguments ')'
	| FUNC_ID '(' arguments EQ_ARROW out_arguments ')'
	;

arguments: argument
	| arguments ',' argument
	;

argument: /* empty */
	| expression move_owner
	;

out_arguments: out_argument
	| out_arguments ',' out_argument
	;

out_argument: expression
	;

dst_vals: dst_val
	| dst_vals ',' dst_val
	;

dst_val: move_owner var_expression
	;

var_expression: type_or_var
	;

array_val: '[' expressions ']'
	| array_val '[' expressions ']'
	;

term: INT
	| UINT
	| STR
	| var_expression
	| array_val
	| '(' expression ')'
	;

assignment: expressions arrow_ope dst_vals
	| assignment arrow_ope dst_vals
	| chain_call arrow_ope dst_vals
	;

arrow_ope: ARROW
	| DBL_ARROW
	;

chain_call: expressions arrow_ope func_call
	| chain_call arrow_ope func_call
	| assignment arrow_ope func_call
	;

type_def: KW_TYPE ID
	| KW_TYPE ID '{' struct_def '}'
	| KW_TYPE ID '=' type 
	;

struct_def: type ID
	| struct_def ';' type ID
	| struct_def ';'
	;

declarations: declaration
	| declarations ',' subdeclaration 
	| declarations ',' declaration 
	;

declaration: var_type ID take_owner
	;

subdeclaration: ID take_owner
	;

take_owner: /* empty */
	| DBL_LESS
	;

return_stmt: KW_RETURN
	| KW_RETURN expressions
	;

var_type: KW_AUTOTYPE
	| type
	;

type: type_or_var
	;

type_or_var: ID
	| type_or_var '.' ID
	| type_or_var '[' array_items ']'
	| type_or_var '@'
	;

array_items: array_item
	| array_items ',' array_item
	;

array_item: /* empty */
	| expression
	| '?'
	;

const_def: KW_CONST const_names '=' expressions;
const_names: ID
	| const_names ',' ID
	;

%%

