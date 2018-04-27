/// Palan Parser.
///
/// Bulid Palan model tree from lexer input.
/// Palan parser (PlnParser.cpp, PlnParser.hpp and related files)
/// is created from this definition file by bison.
///
/// @file	PlnParser.yy
/// @copyright	2017 YAMAGUCHI Toshinobu 

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
#include "models/PlnExpression.h"	// for sizeof(PlnValue)

using std::string;
using std::cerr;
using std::endl;
using std::vector;

class PlnLexer;
}

%code top
{
#include <boost/assert.hpp>
#include "models/PlnModule.h"
#include "models/PlnFunction.h"
#include "models/PlnBlock.h"
#include "models/PlnStatement.h"
#include "models/PlnLoopStatement.h"
#include "models/PlnConditionalBranch.h"
#include "models/PlnType.h"
#include "models/PlnVariable.h"
#include "models/PlnArray.h"
#include "models/expressions/PlnFunctionCall.h"
#include "models/expressions/PlnAddOperation.h"
#include "models/expressions/PlnMulOperation.h"
#include "models/expressions/PlnDivOperation.h"
#include "models/expressions/PlnArrayItem.h"
#include "models/expressions/PlnAssignment.h"
#include "models/expressions/PlnBoolOperation.h"
#include "PlnMessage.h"
#include "PlnConstants.h"

#define CUR_BLOCK	scopes.back().inf.block
#define CUR_FUNC	searchFunction(scopes)
}

%code
{
#include "PlnLexer.h"

int yylex(	palan::PlnParser::semantic_type* yylval,
			palan::PlnParser::location_type* location,
			PlnLexer& lexer);
namespace palan {
static void warn(const PlnParser::location_type& l, const string& m);
}

}

%locations
%define api.namespace {palan}
%define parse.error	verbose
%define api.value.type	variant

%token <int64_t>	INT	"integer"
%token <uint64_t>	UINT	"unsigned integer"
%token <string>	STR	"string"
%token <string>	ID	"identifier"
%token <string>	TYPENAME	"type name"
%token KW_FUNC	"func"
%token KW_CCALL	"ccall"
%token KW_SYSCALL	"syscall"
%token KW_RETURN	"return"
%token KW_WHILE	"while"
%token KW_IF	"if"
%token KW_ELSE	"else"
%token OPE_EQ	"=="
%token OPE_NE	"!="
%token OPE_LE	"<="
%token OPE_GE	">="
%token OPE_AND	"&&"
%token OPE_OR	"||"
%token DBL_LESS		"<<"
%token DBL_GRTR		">>"
%token DBL_ARROW	"->>"
%token ARROW	"->"

%type <PlnFunction*>	function_definition
%type <PlnFunction*>	ccall_declaration
%type <PlnFunction*>	syscall_definition
%type <vector<PlnType*>>	single_return
%type <PlnStatement*>	toplv_statement
%type <PlnStatement*>	basic_statement
%type <PlnBlock*>	toplv_block
%type <vector<PlnStatement*>>	toplv_statements
%type <PlnParameter*>	parameter
%type <bool>	move_owner
%type <bool>	take_owner
%type <PlnValue*>	default_value
%type <PlnBlock*>	block
%type <PlnStatement*> while_statement
%type <PlnStatement*> if_statement
%type <PlnStatement*> else_statement
%type <vector<PlnStatement*>>	statements
%type <PlnStatement*>	statement
%type <vector<PlnExpression*>>	expressions
%type <PlnExpression*>	expression
%type <PlnExpression*>	st_expression
%type <PlnExpression*>	assignment
%type <bool>	arrow_ope
%type <PlnExpression*>	term
%type <PlnExpression*>	func_call
%type <vector<PlnExpression*>>	arguments
%type <PlnExpression*>	argument
%type <vector<PlnExpression*>>	dst_vals
%type <PlnExpression*>	dst_val
%type <PlnExpression*>	unary_expression;
%type <vector<PlnValue>>	declarations
%type <PlnValue>	declaration
%type <PlnValue>	subdeclaration
%type <PlnReturnStmt*>	return_stmt
%type <vector<PlnType*>>	type_def
%type <vector<int>>	array_def
%type <vector<int>>	array_sizes
%type <vector<PlnExpression*>>	array_item
%type <vector<PlnExpression*>>	array_indexes

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
	{
		for (auto t: module.types)
			lexer.push_typename(t->name);
		scopes.push_back(PlnScopeItem(&module));
		scopes.push_back(PlnScopeItem(module.toplevel));
	}

	| module function_definition /* empty */

	| module ccall_declaration
	{
		module.functions.push_back($2);
	}

	| module syscall_definition
	{
		module.functions.push_back($2);
	}
	| module toplv_statement
	{
		module.toplevel->statements.push_back($2);
	}
	;

