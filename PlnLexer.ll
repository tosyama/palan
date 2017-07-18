%{
#include <sstream>
#include "PlnParser.hpp"

using std::cout;
using std::endl;
using std::string;
using std::stringstream;

using namespace palan;

enum {
	INT = PlnParser::token::INT,
	STR = PlnParser::token::STR,
	ID = PlnParser::token::ID
};

#include "PlnLexer.h"
inline void unescape(string& str)
{
	int sz = str.size();
	int s = 0; int d= 0;
	while (s<sz) {
		if (str[s]=='\\') {
			++s;	
			switch(str[s]) {
				case 'n': str[d] = '\n'; break;
				case 't': str[d] = '\t'; break;
				default: str[d] = str[s];
			}
		} else {
			str[d] = str[s];
		}
		++d; ++s;
	}
	str.resize(d);
}

%}
%option c++
%option yyclass="PlnLexer"
%option noyywrap

DIGIT	[0-9]+
ID	[a-zA-Z_][0-9a-zA-Z_]*
DELIMITER	"{"|"}"|"("|")"|","|";"
STRING	\"(\\.|\\\n|[^\\\"])*\"

%%
{DIGIT}	{
			intValue = std::stoi(yytext);
			return INT;
		}
{ID}	{
			strValue = yytext;
			return ID;
		}
{STRING}	{
			strValue=yytext+1; 
			strValue.resize(strValue.size()-1);
			unescape(strValue);
			return STR;
		}
{DELIMITER}	{ return yytext[0]; }
[ \n\t\n\r]+
.		{ cout << "Lexer: Unrecognized char \"" << yytext[0] << "\"" << endl;}

%%

int yylex(PlnParser::semantic_type* yylval, PlnLexer& lexer)
{
	int ret = lexer.yylex();
	switch (ret) {
		case INT: yylval->build<int>() = lexer.intValue; break;
		case STR: 
		case ID: yylval->build<string>() = lexer.strValue; break;
	}

	return ret;
}

int main()
{
	PlnLexer lexer;
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
