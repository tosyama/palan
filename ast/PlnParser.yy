/// Palan Parser.
///
/// Bulid Palan model tree from lexer input.
/// Palan parser (PlnParser.cpp, PlnParser.hpp and related files)
/// is created from this definition file by bison.
///
/// @file	PlnParser.yy
/// @copyright	2018-2019 YAMAGUCHI Toshinobu 

%skeleton "lalr1.cc"
%require "3.0.2"
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
#define LOC(J, L) J["loc"] = { lexer.cur_fid, (int)L.begin.line, (int)L.begin.column, (int)L.end.line, (int)L.end.column }
#define LOC_BE(J, B, E) J["loc"] = { lexer.cur_fid, (int)B.begin.line, (int)B.begin.column, (int)E.end.line, (int)E.end.column }
}

%code
{
#include "PlnLexer.h"

int yylex(	palan::PlnParser::semantic_type* yylval,
			palan::PlnParser::location_type* location,
			PlnLexer& lexer);
}

%locations
%define api.namespace {palan}
%define parse.error	verbose
%define api.value.type	variant

%token <int64_t>	INT	"integer"
%token <uint64_t>	UINT	"unsigned integer"
%token <double>		FLOAT	"float" %token <string>	STR	"string"
%token <string>	ID	"identifier"
%token <string>	FUNC_ID	"function identifier"
%token <string>	TYPENAME	"type name"
%token KW_FUNC	"func"
%token KW_CCALL	"ccall"
%token KW_SYSCALL	"syscall"
%token KW_RETURN	"return"
%token KW_WHILE	"while"
%token KW_BREAK	"break"
%token KW_CONTINUE	"continue"
%token KW_IF	"if"
%token KW_ELSE	"else"
%token KW_CONST	"const"
%token KW_AUTOTYPE	"var"
%token KW_VARLENARG	"..."
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
%token EQ_ARROW	"=>"

%type <string>	pass_by
%type <bool>	move_owner take_owner arrow_ope
%type <vector<json>>	array_def array_sizes
%type <json>	function_definition	palan_function_definition
%type <json>	ccall_declaration syscall_definition
%type <json>	parameter default_value
%type <json>	return_type	return_value
%type <json>	statement semi_stmt
%type <json>	st_expression block
%type <json>	return_stmt break_stmt continue_stmt
%type <json>	while_statement if_statement else_statement
%type <json>	declaration subdeclaration const_def
%type <json>	expression
%type <json>	assignment func_call chain_call term
%type <json>	argument out_argument literal chain_src
%type <json>	dst_val var_expression
%type <json>	array_item array_size
%type <vector<string>>	const_names
%type <vector<json>>	var_type type_def single_return
%type <vector<json>>	parameter_def out_parameter_def
%type <vector<json>>	parameters out_parameters
%type <vector<json>>	return_def
%type <vector<json>>	return_types return_values
%type <vector<json>>	arguments out_arguments
%type <vector<json>>	declarations
%type <vector<json>>	statements expressions
%type <vector<json>>	dst_vals array_val
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
%left '*' '/' '%' '&'
%left UMINUS '!'

%start module	

%%
module: /* empty */
	{
		vector<string> default_types = {
			"sbyte", "int16", "int32", "int64",
			"byte", "uint16", "uint32", "uint64",
			"flo32", "flo64",
		};
		for (auto& type_name: default_types)
			lexer.push_typename(type_name);
	}
	| module statement
	{
		ast["ast"]["stmts"].push_back(move($2));
	}
	;

palan_function_definition: KW_FUNC FUNC_ID '(' parameter_def ')' return_def block
	{
		json func = {
			{"func-type", "palan"},
			{"name", move($2)},
			{"rets", move($6)},
			{"params", move($4)},
			{"impl", move($7)}
		};
		$$ = move(func);
		LOC_BE($$, @$, @6);
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
		LOC($$, @$);
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
		LOC(ret, @3);
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
		LOC($$, @$);
	}
	;