function_definition: KW_FUNC ID '('
		{
			PlnFunction* f = new PlnFunction(FT_PLN, $2);
			f->setParent(&module);
			scopes.push_back(PlnScopeItem(f));
		}
		parameter_def ')' return_def
		{
			PlnFunction* f = scopes.back().inf.function;
			module.functions.push_back(f);
		}
		block
	{
		$$ = scopes.back().inf.function;
		$$->implement = $9;
		scopes.pop_back();
	}
;

return_def: /* empty */
	| ARROW return_types
	| ARROW return_values
	;

return_types: return_type
	| return_types return_type
	;

return_type: type_def
	{
		PlnFunction* f = scopes.back().inf.function;
		string s = "";
		auto v = f->addRetValue(s, &$1, true);
	}
	;

return_values: return_value
	| return_values ',' return_value
	| return_values ',' ID
	{
		PlnFunction* f = scopes.back().inf.function;
		auto v = f->addRetValue($3, NULL, true);
		if (!v) {
			error(@$, PlnMessage::getErr(E_DuplicateVarName, $3));
			YYABORT;
		}
	}
	;

return_value: type_def ID
	{
		PlnFunction* f = scopes.back().inf.function;
		auto v = f->addRetValue($2, &$1, true);
		if (!v) {
			error(@$, PlnMessage::getErr(E_DuplicateVarName, $2));
			YYABORT;
		}
	}
	;

parameter_def: /* empty */
	| parameters
	;

parameters: parameter
	| parameters ',' parameter
	| parameters ',' move_owner ID
	{
		PlnFunction* f = scopes.back().inf.function;
		PlnPassingMethod pm = $3 ? FPM_MOVEOWNER : FPM_COPY;
		auto prm = f->addParam($4, NULL, pm);
		if (!prm) {
			error(@$, PlnMessage::getErr(E_DuplicateVarName, $4));
			YYABORT;
		}
	}
	;

parameter: type_def move_owner ID default_value
	{
		BOOST_ASSERT(scopes.back().type == SC_FUNCTION);
		PlnFunction* f = scopes.back().inf.function;
		PlnPassingMethod pm = $2 ? FPM_MOVEOWNER : FPM_COPY;
		$$ = f->addParam($3, &$1, pm, $4);
		if (!$$) {
			error(@$, PlnMessage::getErr(E_DuplicateVarName, $3));
			YYABORT;
		}
	}
	;

move_owner: /* empty */	{ $$ = false; }
	| DBL_GRTR { $$ = true; }
	;

take_owner: /* empty */	{ $$ = false; }
	| DBL_LESS { $$ = true; }
	;

default_value:	/* empty */	{ $$ = NULL; }
	| '=' ID
	{	
		$$ = NULL; // TODO: get const value.
	}

	| '=' INT
	{
		$$ = new PlnValue($2);
	}

	| '=' UINT
	{
		$$ = new PlnValue($2);
	}

	| '=' STR
	{
		$$ = new PlnValue(module.getReadOnlyData($2));
	}
	;

ccall_declaration: KW_CCALL single_return ID '(' parameter_def ')' ';'
	{
		PlnFunction* f = new PlnFunction(FT_C, $3);
		f->setParent(&module);
		string name = "";
		if ($2.size())
			f->addRetValue(name, &$2, false);
		$$ = f;
	}
	;

syscall_definition: KW_SYSCALL INT ':' single_return ID '(' parameter_def ')' ';'
	{
		PlnFunction* f = new PlnFunction(FT_SYS, $5);
		f->inf.syscall.id = $2;
		f->setParent(&module);
		string name = "";
		if ($4.size())
			f->addRetValue(name, &$4, false);
		$$ = f;
	}
	;

single_return:  { }
	| TYPENAME
	{
		$$.push_back(module.getType($1));
	}
	;

toplv_statement: basic_statement
	{
		$$ = $1;
	}

	| toplv_block
	{
		$$ = new PlnStatement($1, CUR_BLOCK);
	}
	;

