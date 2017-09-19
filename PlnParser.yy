/// Palan Parser.
///
/// Bulid Palan model tree from lexer input.
/// Palan parser (PlnParser.cpp, PlnParser.hpp and related files)
/// is created from this definition file by bison.
///
/// @file	PlnParser.yy
/// @copyright	2017- YAMAGUCHI Toshinobu 

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
#include "PlnModel.h"
#include "PlnScopeStack.h"

using std::string;
using std::cerr;
using std::endl;
using std::vector;

class PlnLexer;
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
#include "models/PlnModule.h"
#include "models/PlnFunction.h"
#include "models/PlnBlock.h"
#include "models/PlnStatement.h"
#include "models/PlnType.h"
#include "models/PlnVariable.h"
#include "models/expressions/PlnMultiExpression.h"
#include "models/expressions/PlnFunctionCall.h"
#include "models/expressions/PlnAddOperation.h"
#include "models/expressions/PlnAssignment.h"
#include "PlnMessage.h"

#define CUR_BLOCK	scopes.back().inf.block
#define CUR_FUNC	searchFunction(scopes)
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
%token KW_RETURN	"return"

%type <string>	func_name
%type <PlnFunction*>	function_definition
%type <PlnFunction*>	ccall_declaration
%type <PlnFunction*>	syscall_definition
%type <vector<PlnVariable*>>	func_return
%type <vector<PlnVariable*>>	return_values
%type <PlnVariable*>	return_value
%type <PlnParameter*>	parameter
%type <PlnValue*>	default_value
%type <PlnBlock*>	block
%type <vector<PlnStatement*>>	statements
%type <PlnStatement*>	statement
%type <vector<PlnExpression*>>	expressions
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
%type <PlnReturnStmt*>	return_stmt

%right '='
%left ',' 
%left '+' '-'
%left UMINUS

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
			f->setParent(&module);
			f->setRetValues($1);
			scopes.push_back(PlnScopeItem(f));
		}
		'(' parameters ')' block
	{
		BOOST_ASSERT(scopes.back().type == SC_FUNCTION);
		$$ = scopes.back().inf.function;
		$$->implement = $7;
		scopes.pop_back();
	}
;

func_name: ID		{ $$ = $1; }
	;

func_return: KW_VOID { }

	| return_values
	{
		$$ = move($1);
	}
	;

return_values: return_value
	{
		$$.push_back($1);
	}

	| return_values ',' return_value
	{
		$$ = move($1);
		for (auto v: $$)
			if (v->name == $3->name) {
				error(@$, PlnMessage::getErr(E_DuplicateVarName, $3->name));
				delete $3;
				YYABORT;
			}
		$$.push_back($3);
	}
	;

return_value: ID ID
	{
		PlnType* t = module.getType($1);
		if (!t) {
			error(@$, PlnMessage::getErr(E_UndefinedType, $1));
			YYABORT;
		}
		$$ = new PlnVariable();
		$$->name = $2;
		$$->var_type = t;
	}
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
		$$ = f->addParam($2, t, $4);
		if (!$$) {
			error(@$, PlnMessage::getErr(E_DuplicateVarName, $2));
			YYABORT;
		}
	}
	;

default_value: ID
	{	
		$$ = NULL; // TODO: get const value.
	}

	| INT
	{
		$$ = new PlnValue($1);
	}

	| STR
	{
		$$ = new PlnValue(module.getReadOnlyData($1));
	}
	;

ccall_declaration: KW_CCALL single_return func_name '(' parameters ')' ';'
	{
		PlnFunction* f = new PlnFunction(FT_C, $3);
		f->setParent(&module);
		$$ = f;
	}
	;

syscall_definition: KW_SYSCALL INT ':' single_return func_name '(' parameters ')' ';'
	{
		PlnFunction* f = new PlnFunction(FT_SYS, $5);
		f->inf.syscall.id = $2;
		f->setParent(&module);
		$$ = f;
	}
	;

single_return: KW_VOID
	| ID
	;

block: '{'
		{
			PlnBlock *b = new PlnBlock();
			if (scopes.back().type == SC_BLOCK)
				b->setParent(scopes.back().inf.block);
			else {
				BOOST_ASSERT(scopes.back().type == SC_FUNCTION);
				b->setParent(scopes.back().inf.function);
			}
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
		$$ = new PlnStatement(new PlnVarInit($1), CUR_BLOCK);
	}

	| declarations '=' expressions ';'
	{
		int count=0;
		for (auto e: $3)
			count+=e->values.size();

		if ($1.size() != count) {
			error(@$, PlnMessage::getErr(E_NumOfLRVariables));
			YYABORT;
		}
		BOOST_ASSERT(scopes.back().type == SC_BLOCK);
		$$ = new PlnStatement(new PlnVarInit($1, $3), CUR_BLOCK);
	}
	
	| return_stmt ';'
	{
		$$ = $1;	
	}

	| block
	{
		BOOST_ASSERT(scopes.back().type == SC_BLOCK);
		$$ = new PlnStatement($1, CUR_BLOCK);
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

expressions: expression
	{
		$$.push_back($1);
	}

	| expressions ',' expression
	{
		$$ = move($1);
		$$.push_back($3);
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

	| expression '-' expression
	{
		$$ = PlnAddOperation::create_sub($1, $3);
	}

	| '(' assignment ')'
	{
		$$ = $2;
	}

	| '-' expression %prec UMINUS
	{
		$$ = PlnNegative::create($2);
	}
	
	| term
	{
		$$ = $1;
	}
	;

func_call: func_name '(' arguments ')'
	{
		PlnFunction* f = module.getFunc($1, $3);
		if (f) {
			$$ = new PlnFunctionCall(f, $3);
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

argument: /* empty */
	{
		$$ = NULL;
	}

	| expression
	{
		$$ = $1;
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

assignment: lvals '=' expressions
	{
		int count=0;
		for (auto e: $3)
			count+=e->values.size();
		if ($1.size() != count) {
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

return_stmt: KW_RETURN
	{
		vector<PlnExpression*> empty;
		$$ = new PlnReturnStmt(empty, CUR_BLOCK);
		// TODO: check vals were set.
	}

	| KW_RETURN expressions
	{
		// TODO: type check
		int count=0;
		for (auto e: $2)
			count+=e->values.size();
		$$ = new PlnReturnStmt($2, CUR_BLOCK);
		if ($$->function->return_vals.size() != count)
		{
			error(@$, PlnMessage::getErr(E_NumOfRetValues));
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

