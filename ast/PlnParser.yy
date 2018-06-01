/// Palan Parser.
///
/// Bulid Palan model tree from lexer input.
/// Palan parser (PlnParser.cpp, PlnParser.hpp and related files)
/// is created from this definition file by bison.
///
/// @file	PlnParser.yy
/// @copyright	2018 YAMAGUCHI Toshinobu 

%skeleton "lalr1.cc"
%require "3.0.4"
%defines
%define parser_class_name {PlnParser}
%parse-param	{ PlnLexer &lexer } {json &ast}
%lex-param		{ PlnLexer &lexer }

%code requires
{
#include <vector>
#include <string>
#include <iostream>
#include "../libs/json/single_include/nlohmann/json.hpp"

using std::string;
using std::cerr;
using std::endl;
using std::vector;
using json = nlohmann::json;

class PlnLexer;
}

%code top
{
#include <boost/assert.hpp>
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

%type <string>	single_return
%type <bool>	move_owner take_owner arrow_ope
%type <vector<int64_t>>	array_def array_sizes
%type <json>	function_definition	ccall_declaration syscall_definition
%type <json>	parameter default_value
%type <json>	return_type	return_value
%type <json>	statement
%type <json>	st_expression block return_stmt
%type <json>	while_statement if_statement else_statement
%type <json>	declaration subdeclaration 
%type <json>	expression
%type <json>	assignment func_call term
%type <json>	argument literal
%type <json>	dst_val var_expression
%type <json>	array_item
%type <vector<json>>	type_def
%type <vector<json>>	parameter_def parameters
%type <vector<json>>	return_def
%type <vector<json>>	return_types return_values
%type <vector<json>>	arguments
%type <vector<json>>	declarations
%type <vector<json>>	statements expressions
%type <vector<json>>	dst_vals
%type <vector<json>>	array_indexes

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
		vector<string> default_types = {
			"byte", "int16", "int32", "int64",
			"ubyte", "uint16", "uint32", "uint64",
		};
		for (auto& type_name: default_types)
			lexer.push_typename(type_name);
	}

	| module function_definition
	{
		json proto = {
			{"func-type", $2["func-type"]},
			{"name", $2["name"]},
			{"params", $2["params"]}	
		};

		ast["ast"]["protos"].push_back(move(proto));
		ast["ast"]["funcs"].push_back(move($2));
	}

	| module ccall_declaration
	{
		ast["ast"]["protos"].push_back(move($2));
	}

	| module syscall_definition
	{
		ast["ast"]["protos"].push_back(move($2));
	}
	| module statement
	{
		ast["ast"]["stmts"].push_back(move($2));
	}
	;

function_definition: KW_FUNC ID '(' parameter_def ')' return_def block
	{
		json func = {
			{"func-type", "palan"},
			{"name", move($2)},
			{"rets", move($6)},
			{"params", move($4)},
			{"impl", move($7)}
		};
		$$ = move(func);
	}
	;

return_def: /* empty */ { }
	| ARROW return_types
	{
		$$ = move($2);
	}

	| ARROW return_values
	{
		$$ = move($2);
	}
	;

return_types: return_type
	{
		$$.push_back($1);
	}
	| return_types return_type
	{
		$$ = move($1);
		$$.push_back($2);
	}
	;

return_type: type_def
	{
		json ret = {
			{"var-type", move($1)}
		};
		$$ = move(ret);
	}
	;

return_values: return_value
	{
		$$.push_back($1);
	}
	| return_values ',' return_value
	{
		$$ = move($1);
		$$.push_back($3);
	}
	| return_values ',' ID
	{
		json ret = {
			{ "name", $3 }
		};
		$$ = move($1);
		$$.push_back(move(ret));
	}
	;

return_value: type_def ID
	{
		json ret = {
			{ "var-type", $1 },
			{ "name", $2 }
		};
		$$ = move(ret);
	}
	;

parameter_def: /* empty */ {}
	| parameters { $$ = move($1); }
	;

parameters: parameter { $$.push_back($1); }
	| parameters ',' parameter
	{
		$$ = move($1);
		$$.push_back($3);
	}
	| parameters ',' move_owner ID
	{
		$$ = move($1);
		json prm = {
			{ "name", $4 },
		};
		if ($3)
			prm["move"] = true;
		$$.push_back(move(prm));
	}
	;

