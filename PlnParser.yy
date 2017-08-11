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
class PlnValue;
class PlnVariable;
class PlnParameter;
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

#define CUR_BLOCK	scopes.back().inf.block
}

%locations
%define api.namespace {palan}
%define parse.error	verbose
%define api.value.type	variant

%token <int>	INT	"integer"
%token <string>	ID	"identifier"
%token <string>	STR	"string"
%token KW_CCALL	"ccall"
%token KW_SYSCALL	"syscall"
%token KW_VOID	"void"

%type <string>	func_name
%type <PlnFunction*>	function_definition
%type <PlnFunction*>	ccall_declaration
%type <PlnFunction*>	syscall_definition
%type <PlnParameter*>	parameter
%type <PlnBlock*>	block
%type <vector<PlnStatement*>>	statements
%type <PlnStatement*>	statement
%type <PlnExpression*>	expressions
%type <PlnExpression*>	expression
%type <PlnExpression*>	st_expression
%type <PlnExpression*>	assignment
%type <PlnExpression*>	term
%type <PlnExpression*>	func_call
%type <vector<PlnExpression*>>	arguments
%type <PlnExpression*>	argument
%type <vector<PlnValue>>	lvals
%type <vector<PlnVariable*>>	declarations
%type <PlnVariable*>	declaration
%type <PlnVariable*>	subdeclaration

%right '='
%left ',' 
%left '+'

%start module	

%%
module: /* empty */
	{
		scopes.push_back(PlnScopeItem(&module));
	}

	| module function_definition
	{
		module.functions.push_back($2);
		BOOST_ASSERT(scopes.back().type == SC_MODULE);
	}

	| module ccall_declaration
	{
		module.functions.push_back($2);
	}

	| module syscall_definition
	{
		module.functions.push_back($2);
	}
	;

function_definition: func_return func_name
		{
			PlnFunction* f = new PlnFunction(FT_PLN, $2);
			f->setParent(scopes.back());
			scopes.push_back(PlnScopeItem(f));
		}
		'(' parameters ')'
		{
			BOOST_ASSERT(scopes.back().type == SC_FUNCTION);
			PlnFunction* f = scopes.back().inf.function;
		}
		block
	{
		BOOST_ASSERT(scopes.back().type == SC_FUNCTION);
		$$ = scopes.back().inf.function;
		$$->implement = $8;
		scopes.pop_back();
	}
;

func_name: ID		{ $$ = $1; }
	;

func_return: KW_VOID
	| return_values
	;

return_values: return_value
	| return_values ',' return_value
	;

return_value: ID ID
	;

parameters: /* empty */
	| parameter
	| parameters ',' parameter
	;

parameter: ID ID
	{
		PlnType* t = module.getType($1);
		if (!t) {
			error(@$, PlnMessage::getErr(E_UndefinedType, $1));
			YYABORT;
		}
		BOOST_ASSERT(scopes.back().type == SC_FUNCTION);
		PlnFunction* f = scopes.back().inf.function;
		$$ = f->addParam($2, t);
		if (!$$) {
			error(@$, PlnMessage::getErr(E_DuplicateVarName, $2));
			YYABORT;
		}
	}

	| ID ID '=' default_value
	{
		PlnType* t = module.getType($1);
		if (!t) {
			error(@$, PlnMessage::getErr(E_UndefinedType, $1));
			YYABORT;
		}
		BOOST_ASSERT(scopes.back().type == SC_FUNCTION);
		PlnFunction* f = scopes.back().inf.function;
		$$ = f->addParam($2, t);
		if (!$$) {
			error(@$, PlnMessage::getErr(E_DuplicateVarName, $2));
			YYABORT;
		}
	}

	;

default_value: ID
	| INT
	| STR
	;

ccall_declaration: KW_CCALL single_return func_name '(' parameters ')' ';'
	{
		PlnFunction* f = new PlnFunction(FT_C, $3);
		f->setParent(scopes.back());
		$$ = f;
	}
	;

syscall_definition: KW_SYSCALL INT ':' single_return func_name '(' parameters ')' ';'
	{
		PlnFunction* f = new PlnFunction(FT_SYS, $5);
		f->inf.syscall.id = $2;
		f->setParent(scopes.back());
		$$ = f;
	}
	;

