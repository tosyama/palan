/// Palan Parser.
///
/// Bulid Palan model tree from lexer input.
/// Palan parser (PlnParser.cpp, PlnParser.hpp and related files)
/// is created from this definition file by bison.
///
/// @file	PlnParser.yy
/// @copyright	2018-2021 YAMAGUCHI Toshinobu 

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
#include "../../libs/json/single_include/nlohmann/json.hpp"

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
%token <double>		FLOAT	"float"
%token <string>	STR	"string"
%token <string>	ID	"identifier"
%token <string>	FUNC_ID	"function identifier"
%token KW_TYPE	"type"
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
%token KW_EXTERN	"extern"
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
%token DBL_EQ_ARROW	"=>>"
%token AT_EXCL	"@!"
%token DBL_PLUS	"++"
%token DBL_MINUS "--"

%type <string>	strs
%type <string>	arg_opt
%type <bool>	move_owner take_owner arrow_ope output_arrow
%type <json>	function_definition	palan_function_definition
%type <json>	ccall_declaration syscall_definition
%type <json>	parameter default_value
%type <json>	return_type	return_value single_return
%type <json>	statement semi_stmt
%type <json>	st_expression block
%type <json>	return_stmt break_stmt continue_stmt
%type <json>	while_statement if_statement else_statement
%type <json>	type_def declaration subdeclaration const_def
%type <json>	extern_var_def
%type <json>	expression
%type <json>	assignment func_call chain_call term
%type <json>	argument literal
%type <json>	dst_val var_expression
%type <json>	array_item
%type <json>	array_val /* internal data {name,items,loc} */
%type <json>	increment
%type <vector<string>>	const_names
%type <vector<json>>	/*ids*/ array_vals /* internal data {name,items,loc} array */
%type <vector<json>>	type_prefix_ref type_prefix_arr
/*%type <vector<json>>	var_exp_ids var_exp_affixes */
%type <vector<json>>	array_items
%type <vector<json>>	var_type type
%type <vector<json>>	parameter_def out_parameter_def
%type <vector<json>>	parameters out_parameters
%type <vector<json>>	return_def
%type <vector<json>>	return_types return_values
%type <vector<json>>	arguments out_arguments
%type <vector<json>>	declarations
%type <vector<json>>	statements expressions
%type <vector<json>>	dst_vals chain_src
%type <vector<json>>	struct_def

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
%right UMINUS '!' '@' '#'
%left '.'

%start module	

