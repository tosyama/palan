%skeleton "lalr1.cc"
%require "3.0.4"
%defines
%define parser_class_name {PlnParser}
%parse-param	{ PlnLexer &lexer }
%lex-param		{ PlnLexer &lexer }

%code requires
{
#include <string>
#include <iostream>

using std::string;
using std::cout;
using std::endl;

class PlnLexer;
}

%code
{
int yylex(palan::PlnParser::semantic_type* yylval, PlnLexer& lexer);
}

%define api.namespace {palan}
%define parse.error	verbose
%define api.value.type	variant

%token <int>	INT
%token <string>	ID "identifier"
%token <string>	STR	"string"
%type <string> func_name

%start module	
%%
module: /* empty */
	| module function_definition
	;
function_definition: return_values func_name '(' parameters ')' block
	{ cout << "func def:" << $2 << endl; }
;

return_values: ID	{ cout << "ret values:" << $1 << endl; };
	;
func_name: ID		{ $$ = $1; }
	;
parameters: /* empty */
	| ID
	;
block: '{' statements '}'
	;
statements:	/* empty */
	| statements statement
	;
statement: expression ';'
	| block
	;
func_call: func_name '(' arguments ')'	{ cout << "call: " << $1 << endl; }
	;
arguments: argument
	| arguments ',' argument
	;
argument: /* empty */
	| expression
	;
expression: INT
	| STR { cout << $1 << endl; }
	| func_call
	;
%%

namespace palan 
{

void PlnParser::error(const string& m)
{
	cout << "error: " << m << endl;
}

} // namespace

