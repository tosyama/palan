%skeleton "lalr1.cc"
%require "3.0.4"
%defines
%define parser_class_name {PlnParser}
%parse-param	{ PlnLexer& lexer } { PlnModule& module } { PlnScopeStack& scopes }
%lex-param		{ PlnLexer& lexer }

%code requires
{
#include <vector>
#include <string>
#include <iostream>
#include "PlnScopeStack.h"

using std::string;
using std::cerr;
using std::endl;
using std::vector;

class PlnLexer;
class PlnModule;
class PlnFunction;
class PlnBlock;
class PlnStatement;
class PlnExpression;
class PlnVarInit;
}

%code
{
int yylex(	palan::PlnParser::semantic_type* yylval,
			palan::PlnParser::location_type* location,
			PlnLexer& lexer);
}

%code top
{
#include <boost/assert.hpp>
#include "PlnModel.h"
#include "PlnMessage.h"
}

%locations
%define api.namespace {palan}
%define parse.error	verbose
%define api.value.type	variant

%token <int>	INT	"integer"
%token <string>	ID	"identifier"
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
%type <vector<PlnVarInit*>>	declarations
%type <PlnVarInit*>	declaration
%type <PlnVarInit*>	subdeclaration

%start module	
%%
module: /* empty */
	| module
		{
			scopes.push_back(PlnScopeItem(&module));
		}
		function_definition
	{
		module.functions.push_back($3);
		BOOST_ASSERT(scopes.back().type == SC_MODULE);
		scopes.pop_back();
	}
	;

function_definition: return_values func_name '(' parameters ')'
		{
			PlnFunction* f = new PlnFunction(FT_PLN, $2);
			f->setParent(scopes.back());
			scopes.push_back(PlnScopeItem(f));
		}
		block
	{
		BOOST_ASSERT(scopes.back().type == SC_FUNCTION);
		$$ = scopes.back().inf.function;
		$$->implement = $7;
		$$->finish();
		scopes.pop_back();
	}
;

return_values: ID	{ }
	;
func_name: ID		{ $$ = $1; }
	;
parameters: /* empty */
	| ID
	;

block: '{'
		{
			PlnBlock *b = new PlnBlock();
			b->setParent(scopes.back());
			scopes.push_back(PlnScopeItem(b));
		}
		statements '}'
	{
		BOOST_ASSERT(scopes.back().type == SC_BLOCK);
		$$ = scopes.back().inf.block;
		$$->statements = move($3);
		scopes.pop_back();
	}
	;

statements:	/* empty */ { }
	| statements statement
	{
		if ($2) $1.push_back($2);
		$$ = move($1);
	}
	;

statement: expression ';'
	{
		BOOST_ASSERT(scopes.back().type == SC_BLOCK);
		$$ = new PlnStatement($1, scopes.back().inf.block);
	}

	| declarations ';'
	{
		BOOST_ASSERT(scopes.back().type == SC_BLOCK);
		if ($1.size()) $$ = new PlnStatement($1, scopes.back().inf.block);
		else $$ = NULL;
	}

	| block
	{
		BOOST_ASSERT(scopes.back().type == SC_BLOCK);
		$$ = new PlnStatement($1, scopes.back().inf.block);
	}
	;

func_call: func_name '(' arguments ')'
	{
		PlnFunction* f = module.getFunc($1);
		if (f) {
			PlnFunctionCall* fc = new PlnFunctionCall();
			fc->type = ET_FUNCCALL;
			fc->function = f;
			fc->arguments = move($3);
			$$ = fc;
		} else {
			error(@$, PlnMessage::getErr(E_UndefinedFunction, $1));
			YYABORT;
		}
	}
	;

arguments: argument
	{
		$$.push_back($1);
	}

	| arguments ',' argument
	{
		$1.push_back($3);
		$$ = move($1);
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
		$$  = new PlnExpression(PlnValue($1));
	}

	| STR
	{
		$$ = new PlnExpression(PlnValue(module.getReadOnlyData($1)));
	}

	| ID
	{
		PlnVariable *v = scopes.back().inf.block->getVariable($1);
		if (v) $$ = new PlnExpression(PlnValue(v));
		else {
			error(@$, PlnMessage::getErr(E_UndefinedVariable, $1));
			YYABORT;
		}
	}

	| func_call
	{
		$$ = $1;
	}

	| lvals '=' expression
	{
		$$ = NULL;
	}
	;

lvals: ID
	| lvals ',' ID
	;

declarations: declaration
	{
		if ($1)	$$.push_back($1);
	}

	| declarations ',' subdeclaration 
	{
		$$ = move($1);
		if ($3) $$.push_back($3);
	}

	| declarations ',' declaration 
	{
		$$ = move($1);
		if ($3) $$.push_back($3);
	}
	;

declaration: ID ID
	{
		PlnType* t = module.getType($1);
		if (!t) {
			error(@$, PlnMessage::getErr(E_UndefinedType, $1));
			YYABORT;
		}
		PlnBlock* b = scopes.back().inf.block;
		if (!b->declareVariable($2, t)) {
			error(@$, PlnMessage::getErr(E_DuplicateVarName, $2));
			YYABORT;
		}
		$$ = NULL;
	}

	| ID ID '=' expression
	{
		PlnType* t = module.getType($1);
		if (!t) {
			error(@$, PlnMessage::getErr(E_UndefinedType, $1));
			YYABORT;
		}
		PlnVariable* v = scopes.back().inf.block->declareVariable($2, t);
		if (!v) {
			error(@$, PlnMessage::getErr(E_DuplicateVarName, $2));
			YYABORT;
		}
		$$ = new PlnVarInit(v, $4);
	}
	;

subdeclaration: ID
	{
		PlnBlock* b = scopes.back().inf.block;
		if (!b->declareVariable($1)) {
			error(@$, PlnMessage::getErr(E_DuplicateVarName, $1));
			YYABORT;
		}
		$$ = NULL;
	}

	| ID '=' expression
	{
		PlnBlock* b = scopes.back().inf.block;
		PlnVariable* v = b->declareVariable($1);
		if (!v) {
			error(@$, PlnMessage::getErr(E_DuplicateVarName, $1));
			YYABORT;
		}
		$$ = new PlnVarInit(v, $3);
	}
	;

%%

namespace palan 
{

void PlnParser::error(const location_type& l, const string& m)
{
	cerr << "error: " << l << ": " << m << endl;
}

} // namespace