basic_statement: st_expression ';'
	{ 
		$$ = new PlnStatement($1, CUR_BLOCK);
	}

	| declarations ';'
	{
		$$ = new PlnStatement(new PlnVarInit($1), CUR_BLOCK);
	}

	| declarations '=' expressions ';'
	{
		int count=0;
		for (auto e: $3)
			count+=e->values.size();

		if ($1.size() > count) {
			error(@$, PlnMessage::getErr(E_NumOfLRVariables));
			YYABORT;
		} if ($1.size() > 1 && $1.size() < count) {
			warn(@$, PlnMessage::getWarn(W_NumOfLRVariables));
		}

		BOOST_ASSERT(scopes.back().type == SC_BLOCK);
		$$ = new PlnStatement(new PlnVarInit($1, &$3), CUR_BLOCK);
	}
	| while_statement
	{
		$$ = $1;
	}
	| if_statement
	{
		$$ = $1;
	}
	;
	
toplv_block: '{'
		{
			PlnBlock *b = new PlnBlock();
			if (scopes.back().type == SC_BLOCK)
				b->setParent(scopes.back().inf.block);
			else
				BOOST_ASSERT(false);
			scopes.push_back(PlnScopeItem(b));
		}
		toplv_statements '}'
	{
		BOOST_ASSERT(scopes.back().type == SC_BLOCK);
		$$ = CUR_BLOCK;
		$$->statements = move($3);
		scopes.pop_back();
	}
	;

toplv_statements:	/* empty */ { }
	| toplv_statements toplv_statement
	{
		if ($2) $1.push_back($2);
		$$ = move($1);
	}
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

while_statement: KW_WHILE st_expression block
	{
		$$ = new PlnWhileStatement($2, $3, CUR_BLOCK);
	}
	;

if_statement: KW_IF st_expression block else_statement
	{
		$$ = new PlnIfStatement($2, $3, $4, CUR_BLOCK);
	}

else_statement: /* empty */
	{
		$$ = NULL;
	}
	
	| KW_ELSE block
	{
		$$ = new PlnStatement($2, CUR_BLOCK);
	}

	| KW_ELSE if_statement
	{
		$$ = $2;
	}
	;

statements:	/* empty */ { }
	| statements statement
	{
		if ($2) $1.push_back($2);
		$$ = move($1);
	}
	;

