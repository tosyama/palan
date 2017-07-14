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
%token	EOL	
%token <int>	INT
%token <string>	STR

%start	statements
%%
statements: statements statement
	| statement
	;
statement:	INT ':' STR EOL { cout << "statement." << endl; }
	;
%%

using namespace yy;

void PlnParser::error(const string& m)
{
	cout << "error: " << m << endl;
}

int yylex(PlnParser::semantic_type* yylval)
{
	cout << "lex" << endl;
	return 0;
}

int main()
{
	PlnParser parser;
	parser.parse();

	return 0;
}