parameter: type_def move_owner ID default_value
	{
		json prm = {
			{ "var-type", move($1) },
			{ "name", move($3) },
		};
		if ($2)
			prm["move"] = true;
		if (!$4.is_null())
			prm["default-val"] = move($4);
		$$ = move(prm);
	}
	;

move_owner: /* empty */	{ $$ = false; }
	| DBL_GRTR { $$ = true; }
	;

take_owner: /* empty */	{ $$ = false; }
	| DBL_LESS { $$ = true; }
	;

default_value:	/* empty */	{  }
	| '=' literal
	{
		$$ = move($2);
	}
	;

ccall_declaration: KW_CCALL single_return ID '(' parameter_def ')' ';'
	{
		json ccall = {
			{"func-type", "ccall"},
			{"name", $3},
			{"params", $5},
			{"ret-type", $2},
		};
		$$ = move(ccall);
	}
	;

syscall_definition: KW_SYSCALL INT ':' single_return ID '(' parameter_def ')' ';'
	{
		json syscall = {
			{"func-type","syscall"},
			{"id",$2},
			{"name",$5},
			{"ret-type",$4},
			{"params", $7}	
		};
		$$ = move(syscall);
	}
	;

single_return:  { }
	| TYPENAME
	{
		$$ = move($1);
	}
	;

statement: st_expression ';'
	{ 
		json stmt = {
			{"stmt-type", "exp"},
			{"exp", move($1)}
		};
		$$ = move(stmt);
	}
	| declarations ';'
	{
		json stmt = {
			{"stmt-type", "var-init"},
			{"vars", move($1)},
		};
		$$ = move(stmt);
	}
	| declarations '=' expressions ';'
	{
		json stmt = {
			{"stmt-type", "var-init"},
			{"vars", move($1)},
			{"inits", move($3)},
		};
		$$ = move(stmt);
	}
	| return_stmt ';'
	{
		$$ = move($1);
	}
	| block
	{
		json stmt = {
			{"stmt-type", "block"},
			{"block",  move($1) }
		};
		$$ = move(stmt);
	}
	| while_statement
	{
		$$ = move($1);
	}
	| if_statement
	{
		$$ = move($1);
	}
	;
	
block: '{' statements '}'
	{
		json block = {
			{"stmts", move($2)}
		};
		$$ = move(block);
	}
	;

while_statement: KW_WHILE st_expression block
	{
		json whl = {
			{"stmt-type", "while"},
			{"cond", move($2)},
			{"block", $3}
		};
		$$ = move(whl);
	}
	;

if_statement: KW_IF st_expression block else_statement
	{
		json ifs = {
			{"stmt-type", "if"},
			{"cond", move($2)},
			{"block", $3}
		};
		if (!$4.is_null())
			ifs["else"] = move($4);
		$$ = move(ifs);
	}

else_statement: /* empty */
	{
	}
	
	| KW_ELSE block
	{
		json stmt = {
			{"stmt-type", "block"},
			{"block",  move($2) }
		};
		$$ = move(stmt);
	}

	| KW_ELSE if_statement
	{
		$$ = move($2);
	}
	;

statements:	/* empty */ { }
	| statements statement
	{
		$1.push_back(move($2));
		$$ = move($1);
	}
	;

st_expression: expression
	{
		$$ = move($1);
	}
	| assignment
	{
		$$ = move($1);
	}
	;

