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
			f->type = FT_PLN;
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
		if ($2->isEmpty()) {
			//TODO: free inner memory.
		} else {
			$1.push_back($2);
		}
		$$ = move($1);
	}
	;
statement: expression ';'
	{
		BOOST_ASSERT(scopes.back().type == SC_BLOCK);
		$$ = new PlnStatement();
		$$->type = ST_EXPRSN;
		$$->inf.expression = $1;
		$$->parent = scopes.back().inf.block;
	}

	| declarations ';'
	{
		BOOST_ASSERT(scopes.back().type == SC_BLOCK);
		$$ = new PlnStatement();
		$$->type = ST_VARINIT;
		$$->parent = scopes.back().inf.block;
		$$->inf.var_inits = new vector<PlnVarInit*>();
		*$$->inf.var_inits =  move($1);
	}

	| block
	{
		BOOST_ASSERT(scopes.back().type == SC_BLOCK);
		$$ = new PlnStatement();
		$$->type = ST_BLOCK;
		$$->inf.block = $1;
		$$->parent = scopes.back().inf.block;
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
		$$  = new PlnExpression();
		$$->type = ET_VALUE;
		$$->values.push_back(PlnValue());
		$$->values.back().type = VL_LIT_INT8;
		$$->values.back().inf.intValue = $1;
	}

	| STR
	{
		$$ = new PlnExpression();
		$$->type = ET_VALUE;
		$$->values.push_back(PlnValue());
		$$->values.back().type = VL_RO_DATA;
		$$->values.back().inf.rod = module.getReadOnlyData($1);
	}

	| ID
	{
		PlnVariable *v = scopes.back().inf.block->getVariable($1);
		if (v) {
			$$ = new PlnExpression();
			$$->type = ET_VALUE;
			$$->values.push_back(PlnValue());
			$$->values.back().type = VL_VAR;
			$$->values.back().inf.var = v;
		} else {
			error(@$, PlnMessage::getErr(E_UndefinedVariable, $1));
			YYABORT;
		}
	}

	| func_call
	{
		$$ = $1;
	}
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
		PlnBlock* b = scopes.back().inf.block;
		b->declareVariable($2, $1);
		$$ = NULL;
	}

	| ID ID '=' expression
	{
		PlnBlock* b = scopes.back().inf.block;
		b->declareVariable($2, $1);
		PlnVarInit* vi = new PlnVarInit();
		vi->vars.push_back(b->variables.back());
		vi->initializer = $4;
		$$ = vi;
	}
	;

subdeclaration: ID
	{
		PlnBlock* b = scopes.back().inf.block;
		b->declareVariable($1);
		$$ = NULL;
	}

	| ID '=' expression
	{
		PlnBlock* b = scopes.back().inf.block;
		b->declareVariable($1);
		PlnVarInit* vi = new PlnVarInit();
		vi->vars.push_back(b->variables.back());
		vi->initializer = $3;
		$$ = vi;
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

