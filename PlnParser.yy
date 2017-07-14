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

int yylex();
}

%define api.value.type	variant
%token	END	0
%token	EOL	
%token <int>	INT
%token <string>	STR

%start	statements
%%

statement:	INT ':' STR 
	
statements: /* empty */
	| statements statement EOL
	| statements statement END
%%

int yylex()
{
	retrun END;
}

int main()
{
	cout << "test.\n";
	return 0;
}