statement: basic_statement
	{
		$$ = $1;
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

	| expression '*' expression
	{
		$$ = PlnMulOperation::create($1, $3);
	}

	| expression '/' expression
	{
		$$ = PlnDivOperation::create($1, $3);
	}

	| expression '%' expression
	{
		$$ = PlnDivOperation::create_mod($1, $3);
	}

	| expression OPE_EQ expression
	{
		$$ = new PlnCmpOperation($1, $3, CMP_EQ);
	}

	| expression OPE_NE expression
	{
		$$ = new PlnCmpOperation($1, $3, CMP_NE);
	}

	| expression '<' expression
	{
		$$ = new PlnCmpOperation($1, $3, CMP_L);
	}

	| expression '>' expression
	{
		$$ = new PlnCmpOperation($1, $3, CMP_G);
	}

	| expression OPE_LE expression
	{
		$$ = new PlnCmpOperation($1, $3, CMP_LE);
	}

	| expression OPE_GE expression
	{
		$$ = new PlnCmpOperation($1, $3, CMP_GE);
	}

	| expression OPE_AND expression
	{
		$$ = new PlnBoolOperation($1, $3, ET_AND);
	}

	| expression OPE_OR expression
	{
		$$ = new PlnBoolOperation($1, $3, ET_OR);
	}

	| '(' assignment ')'
	{
		$$ = $2;
	}

	| '-' expression %prec UMINUS
	{
		$$ = PlnNegative::create($2);
	}

	| '!' expression
	{
		$$ = PlnBoolOperation::getNot($2);
	}
	
	| term
	{
		$$ = $1;
	}
	;

func_call: ID '(' arguments ')'
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

	| expression move_owner
	{
		$$ = $1; 
		if ($2) {
			if ($$->values[0].type != VL_VAR) {
				error(@$, PlnMessage::getErr(E_CantUseMoveOwnership, "non-variable"));
				YYABORT;
			}
			$$->values[0].asgn_type = ASGN_MOVE;
		}
	}
	;

dst_vals:  unary_expression
	{
		$1->values[0].asgn_type = ASGN_COPY;
		$$.push_back($1);
	}

	| dst_vals ',' dst_val
	{
		$$ = move($1);
		$$.push_back($3);
	}
	;

dst_val: move_owner unary_expression 
	{
		$$ = $2;
		if ($1) {
			auto var = $$->values[0].inf.var;
			if (var->var_type.back()->data_type != DT_OBJECT_REF) {
				error(@$, PlnMessage::getErr(E_CantUseMoveOwnership, var->name));
				YYABORT;
			}
			$$->values[0].asgn_type = ASGN_MOVE;
		} else {
			$$->values[0].asgn_type = ASGN_COPY;
		}
	}

unary_expression: ID
	{
		PlnVariable* var = CUR_BLOCK->getVariable($1);
		if (var) 
			$$ = new PlnExpression(PlnValue(var));
		else {
			error(@$, PlnMessage::getErr(E_UndefinedVariable,$1));
			YYABORT;
		}
	}

	| unary_expression array_item
	{
		$$ = new PlnArrayItem($1, $2);
	}
	;

term: INT
	{
		$$  = new PlnExpression(PlnValue($1));
	}

	| UINT
	{
		$$ = new PlnExpression(PlnValue($1));
	}

	| STR
	{
		$$ = new PlnExpression(PlnValue(module.getReadOnlyData($1)));
	}

	| unary_expression
	{
		$$ = $1;
	}

	| '(' expression ')'
	{
		$$ = $2;
	}
	;

assignment: expressions arrow_ope dst_vals 
	{
		if ($2) {
			auto var = $3.front()->values[0].inf.var;
			if (var->var_type.back()->data_type != DT_OBJECT_REF) {
				error(@$, PlnMessage::getErr(E_CantUseMoveOwnership, var->name));
				YYABORT;
			}
			$3.front()->values[0].asgn_type = ASGN_MOVE;
		}

		int count=0;
		for (auto e: $1)
			count+=e->values.size();

		if ($3.size() > count) {
			error(@$, PlnMessage::getErr(E_NumOfLRVariables));
			YYABORT;
		} if ($3.size() > 1 && $3.size() < count) {
			warn(@$, PlnMessage::getWarn(W_NumOfLRVariables));
		}
		$$ = new PlnAssignment($3, $1);
	}
	;

arrow_ope: ARROW	{ $$ = false; }
	| DBL_ARROW	{ $$ = true; }
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

declaration: type_def ID take_owner
	{
		auto var = CUR_BLOCK->declareVariable($2, $1, true);
		if (!var) {
			error(@$, PlnMessage::getErr(E_DuplicateVarName, $2));
			YYABORT;
		}
		$$ = PlnValue(var);
		if ($3) {
			if (var->var_type.back()->data_type != DT_OBJECT_REF) {
				error(@$, PlnMessage::getErr(E_CantUseMoveOwnership, $2));
				YYABORT;
			}
			$$.asgn_type = ASGN_MOVE;
		} else {
			$$.asgn_type = ASGN_COPY;
		}
	}
	;

subdeclaration: ID take_owner
	{
		vector<PlnType *> null_type;
		auto var = CUR_BLOCK->declareVariable($1, null_type, true);
		if (!var) {
			error(@$, PlnMessage::getErr(E_DuplicateVarName, $1));
			YYABORT;
		}
		$$ = PlnValue(var);
		if ($2) {
			if (var->var_type.back()->data_type != DT_OBJECT_REF) {
				error(@$, PlnMessage::getErr(E_CantUseMoveOwnership, $1));
				YYABORT;
			}
			$$.asgn_type = ASGN_MOVE;
		} else {
			$$.asgn_type = ASGN_COPY;
		}
	}
	;

return_stmt: KW_RETURN
	{
		vector<PlnExpression*> empty;
		$$ = new PlnReturnStmt(empty, CUR_BLOCK);
		auto& rvs = $$->function->return_vals;
		if (rvs.size() > 0 && rvs[0]->name == "") {
			error(@$, PlnMessage::getErr(E_NeedRetValues));
			YYABORT;
		}
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

type_def: TYPENAME	{ $$.push_back(module.getType($1)); }
	| type_def array_def
	{
		$$ = move($1);
		auto t = module.getFixedArrayType($$, $2);
		$$.push_back(t);
	}
	;

array_def: '[' array_sizes ']'
	{
		$$ = move($2);
	}
	;

array_sizes: INT
	{
		$$.push_back($1);
	}

	| array_sizes ',' INT
	{
		$$.push_back($3);
		$$.insert($$.end(),$1.begin(),$1.end()); 
	}
	;

array_item: '[' array_indexes ']'
	{
		$$ = move($2);
	}
	;

array_indexes: expression
	{
		$$.push_back($1);
	}

	| array_indexes ',' expression
	{
		$$.push_back($3);
		$$.insert($$.end(),$1.begin(),$1.end()); 
	}
	;

%%

namespace palan 
{

void PlnParser::error(const location_type& l, const string& m)
{
	cerr << "error: " << l << ": " << m << endl;
}

void warn(const PlnParser::location_type& l, const string& m)
{
	cerr << "warning: " << l << ": " << m << endl;
}

} // namespace

