%{
#include <iostream>
#include <sstream>
#include "PlnParser.hpp"

using std::cout;
using std::endl;
using std::stringstream;

using namespace palan;

enum {
	INT = PlnParser::token::INT,
	STR = PlnParser::token::STR,
	ID = PlnParser::token::ID
};

%}
%option c++
%option prefix="Pln"
%option noyywrap

DIGIT	[0-9]+
ID	[a-zA-Z_][0-9a-zA-Z_]*
DELIMITER	"{"|"}"|"("|")"|","|";"
STRING	\"(\\.|\\\n|[^\\\"])*\"

%%
{DIGIT}	{ cout << "INT:" << std::stoi(yytext) << endl; return INT; }
{ID}	{ cout << "ID:" << yytext << endl; return ID; }
{STRING}	{ cout << "STR:" << yytext << endl; return STR; }
{DELIMITER}	{ cout << "Deli:" << yytext << endl; return yytext[0]; }
[ \n\t\n\r]+
.		{ cout << "Lexer: Unrecognized: \"" << yytext[0] << "\"" << endl;}

%%

int yylex(PlnParser::semantic_type* yylval, PlnFlexLexer& lexer)
{
	int ret = lexer.yylex();
	switch (ret){
		case INT: yylval->build<int>() = 99; break;
		case STR: yylval->build<string>() = "psr: str"; break;
		case ID: yylval->build<string>() = "psr: id"; break;
	}

	return ret;
}

int main()
{
	PlnFlexLexer lexer;
	stringstream str(
		"void main()\n"
		"{\n"
		"	sys_write(1,\"Hello World!\\n\", 14);\n"
		"	sys_exit(0);\n"
		"}"
	);
	lexer.switch_streams(&str,&cout);

	PlnParser parser(lexer);
	parser.parse();

	return 0;
}