parameter_def: /* empty */ {}
	| KW_VARLENARG
	{
		json varprm = {
			{ "name", "..." }
		};
		$$.push_back(move(varprm));
	}
	| parameters ',' KW_VARLENARG
	{
		$$ = move($1);
		json varprm = {
			{ "name", "..." }
		};
		$$.push_back(move(varprm));
	}
	| parameters { $$ = move($1); }
	;

parameters: parameter { $$.push_back($1); }
	| parameters ',' parameter
	{
		$$ = move($1);
		$$.push_back($3);
	}
	| parameters ',' pass_by ID default_value
	{
		$$ = move($1);
		json prm = {
			{ "name", $4 },
			{ "pass-by", $3 }
		};
		if (!$5.is_null())
			prm["default-val"] = move($5);

		LOC_BE(prm, @3, @4);

		$$.push_back(move(prm));
	}
	;

parameter: type_def pass_by ID default_value
	{
		json prm = {
			{ "var-type", move($1) },
			{ "name", move($3) },
			{ "pass-by", $2 }
		};
		if (!$4.is_null())
			prm["default-val"] = move($4);
		$$ = move(prm);
		LOC($$, @$);
	}

	| '@'
	{
		json placeholder = {
			{ "name", "@" },
		};
		$$ = move(placeholder);
		LOC($$, @$);
	}
	;

out_parameter_def: /* empty */ { }
	| EQ_ARROW out_parameters
	{
		$$ = move($2);
	}
	;

out_parameters: type_def ID
	{
		json prm = {
			{ "var-type", move($1) },
			{ "name", move($2) },
			{ "pass-by", "write" }
		};
		LOC(prm, @2);
		$$.push_back(move(prm));
	}

	| out_parameters ',' type_def ID
	{
		$$ = move($1);
		json prm = {
			{ "var-type", move($3) },
			{ "name", move($4) },
			{ "pass-by", "write" }
		};
		LOC(prm, @4);
		$$.push_back(move(prm));
	}

	| out_parameters ',' ID
	{
		$$ = move($1);
		json prm = {
			{ "name", move($3) },
			{ "pass-by", "write" }
		};
		LOC(prm, @3);
		$$.push_back(move(prm));
	}
	;

move_owner: /* empty */	{ $$ = false; }
	| DBL_GRTR { $$ = true; }
	;

pass_by: /* empty */ { $$ = "copy"; }
	| DBL_GRTR { $$ = "move"; }
	| '&' { $$ = "ro-ref"; }

take_owner: /* empty */	{ $$ = false; }
	| DBL_LESS { $$ = true; }
	;

default_value:	/* empty */	{  }
	| '=' expression
	{
		$$ = move($2);
	}
	;

ccall_declaration: KW_CCALL FUNC_ID '(' parameter_def out_parameter_def ')' single_return ';'
	{
		vector<json> params = move($4);
		int pind = 0;
		for (json& out_param: $5) {
			while (pind < params.size()) {
				if (params[pind]["name"] == "@")
					break;
				pind++;
			}

			if (pind < params.size()) {
				params[pind] = move(out_param);
				pind++;
			} else {
				params.push_back(move(out_param));
			}
		}

		json ccall = {
			{"func-type", "ccall"},
			{"name", $2},
			{"params", move(params)},
		};
		if ($7.size()) {
			ccall["ret"] = $7;
		}
		$$ = move(ccall);
		LOC($$, @$);
	}
	;

syscall_definition: KW_SYSCALL INT ':' FUNC_ID '(' parameter_def ')' single_return ';'
	{
		json syscall = {
			{"func-type","syscall"},
			{"call-id",$2},
			{"name",$4},
			{"params", $6}	
		};
		if ($8.size()) {
			syscall["ret"] = $8;
		}
		$$ = move(syscall);
		LOC($$, @$);
	}
	;

single_return: /* empty */ { }
	| ARROW type_def
	{
		$$ = move($2);
	}
	;