single_return: KW_VOID
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
		$$ = CUR_BLOCK;
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

statement: st_expression ';'
	{
		BOOST_ASSERT(scopes.back().type == SC_BLOCK);
		$$ = new PlnStatement($1, CUR_BLOCK);
	}

	| declarations ';'
	{
		BOOST_ASSERT(scopes.back().type == SC_BLOCK);
		$$ = new PlnStatement(new PlnVarInit($1, NULL), CUR_BLOCK);
	}

	| declarations '=' expressions ';'
	{
		if ($1.size() != $3->values.size()) {
			error(@$, PlnMessage::getErr(E_NumOfLRVariables));
			YYABORT;
		}
		BOOST_ASSERT(scopes.back().type == SC_BLOCK);
		$$ = new PlnStatement(new PlnVarInit($1, $3), CUR_BLOCK);
	}

	| block
	{
		BOOST_ASSERT(scopes.back().type == SC_BLOCK);
		$$ = new PlnStatement($1, CUR_BLOCK);
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

expressions: expression
	{
		$$ = $1;
	}

	| expressions ',' expression
	{
		if ($1->type == ET_MULTI) {
			$$ = $1;
			static_cast<PlnMultiExpression*>($$)->append($3);
		} else {
			$$ = new PlnMultiExpression($1, $3);
		}
	}
	;

lvals:  ID
	{
		PlnVariable* v=CUR_BLOCK->getVariable($1);
		if (v) $$.push_back(v);
		else {
			error(@$, PlnMessage::getErr(E_UndefinedVariable,$1));
			YYABORT;
		}
	}

	| lvals ',' ID
	{
		$$ = move($1);
		PlnVariable* v=CUR_BLOCK->getVariable($3);
		if (v) $$.push_back(v);
		else {
			error(@$, PlnMessage::getErr(E_UndefinedVariable,$3));
			YYABORT;
		}
	}
	;

term: INT
	{
		$$  = new PlnExpression(PlnValue($1));
	}

	| STR
	{
		$$ = new PlnExpression(PlnValue(module.getReadOnlyData($1)));
	}

	| ID
	{
		PlnVariable *v = CUR_BLOCK->getVariable($1);
		if (v) $$ = new PlnExpression(PlnValue(v));
		else {
			error(@$, PlnMessage::getErr(E_UndefinedVariable, $1));
			YYABORT;
		}
	} 

	| '(' expression ')'
	{
		$$ = $2;
	}
	;
	
expression:
	func_call
	{
		$$ = $1;
	}

	| expression '+' expression
	{
		$$ = PlnAddOperation::create($1, $3);
	}

	| '(' assignment ')'
	{
		$$ = $2;
	}
	
	| term
	{
		$$ = $1;
	}
	;

st_expression: expression
	{
		$$ = $1;
	}
	| assignment
	{
		$$ = $1;
	}
	;

assignment: lvals '=' expressions
	{
		if ($1.size() != $3->values.size()) {
			error(@$, PlnMessage::getErr(E_NumOfLRVariables));
			YYABORT;
		}
		$$ = new PlnAssignment($1, $3);
	}
	;
	
declarations: declaration
	{
		$$.push_back($1);
	}

	| declarations ',' subdeclaration 
	{
		$$ = move($1);
		$$.push_back($3);
	}

	| declarations ',' declaration 
	{
		$$ = move($1);
		$$.push_back($3);
	}
	;

declaration: ID ID
	{
		PlnType* t = module.getType($1);
		if (!t) {
			error(@$, PlnMessage::getErr(E_UndefinedType, $1));
			YYABORT;
		}
		$$ = CUR_BLOCK->declareVariable($2, t);
		if (!$$) {
			error(@$, PlnMessage::getErr(E_DuplicateVarName, $2));
			YYABORT;
		}
	}
	;

subdeclaration: ID
	{
		$$ = CUR_BLOCK->declareVariable($1);
		if (!$$) {
			error(@$, PlnMessage::getErr(E_DuplicateVarName, $1));
			YYABORT;
		}
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

