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
%token TYPENAME
%token KW_FUNC
%token KW_CCALL
%token KW_SYSCALL
%token KW_RETURN
%token KW_WHILE
%token KW_IF
%token KW_ELSE
%token KW_CONST
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

%right '='
%left ARROW DBL_ARROW
%left ',' 
%left OPE_OR
%left OPE_AND
%left DBL_LESS DBL_GRTR
%left OPE_EQ OPE_NE
%left '<' '>' OPE_LE OPE_GE
%left '+' '-'
%left '*' '/' '%'
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
	| return_types return_type
	;

return_type: type_def
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
	| parameters ',' move_owner ID default_value
	;

parameter: type_def move_owner ID default_value
	;

move_owner: /* empty */
	| DBL_GRTR
	;

take_owner: /* empty */
	| DBL_LESS
	;

default_value: /* empty */
	| '=' ID
	| '=' INT
	| '=' UINT
	| '=' STR
	;

ccall_declaration: KW_CCALL single_return FUNC_ID '(' parameter_def ')' ';'
	;

syscall_definition: KW_SYSCALL INT ':' single_return FUNC_ID '(' parameter_def ')' ';'
	;

single_return: /* empty */
	| type_def
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
	| declarations
	| declarations '=' expression
	| const_def
	| return_stmt
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
	;

arguments: argument
	| arguments ',' argument
	;

argument: /* empty */
	| expression move_owner
	;

dst_vals: dst_val
	| dst_vals ',' dst_val
	;

dst_val: move_owner unary_expression 
	;

unary_expression: ID
	| unary_expression array_item
	;

term: INT
	| UINT
	| STR
	| unary_expression
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

declarations: declaration
	| declarations ',' subdeclaration 
	| declarations ',' declaration 
	;

declaration: type_def ID take_owner
	;

subdeclaration: ID take_owner
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
	| expression
	;

array_item: '[' array_indexes ']'
	;

array_indexes: array_index
	| array_indexes ',' array_index
	;

array_index: expression
	;

const_def: KW_CONST const_names '=' expressions;
const_names: ID
	| const_names ',' ID
	;

%%