statement: semi_stmt ';'
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
		LOC($$, @$);
	}

	| while_statement
	{
		$$ = move($1);
	}

	| if_statement
	{
		$$ = move($1);
	}

	| function_definition
	{
		int id = ast["ast"]["funcs"].size();
		$1["id"] = id;
		ast["ast"]["funcs"].push_back(move($1));
		
		json funcdef_stmt = {
			{"stmt-type", "func-def"},
			{"id", id}
		};

		$$ = funcdef_stmt;
	}
	;

semi_stmt: st_expression
	{ 
		json stmt = {
			{"stmt-type", "exp"},
			{"exp", move($1)}
		};
		$$ = move(stmt);
		LOC($$, @$);
	}

	| declarations
	{
		json stmt = {
			{"stmt-type", "var-init"},
			{"vars", move($1)},
		};
		$$ = move(stmt);
		LOC($$, @$);
	}

	| declarations '=' expressions
	{
		json stmt = {
			{"stmt-type", "var-init"},
			{"vars", move($1)},
			{"inits", move($3)},
		};
		$$ = move(stmt);
		LOC($$, @$);
	}

	| subdeclaration '=' expression
	{
		// add empty list for type inference
		$1["var-type"] = vector<json>();
		vector<json> vars = { $1 };
		vector<json> inits = { $3 };
		json stmt = {
			{"stmt-type", "var-init"},
			{"vars", move(vars)},
			{"inits", move(inits)},
		};
		$$ = move(stmt);
		LOC($$, @$);
	}

	| const_def
	{
		$$ = move($1);
	}

	| return_stmt
	{
		$$ = move($1);
	}
	
	| break_stmt 
	{
		$$ = move($1);
	}

	| continue_stmt 
	{
		$$ = move($1);
	}
	;

