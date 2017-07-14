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
int yylex(yy::PlnParser::semantic_type* yylval);
}
%define parse.error	verbose
%define api.value.type	variant
%token <int>	INT
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

return_values: STR	{ cout << "ret values:" << $1 << endl; };
	;
func_name: STR		{ $$ = $1; }
	;
parameters: /* empty */
	| STR
	;
block: '{' statements '}'
	;
statements:	/* empty */
	| statements statement
	;
statement: func_call ';'
	;
func_call: func_name '(' ')'	{ cout << "call: " << $1 << endl; }
	;

%%

using namespace yy;

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
	STR = PlnParser::token::STR
};

lexdata data[] = {
	{ STR, 0, "void" }, { STR, 0, "main" }, { '(', 0, "" }, { ')', 0, "" },
	{ '{', 0, "" },
		{ STR, 0, "sys_write" }, { '(', 0, "" }, { ')', 0, "" }, { ';', 0, "" },
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