%%
module: /* empty */
	{
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
	| return_types ',' return_type
	{
		$$ = move($1);
		$$.push_back($3);
	}
	;

return_type: type
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

return_value: type ID
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
			{ "name", "..." },
			{ "io", "in" },
			{ "moveto", "none" }
		};
		LOC(varprm, @$);
		$$.push_back(move(varprm));
	}
	| parameters ',' KW_VARLENARG
	{
		$$ = move($1);
		json varprm = {
			{ "name", "..." },
			{ "io", "in" },
			{ "moveto", "none" }
		};
		LOC(varprm, @3);
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
	| parameters ',' ID default_value
	{
		$$ = move($1);
		json prm = {
			{ "name", $3 },
			{ "io", "in" },
			{ "moveto", "none" }
		};
		if (!$4.is_null())
			prm["default-val"] = move($4);

		LOC_BE(prm, @3, @4);

		$$.push_back(move(prm));
	}
	| parameters ',' DBL_GRTR ID default_value
	{
		$$ = move($1);
		json prm = {
			{ "name", $4 },
			{ "io", "in" },
			{ "moveto", "callee" }
		};
		if (!$5.is_null())
			prm["default-val"] = move($5);

		LOC_BE(prm, @3, @4);

		$$.push_back(move(prm));
	}
	;

parameter: type move_owner ID default_value
	{
		json prm = {
			{ "var-type", move($1) },
			{ "name", move($3) },
			{ "io", "in" },
			{ "moveto", $2 ? "callee" : "none" }
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
	| EQ_ARROW KW_VARLENARG 
	{
		json prm = {
			{ "name", "..." },
			{ "io", "out" },
			{ "moveto", "none" }
		};
		LOC(prm, @2);
		$$.push_back(move(prm));
	}
	| EQ_ARROW out_parameters ',' KW_VARLENARG
	{
		$$ = move($2);
		json prm = {
			{ "name", "..." },
			{ "io", "out" },
			{ "moveto", "none" }
		};
		LOC(prm, @4);
		$$.push_back(move(prm));
	}
	;

out_parameters: type ID move_owner
	{
		json prm = {
			{ "var-type", move($1) },
			{ "name", move($2) },
			{ "io", "out" },
			{ "moveto", $3 ? "caller" : "none" }
		};
		LOC(prm, @2);
		$$.push_back(move(prm));
	}
	| out_parameters ',' type ID move_owner

	{
		$$ = move($1);
		json prm = {
			{ "var-type", move($3) },
			{ "name", move($4) },
			{ "io", "out" },
			{ "moveto", $5 ? "caller" : "none" }
		};
		LOC(prm, @4);
		$$.push_back(move(prm));
	}
	| out_parameters ',' ID move_owner
	{
		$$ = move($1);
		json prm = {
			{ "name", move($3) },
			{ "io", "out" },
			{ "moveto", $4 ? "caller" : "none" }
		};
		LOC(prm, @3);
		$$.push_back(move(prm));
	}
	;

move_owner: /* empty */	{ $$ = false; }
	| DBL_GRTR { $$ = true; }
	;

default_value:	/* empty */	{  }
	| '=' expression
	{
		$$ = move($2);
	}
	;

ccall_declaration: KW_CCALL FUNC_ID '(' parameter_def out_parameter_def ')' single_return at_lib';'
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
		if ($7.is_object()) {
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
		if ($8.is_object()) {
			syscall["ret"] = $8;
		}
		$$ = move(syscall);
		LOC($$, @$);
	}
	;

single_return: /* empty */ { }
	| ARROW type
	{
		json retval = {
			{"var-type", move($2)}
		};
		$$ = move(retval);
		LOC($$, @$);
	}
	;

at_lib: /* empty */
	| ':' ID
	{
		json lib = {{ "name", $2 }};
		ast["ast"]["libs"].push_back(lib);
	}
	| ':' STR 
	{
		json lib = {{ "name", $2 }};
		ast["ast"]["libs"].push_back(lib);
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
	| type_def 
	{
		$$ = move($1);
	}
	| extern_var_def
	{
		$$ = move($1);
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
 	| ID take_owner '=' expression
 	{
 		json var = {
 			{"name", move($1)},
 			{"var-type", vector<json>()}
 		};
		var["get-owner"] = $2;
 		LOC(var, @1);
 		vector<json> vars = { var };
 		vector<json> inits = { $4 };
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
	| increment 
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
	| FUNC_ID '(' arguments output_arrow out_arguments ')'
	{
		if ($4) {
			$5.front()["option"] = "get-owner";
		}
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

output_arrow: EQ_ARROW { $$ = false; }
	| DBL_EQ_ARROW { $$ = true; }
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
	| expression arg_opt
	{
		$$["exp"] = move($1);
		$$["option"] = $2;
	}
	;

arg_opt: /* empty */
	{
		$$ = "none";
	}
	| DBL_GRTR
	{
		$$ = "move-owner";
	}
	| '!'
	{
		$$ = "writable-ref";
	}
	;

out_arguments: expression
	{
		json out_arg;
		out_arg["exp"] = move($1);
		out_arg["option"] = "none";
		$$.push_back(move(out_arg));
	}
	| out_arguments ',' expression
	{
		$$ = move($1);
		json out_arg;
		out_arg["exp"] = move($3);
		out_arg["option"] = "none";
		$$.push_back(move(out_arg));
	}
	| out_arguments ',' DBL_GRTR expression
	{
		$$ = move($1);
		json out_arg;
		out_arg["exp"] = move($4);
		out_arg["option"] = "get-owner";
		$$.push_back(move(out_arg));
	}
	;

dst_vals: dst_val 
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
		$$["exp"] = move($2);
		$$["get-owner"] = $1;
	}
	;

var_expression: ID
	{
		json vexp = {
			{ "exp-type", "var" },
			{ "var-name", $1 }
		};
		$$ = move(vexp);
		LOC($$, @1);
	}
	| var_expression array_val
	{
		json vexp = {
			{ "exp-type", "index" },
			{ "base-exp", $1 },
			{ "indexes", $2["items"] }
		};
		$$ = move(vexp);
		LOC($$, @2);
	}

	| var_expression '.' ID
	{
		json vexp = {
			{ "exp-type", "member" },
			{ "base-exp", $1 },
			{ "member-name", $3 }
		};
		$$ = move(vexp);
		LOC($$, @2);
	}
	;

term: literal
	{
		$$ = move($1);
	}
	| array_vals
	{
		if ($1.size() >= 2) {
			bool three_dim = true;
			for (json& e: $1) {
				if (e["items"].size() != 1 || e["items"][0]["exp-type"] != "array-val") {
					three_dim = false;
					break;
				}
			}

			vector<json> vals;
			if (three_dim) {
				for (json& val: $1) {
					json jval = {
						{"exp-type", "array-val"},
						{"vals", move(val["items"][0]["vals"])},
						{"loc", move(val["items"][0]["loc"])}
					};
					vals.push_back(move(jval));
				}
			} else {
				for (json& val: $1) {
					json jval = {
						{"exp-type", "array-val"},
						{"vals", move(val["items"])},
						{"loc", move(val["loc"])}
					};
					vals.push_back(move(jval));
				}
			}
			json arr_val = {
				{"exp-type", "array-val"},
				{"vals", move(vals)}
			};
			$$ = move(arr_val);
		} else {
			json arr_val = {
				{"exp-type", "array-val"},
				{"vals", $1[0]["items"]}
			};
			$$ = move(arr_val);
		}
		LOC($$, @$);
	}
	| var_expression
	{
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
	| strs
	{
		json lit_str = {
			{"exp-type", "lit-str"},
			{"val", $1}
		};
		$$ = move(lit_str);
		LOC($$, @$);
	}
	;

strs: STR
	{
		$$ = move($1);
	}
	| strs STR
	{
		$$ = $1 + $2;
	}
	;

array_vals: array_val
	{
		$$.push_back(move($1));
	}
	| array_vals array_val
	{
		$$ = move($1);
		$$.push_back(move($2));
	}
	;

array_val: '[' array_items ']'
	{
		json arr_val = {
			{"name", "[]"},
			{"items", $2}
		};
		LOC(arr_val, @$);
		$$ = move(arr_val);
	}
	;

chain_src: expressions
	{
		$$ = move($1);
	}
	| assignment
	{
		vector<json> exps = { move($1) };
		$$ = move(exps);
	}
	| chain_call
	{
		vector<json> exps = { move($1) };
		$$ = move(exps);
	}
	;

assignment: chain_src arrow_ope dst_vals 
	{
		json asgn = {
			{"exp-type", "asgn"},
			{"src-exps", move($1)},
			{"dst-vals", move($3)}
		};
		asgn["dst-vals"][0]["get-owner"] = $2;
		$$ = move(asgn);
		LOC($$, @$);
	}
	;
	
chain_call: chain_src arrow_ope func_call
	{
		vector<json> in_args;
		for (auto& e: $1) {
			json arg = {
				{"exp", e},
				{"option", "none"}
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
		if ($2) {
			c_call["in-args"][0]["option"] = "move-owner";
		}
		$$ = move(c_call);
		LOC($$, @$);
	}
	;

arrow_ope: ARROW	{ $$ = false; }
	| DBL_ARROW	{ $$ = true; }
	;

type_def: KW_TYPE ID
	{
		json stmt = {
			{"stmt-type", "type-def"},
			{"type", "obj-ref"},
			{"name", $2},
		};
		$$ = move(stmt);
		LOC($$, @$);
	}
	| KW_TYPE ID '{' struct_def '}'
	{
		json stmt = {
			{"stmt-type", "type-def"},
			{"type", "struct"},
			{"name", $2},
			{"members", $4},
		};
		$$ = move(stmt);
		LOC($$, @$);
	}
	| KW_TYPE ID '=' type 
	{
		json stmt = {
			{"stmt-type", "type-def"},
			{"type", "alias"},
			{"name", $2},
			{"var-type", $4},
		};
		$$ = move(stmt);
		LOC($$, @$);
	}
	;

extern_var_def: KW_EXTERN type ID
	{
		vector<string> names = { $3 };
		json stmt = {
			{"stmt-type", "extern-var"},
			{"var-type", $2},
			{"names", names}
		};
		$$ = move(stmt);
		LOC($$, @$);
	}
	| extern_var_def ',' ID
	{
		$$ = move($1);
		$$["names"].push_back($3);
		LOC($$, @$);
	}
	;

struct_def: type ID
	{
		json member = {
			{"type", $1},
			{"name", $2},
		};
		$$.push_back(member);
	}
	| struct_def ';' type ID
	{
		$$ = move($1);
		json member = {
			{"type", $3},
			{"name", $4},
		};
		$$.push_back(member);
	}
	| struct_def ';'
	{
		$$ = move($1);
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
		dec["get-owner"] = $3;
		$$ = move(dec);
		LOC($$, @$);
	}
	;

subdeclaration: ID take_owner
	{
		json dec = {
			{"name", move($1)}
		};
		dec["get-owner"] = $2;
		$$ = move(dec);
		LOC($$, @$);
	}
	;

take_owner: /* empty */ { $$ = false; }
	| DBL_LESS { $$ = true; }
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

var_type: KW_AUTOTYPE	/* empty */
	{
	}
	| type
	{
		$$ = move($1);
	}
	;

type: ID
	{
		json t = {
			{ "name", $1 },
			{ "mode", "---" }
		};
		LOC(t, @1);
		$$.push_back(move(t));
	}
	| type_prefix_ref ID 
	{
		json& t = $1.back();
		t["name"] = $2;
		LOC(t, @2);
		$$ = move($1);
	}
	| type_prefix_arr ID
	{
		json t = {
			{ "name", $2 },
			{ "mode", "---" }
		};
		LOC(t, @2);
		$1.push_back(move(t));
		$$ = move($1);
	}
	;

type_prefix_ref: '@'
	{
		json t = { { "mode", "rir" } };
		$$.push_back(move(t));
	}
	| AT_EXCL
	{
		json t = { { "mode", "wcr" } };
		$$.push_back(move(t));
	}
	| '#'
	{
		json t = { { "mode", "wis" } };
		$$.push_back(move(t));
	}
	| type_prefix_arr '@'
	{
		json t = { { "mode", "rir" } };
		$1.push_back(move(t));
		$$ = move($1);
	}
	| type_prefix_arr AT_EXCL
	{
		json t = { { "mode", "wcr" } };
		$1.push_back(move(t));
		$$ = move($1);
	}
	;

type_prefix_arr: array_vals
	{
		for (json &a: $1) {
			json t = {
				{ "name", "[]" },
				{ "sizes", move(a["items"]) },
				{ "mode", "---" },
				{ "loc", move(a["loc"]) }
			};
			$$.push_back(move(t));
		}
	}
	| type_prefix_ref array_vals
	{
		string last_mode = $1.back()["mode"];
		$1.pop_back();
		int ind = $1.size();

		for (json &a: $2) {
			json t = {
				{ "name", "[]" },
				{ "sizes", move(a["items"]) },
				{ "mode", "---" },
				{ "loc", move(a["loc"]) }
			};
			$1.push_back(move(t));
		}
		$1[ind]["mode"] = last_mode;
		$$ = move($1);
	}
	;

array_items: array_item
	{
		$$.push_back($1);
	}
	| array_items ',' array_item
	{
		$$ = move($1);
		$$.push_back($3);
	}
	;

array_item: /* empty */
	{
		json inference_size = {
			{"exp-type", "token"},
			{"info", ""}
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
		json unknown = {
			{"exp-type", "token"},
			{"info", "?"}
		};
		$$ = move(unknown);
		LOC($$, @$);
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

increment: var_expression DBL_PLUS
	{
		json one = {
			{"exp-type", "lit-int"},
			{"val", 1}
		};
		LOC(one, @2);

		json inc = {
			{"stmt-type", "ope-asgn"},
			{"dst-val", $1},
			{"ope", "+" },
			{"rval", one}
		};

		$$ = move(inc);
		LOC($$, @$);
	}
	| var_expression DBL_MINUS
	{
		json one = {
			{"exp-type", "lit-int"},
			{"val", -1}
		};
		LOC(one, @2);

		json inc = {
			{"stmt-type", "ope-asgn"},
			{"dst-val", $1},
			{"ope", "+" },
			{"rval", one}
		};

		$$ = move(inc);
		LOC($$, @$);
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