function_definition: palan_function_definition
	{
		$$ = move($1);
	}

	| ccall_declaration
	{
		$$ = move($1);
	}

	| syscall_definition
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
		LOC($$, @$);
	}

	| '{' statements semi_stmt '}'
	{
		$2.push_back(move($3));
		json block = {
			{"stmts", move($2)}
		};
		$$ = move(block);
		LOC($$, @$);
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
		LOC_BE($$, @$, @2);
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
		LOC($$, @1);
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
		LOC($$, @1);
	}

	| KW_ELSE if_statement
	{
		$$ = move($2);
		$$["loc"][1] = @1.begin.line;
		$$["loc"][2] = @1.begin.column;
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

	| chain_call
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
		LOC($$, @$);
	}

	| expression '-' expression
	{
		json ope { {"exp-type", "-"}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
		LOC($$, @$);
	}

	| expression '*' expression
	{
		json ope { {"exp-type", "*"}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
		LOC($$, @$);
	}

	| expression '/' expression
	{
		json ope { {"exp-type", "/"}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
		LOC($$, @$);
	}

	| expression '%' expression
	{
		json ope { {"exp-type", "%"}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
		LOC($$, @$);
	}

	| expression OPE_EQ expression
	{
		json ope { {"exp-type", "=="}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
		LOC($$, @$);
	}

	| expression OPE_NE expression
	{
		json ope { {"exp-type", "!="}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
		LOC($$, @$);
	}

	| expression '<' expression
	{
		json ope { {"exp-type", "<"}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
		LOC($$, @$);
	}

	| expression '>' expression
	{
		json ope { {"exp-type", ">"}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
		LOC($$, @$);
	}

	| expression OPE_LE expression
	{
		json ope { {"exp-type", "<="}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
		LOC($$, @$);
	}

	| expression OPE_GE expression
	{
		json ope { {"exp-type", ">="}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
		LOC($$, @$);
	}

	| expression OPE_AND expression
	{
		json ope { {"exp-type", "&&"}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
		LOC($$, @$);
	}

	| expression OPE_OR expression
	{
		json ope { {"exp-type", "||"}, {"lval", move($1)}, {"rval", move($3)} };
		$$ = move(ope);
		LOC($$, @$);
	}

	| '(' assignment ')'
	{
		$$ = move($2);
	}

	| '(' chain_call ')'
	{
		$$ = move($2);
	}

	| '-' expression %prec UMINUS
	{
		json ope { {"exp-type", "uminus"}, {"val", move($2)} };
		$$ = move(ope);
		LOC($$, @$);
	}

	| '!' expression
	{
		json ope = { {"exp-type", "not"}, {"val", move($2)} };
		$$ = move(ope);
		LOC($$, @$);
	}
	
	| term
	{
		$$ = move($1);
	}

	;

func_call: FUNC_ID '(' arguments ')'
	{
		vector<json> empty;
		json func_call = {
			{"exp-type", "func-call"},
			{"func-name", $1},
			{"args", $3},
			{"out-args", empty }
		};
		$$ = move(func_call);
		LOC($$, @$);
	}

	| FUNC_ID '(' arguments EQ_ARROW out_arguments ')'
	{
		json func_call = {
			{"exp-type", "func-call"},
			{"func-name", $1},
			{"args", $3},
			{"out-args", $5}
		};
		$$ = move(func_call);
		LOC($$, @$);
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
		// for the case first arg is empty
		if (!$$.size()) {
			json empty_json;
			$$.push_back(empty_json);
		}
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

out_arguments: out_argument
	{
		$$.push_back(move($1));
	}
	| out_arguments ',' out_argument
	{
		$$ = move($1);
		$$.push_back(move($3));
	}
	;

out_argument: expression
	{
		$$["exp"] = move($1);
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
		LOC($$, @$);
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

	| array_val
	{
		if ($1.size() >= 2) {
			bool three_dim = true;
			for (json e: $1) {
				if (e["exp-type"] != "array-val"
						|| e["vals"].size() != 1
						|| e["vals"][0]["exp-type"] != "array-val") {
					three_dim = false;
					break;
				}
			}

			if (three_dim) {
				for (int i=1; i<$1.size(); i++) {
					$1[0]["vals"].push_back($1[i]["vals"][0]);
				}
				$$ = move($1[0]);

			} else {
				json arr_val = {
					{"exp-type", "array-val"},
					{"vals", $1}
				};
				$$ = move(arr_val);
			}
		} else {
			$$ = move($1[0]);
		}
		LOC($$, @$);
	}

	| var_expression
	{
		$1["exp-type"] = "var";
		$$ = move($1);
		LOC($$, @$);
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
		LOC($$, @$);
	}

	| UINT
	{
		json lit_uint = {
			{"exp-type", "lit-uint"},
			{"val", $1}
		};
		$$ = move(lit_uint);
		LOC($$, @$);
	}

	| FLOAT 
	{
		json lit_float = {
			{"exp-type", "lit-float"},
			{"val", $1}
		};
		$$ = move(lit_float);
		LOC($$, @$);
	}

	| STR
	{
		json lit_str = {
			{"exp-type", "lit-str"},
			{"val", $1}
		};
		$$ = move(lit_str);
		LOC($$, @$);
	}
	;

array_val: '[' expressions ']'
	{
		json arr_val = {
			{"exp-type", "array-val"},
			{"vals", $2}
		};
		LOC(arr_val, @$);
		$$.push_back(arr_val);
	}

	| array_val '[' expressions ']'
	{
		$$ = move($1);
		json arr_val = {
			{"exp-type", "array-val"},
			{"vals", $3}
		};
		LOC_BE(arr_val, @2, @4);
		$$.push_back(arr_val);
	}
	;

chain_src: assignment
	{
		$$ = move($1);
	}

	| chain_call
	{
		$$ = move($1);
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
		LOC($$, @$);
	}

	| chain_src arrow_ope dst_vals
	{
		vector<json> exps = { $1 };
		json asgn = {
			{"exp-type", "asgn"},
			{"src-exps", move(exps) },
			{"dst-vals", move($3)}
		};
		if ($2) asgn["dst-vals"][0]["move"] = true;
		$$ = move(asgn);
		LOC($$, @$);
	}
	;

arrow_ope: ARROW	{ $$ = false; }
	| DBL_ARROW	{ $$ = true; }
	;
	
chain_call: expressions arrow_ope func_call
	{
		vector<json> in_args;
		for (auto& e: $1) {
			json arg = {
				{"exp", e}
			};
			in_args.push_back(move(arg));
		}
		
		json c_call = {
			{"exp-type", "chain-call"},
			{"func-name", $3["func-name"]},
			{"in-args", move(in_args)},
			{"args", $3["args"]},
			{"out-args", $3["out-args"]},
		};
		if ($2) c_call["in-args"][0]["move"] = true;
		$$ = move(c_call);
		LOC($$, @$);
	}

	| chain_src arrow_ope func_call
	{
		json arg = { {"exp", move($1)} };
		vector<json> in_args = { arg };

		json c_call = {
			{"exp-type", "chain-call"},
			{"func-name", $3["func-name"]},
			{"in-args", move(in_args)},
			{"args", $3["args"]},
			{"out-args", $3["out-args"]},
		};
		if ($2) c_call["in-args"][0]["move"] = true;
		$$ = move(c_call);
		LOC($$, @$);
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

declaration: var_type ID take_owner
	{
		json dec = {
			{"var-type", move($1)},
			{"name", move($2)}
		};
		if ($3) dec["move"] = true;
		$$ = move(dec);
		LOC($$, @$);
	}
	;

subdeclaration: ID take_owner
	{
		json dec = {
			{"name", move($1)}
		};
		if ($2) dec["move"] = true;
		$$ = move(dec);
		LOC($$, @$);
	}
	;

return_stmt: KW_RETURN
	{
		json ret = { {"stmt-type", "return"} };
		$$ = move(ret);
		LOC($$, @$);
	}
	| KW_RETURN expressions
	{
		json ret = {
			{"stmt-type", "return"},
			{"ret-vals", move($2)}
		};
		$$ = move(ret);
		LOC($$, @1);
	}
	;

break_stmt: KW_BREAK
	{
		json ret = { {"stmt-type", "break"} };
		$$ = move(ret);
		LOC($$, @$);
	}
	;

continue_stmt: KW_CONTINUE
	{
		json ret = { {"stmt-type", "continue"} };
		$$ = move(ret);
		LOC($$, @$);
	}
	;

var_type: KW_AUTOTYPE	/* empty */
	{
	}

	| type_def
	{
		$$ = move($1);
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
			{"sizes", move($2)}
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

array_sizes: array_size
	{
		$$.push_back($1);
	}

	| array_sizes ',' array_size 
	{
		$$ = move($1);
		$$.push_back($3);
	}
	;

array_size: /* empty */
	{
		json inference_size = {
			{"exp-type", "lit-int"},
			{"val", -1}
		};
		$$ = move(inference_size);
		LOC($$, @$);
	}

	| expression
	{
		$$ = move($1);
	}
	
	| '?'
	{
		json unknown_size = {
			{"exp-type", "lit-int"},
			{"val", 0}
		};
		$$ = move(unknown_size);
		LOC($$, @$);
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
		$$.push_back(move($3));
	}
	;

const_def: KW_CONST const_names '=' expressions
	{
		json cnst = {
			{"stmt-type", "const"},
			{"names", move($2)},
			{"values", move($4)}
		};
		$$ = move(cnst);
		LOC($$, @$);
	}
	;
const_names: ID
	{
		$$.push_back($1);
	}
	| const_names ',' ID
	{
		$$ = move($1);
		$$.push_back($3);
	}
	;

%%

namespace palan 
{

void PlnParser::error(const location_type& l, const string& m)
{
	json err = {
		{"msg", m}
	};
	err["loc"] = { lexer.cur_fid, (int)l.begin.line, (int)l.begin.column, (int)l.end.line, (int)l.end.column };
	ast["errs"].push_back(err);
}

} // namespace

