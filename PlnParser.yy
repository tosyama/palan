%skeleton "lalr1.cc"
%require "3.0.4"
%defines
%define parser_class_name {PlnParser}

%code requires
{
#include <string>
#include <iostream>
using std::string;
using std::cout;
using std::endl;
}

%code
{
int yylex(palan::PlnParser::semantic_type* yylval);
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
	| func_call
	;
%%

using namespace palan;

void PlnParser::error(const string& m)
{
	cout << "error: " << m << endl;
}

typedef struct {
	int ret;
	int i;
	string s;
} lexdata;

enum {
	INT = PlnParser::token::INT,
	STR = PlnParser::token::STR,
	ID = PlnParser::token::ID
};

lexdata data[] = {
	{ ID, 0, "void" }, { ID, 0, "main" }, { '(', 0, "" }, { ')', 0, "" },
	{ '{', 0, "" },
		{ ID, 0, "sys_write" }, { '(', 0, "" },
			{ INT, 1, "" }, { ',', 0, ""},
			{ INT, 1, "" }, { ',', 0, ""},
			{ INT, 14, "" }, 
		{ ')', 0, "" }, { ';', 0, "" },
		{ ID, 0, "sys_exit" }, { '(', 0, "" },
			{ INT, 0, "" }, 
		{ ')', 0, "" }, { ';', 0, "" },
	{ '}', 0, "" }
};

int cur = 0;

int yylex(PlnParser::semantic_type* yylval)
{
	int num = sizeof(data)/sizeof(lexdata);
	if (cur >= num) return 0;
	int ret = data[cur].ret;
	switch (ret){
		case INT: yylval->build<int>() = data[cur].i; break;
		case STR: yylval->build<string>() = data[cur].s; break;
		case ID: yylval->build<string>() = data[cur].s; break;
	}
	cur++;
	
	return ret;
}

int main()
{
	PlnParser parser;
	parser.parse();

	return 0;
}
