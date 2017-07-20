%skeleton "lalr1.cc"
%require "3.0.4"
%defines
%define parser_class_name {PlnParser}
%parse-param	{ PlnLexer& lexer } { PlnModule& module }
%lex-param		{ PlnLexer& lexer }

%code requires
{
#include <vector>
#include <string>
#include <iostream>

using std::string;
using std::cout;
using std::endl;
using std::vector;

class PlnLexer;
class PlnModule;
class PlnFunction;
class PlnBlock;
class PlnStatement;
class PlnExpression;

}

%code
{
int yylex(	palan::PlnParser::semantic_type* yylval,
			palan::PlnParser::location_type* location,
			PlnLexer& lexer);
}

%code top
{
#include "PlnModel.h"
}

%locations
%define api.namespace {palan}
%define parse.error	verbose
%define api.value.type	variant

%token <int>	INT
%token <string>	ID "identifier"
%token <string>	STR	"string"

%type <string>	func_name
%type <PlnFunction*>	function_definition
%type <PlnBlock*>	block
%type <vector<PlnStatement*>>	statements
%type <PlnStatement*>	statement
%type <PlnExpression*>	expression
%type <PlnExpression*>	func_call
%type <vector<PlnExpression*>>	arguments
%type <PlnExpression*>	argument

%start module	
%%
module: /* empty */
	| module function_definition
	{
		module.addFunc(*$2);
	}
	;
function_definition: return_values func_name '(' parameters ')' block
	{
		$$ = new PlnFunction($2);
		$$->type = FT_PLN;
		$$->implement = $6;
	}
;

return_values: ID	{ }
	;
func_name: ID		{ $$ = $1; }
	;
parameters: /* empty */
	| ID
	;

block: '{' statements '}'
	{
		$$ = new PlnBlock();
		$$->statements = move($2);
	}
	;
statements:	/* empty */ { }
	| statements statement
	{
		$1.push_back($2);
		$$ = move($1);
	}
	;
statement: expression ';'
	{
		$$ = new PlnStatement();
		$$->type = ST_EXPRSN;
		$$->inf.expression = $1;
	}

	| block
	{
		$$ = new PlnStatement();
		$$->type = ST_BLOCK;
		$$->inf.block = $1;
	}
	;

func_call: func_name '(' arguments ')'
	{
		PlnFunctionCall* fc = new PlnFunctionCall();
		fc->type = ET_FUNCCALL;
		fc->function = module.getFunc($1);
		fc->arguments = move($3);
		$$ = fc;
	}
	;

arguments: argument
	{
		$$.push_back($1);
	}

	| arguments ',' argument
	{
		$$.push_back($3);
	}
	;
argument: /* empty */ // ToDo: replace default
	| expression
	{
		$$ = $1;
	}
	;
expression: INT
	{
		$$  = new PlnExpression();
		$$->type = ET_VALUE;
		$$->value.type = VL_LIT_INT8;
		$$->value.inf.intValue = $1;
	}

	| STR
	{
		$$ = new PlnExpression();
		$$->type = ET_VALUE;
		$$->value.type = VL_RO_DATA;
		$$->value.inf.rod = module.getReadOnlyData($1);
	}

	| func_call
	{
		$$ = $1;
	}
	;
%%

namespace palan 
{

void PlnParser::error(const location_type& l, const string& m)
{
	cout << "error: " << l << ":" << m << endl;
}

} // namespace

