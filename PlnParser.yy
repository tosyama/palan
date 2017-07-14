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

%define api.value.type	variant
%token <int>	INT
%token <string>	STR

%start	statements
%%
statements: statements statement
	| statement
	;
statement:	INT ':' STR '\n' { cout << "statement:" << $1 << "," << $3 << endl; }
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

lexdata data[4] = {
	{ INT, 9, "" },
	{ ':', 0, "" },
	{ STR, 0, "test" },
	{ '\n', 0, "" }
};

int cur = 0;

int yylex(PlnParser::semantic_type* yylval)
{
	if (cur >= 4) return 0;
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