expressions: expression
	{
		$$.push_back(move($1));
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
		$$ = move($1);
	}

	| expression '+' expression
	{
		json ope { {"exp-type", "+"}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
	}

	| expression '-' expression
	{
		json ope { {"exp-type", "-"}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
	}

	| expression '*' expression
	{
		json ope { {"exp-type", "*"}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
	}

	| expression '/' expression
	{
		json ope { {"exp-type", "/"}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
	}

	| expression '%' expression
	{
		json ope { {"exp-type", "%"}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
	}

	| expression OPE_EQ expression
	{
		json ope { {"exp-type", "=="}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
	}

	| expression OPE_NE expression
	{
		json ope { {"exp-type", "!="}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
	}

	| expression '<' expression
	{
		json ope { {"exp-type", "<"}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
	}

	| expression '>' expression
	{
		json ope { {"exp-type", ">"}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
	}

	| expression OPE_LE expression
	{
		json ope { {"exp-type", "<="}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
	}

	| expression OPE_GE expression
	{
		json ope { {"exp-type", ">="}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
	}

	| expression OPE_AND expression
	{
		json ope { {"exp-type", "&&"}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
	}

	| expression OPE_OR expression
	{
		json ope { {"exp-type", "||"}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
	}

	| '(' assignment ')'
	{
		$$ = move($2);
	}

	| '-' expression %prec UMINUS
	{
		json ope { {"exp-type", "uminus"}, {"val", move($2)} };
		$$ = move(ope);
	}

	| '!' expression
	{
		json ope = { {"exp-type", "not"}, {"val", move($2)} };
		$$ = move(ope);
	}
	
	| term
	{
		$$ = move($1);
	}
	;

func_call: ID '(' arguments ')'
	{
		json func_call = {
			{"exp-type", "func-call"},
			{"func-name", $1},
			{"args", $3}
		};
		$$ = move(func_call);
	}
	;

arguments: argument
	{
		if (!$1.is_null())
			$$.push_back(move($1));
	}

	| arguments ',' argument
	{
		$$ = move($1);
		$$.push_back(move($3));
	}
	;

argument: /* empty */
	{
	}

	| expression move_owner
	{
		$$["exp"] = move($1);
		if ($2) $$["move"] = true;
	}
	;

dst_vals:  var_expression
	{
		$$.push_back(move($1));
	}

	| dst_vals ',' dst_val
	{
		$$ = move($1);
		$$.push_back($3);
	}
	;

dst_val: move_owner var_expression 
	{
		$$ = move($2);
		if ($1) $$["move"] = true;

	}

var_expression: ID
	{
		json uexp = {
			{ "base-var", $1 }
		};
		$$ = move(uexp);
	}

	| var_expression array_item
	{
		$$ = move($1);
		$$["opes"].push_back(move($2));
	}
	;

term: literal
	{
		$$ = move($1);
	}
	| var_expression
	{
		$1["exp-type"] = "var";
		$$ = move($1);
	}
	| '(' expression ')'
	{
		$$ = move($2);
	}
	;

literal: INT
	{
		json lit_int = {
			{"exp-type", "lit-int"},
			{"val", $1}
		};
		$$ = move(lit_int);
	}

	| UINT
	{
		json lit_uint = {
			{"exp-type", "lit-uint"},
			{"val", $1}
		};
		$$ = move(lit_uint);
	}

	| STR
	{
		json lit_str = {
			{"exp-type", "lit-str"},
			{"val", $1}
		};
		$$ = move(lit_str);
	}
	;

assignment: expressions arrow_ope dst_vals 
	{
		json asgn = {
			{"exp-type", "asgn"},
			{"src-exps", move($1)},
			{"dst-vals", move($3)}
		};
		if ($2) asgn["dst-vals"][0]["move"] = true;
		$$ = move(asgn);
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
		json dec = {
			{"var-type", move($1)},
			{"name", move($2)}
		};
		if ($3) dec["move"] = true;
		$$ = move(dec);
	}
	;

subdeclaration: ID take_owner
	{
		json dec = {
			{"name", move($1)}
		};
		if ($2) dec["move"] = true;
		$$ = move(dec);
	}
	;

return_stmt: KW_RETURN
	{
		json ret = { {"stmt-type", "return"} };
		$$ = move(ret);
	}
	| KW_RETURN expressions
	{
		json ret = {
			{"stmt-type", "return"},
			{"ret-vals", move($2)}
		};
		$$ = move(ret);
	}
	;

type_def: TYPENAME
	{
		json ptype = {
			{"name", $1}
		};
		$$.push_back(move(ptype));
	}

	| type_def array_def
	{
		json atype = {
			{"name", "[]"},
			{"sizes", $2}
		};
		$$ = move($1);
		$$.push_back(move(atype));
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
		$$ = move($1);
		$$.push_back($3);
	}
	;

array_item: '[' array_indexes ']'
	{
		json arri = {
			{"ope-type", "index"},
			{"indexes", move($2)}
		};
		$$ = move(arri);
	}
	;

array_indexes: expression
	{
		$$.push_back(move($1));
	}

	| array_indexes ',' expression
	{
		$$ = move($1);
		$$.push_back(move($1));
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

