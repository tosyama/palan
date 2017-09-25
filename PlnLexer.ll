%{
/// Palan Lexer.
///
/// Anarize token of input soruce code.
/// Palan Lexer (PlnLexer.cpp) is created
/// from this definition file by flex.
///
/// @file	PlnLexer.ll
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <algorithm>
#include "PlnParser.hpp"

using std::cout;
using std::endl;
using std::string;

using namespace palan;

#include "PlnLexer.h"

#undef YY_DECL
#define YY_DECL int PlnLexer::yylex(PlnParser::semantic_type& lval, PlnParser::location_type& loc)
#define YY_USER_ACTION	loc.columns(yyleng);

enum {
	INT			= PlnParser::token::INT,
	STR			= PlnParser::token::STR,
	ID			= PlnParser::token::ID,
	TYPENAME	= PlnParser::token::TYPENAME,
	FUNC_ID		= PlnParser::token::FUNC_ID,
	KW_FUNC		= PlnParser::token::KW_FUNC,
	KW_CCALL	= PlnParser::token::KW_CCALL,
	KW_SYSCALL	= PlnParser::token::KW_SYSCALL,
	KW_RETURN	= PlnParser::token::KW_RETURN
};

static string& unescape(string& str);

%}
%option c++
%option yyclass="PlnLexer"
%option noyywrap

DIGIT	[0-9]+
ID	[a-zA-Z_][0-9a-zA-Z_]*
FUNC_ID	{ID}[ \t\n\r]*"("
DELIMITER	"{"|"}"|"("|")"|","|";"|":"|"="|"+"|"-"|"*"|"/"
STRING	"\""(\\.|\\\n|[^\\\"])*"\""
COMMENT1	\/\/[^\n]*\n

%%
%{
	loc.begin.filename = &filename;
	loc.end.filename = &filename;
	loc.step();
%}

{COMMENT1}	{ loc.lines(); }
{DIGIT}	{
		lval.build<int>() = std::stoi(yytext);
		return INT;
	}
ccall	{ return KW_CCALL; }
syscall	{ return KW_SYSCALL; }
func	{ return KW_FUNC; }
return	{ return KW_RETURN; }
{FUNC_ID}	{
		string s = yytext;
		for (auto& c: s)
			if (c==' ' || c=='\t' || c=='\n' || c=='\r' || c=='(') {
				c = '\0';
				break;
			}
		lval.build<string>() = s.c_str();
		return FUNC_ID;
	}
{ID}	{
		string id = yytext;
		if (std::binary_search(typenames.begin(), typenames.end(), id)) {
			lval.build<string>() = move(id);
			return TYPENAME;
		}
		lval.build<string>() = move(id);
		return ID;
	}
{STRING}	{
		string str(yytext+1,yyleng-2); 
		lval.build<string>() = unescape(str);
		return STR;
	}
{DELIMITER}	{ return yytext[0]; }
[ \t]+		{ loc.step(); }
\r\n|\r|\n	{ loc.lines(); }
.	{
		cout << "Lexer: Unrecognized char \"" << yytext[0] << "\"" << endl;
		loc.step();
	}

%%

int yylex(PlnParser::semantic_type* yylval, PlnParser::location_type* location, PlnLexer& lexer)
{
	return lexer.yylex(*yylval, *location);
}

inline int hexc(int c)
{
	if (c>='0' && c<='9') return c-'0';
	if (c>='a' && c<='f') return 10+c-'a';
	if (c>='A' && c<='F') return 10+c-'A';
	return -1;
}

static string& unescape(string& str)
{
	int sz = str.size();
	int d=0;
	for (int s=0; s<sz; ++s,++d) {
		if (str[s] != '\\') {
			str[d] = str[s];
			continue;
		}
		
		++s;
		
		switch(str[s]) {
			case 'a': str[d] = '\a'; break;
			case 'b': str[d] = '\b'; break;
			case 'n': str[d] = '\n'; break;
			case 'r': str[d] = '\r'; break;
			case 't': str[d] = '\t'; break;
			case 'v': str[d] = '\v'; break;
			case '0': str[d] = '\0'; break;

			case 'x': {
				int h1 = hexc(str[s+1]), h2 = hexc(str[s+2]);
				if (h1 >= 0 && h2 >= 0) {
					str[d] = 16*h1 + h2; s+=2;
				} else {
					str[d] = 'x';
				}
			} break;

			default: str[d] = str[s];
		} /* switch */
	}
	str.resize(d);
	return str;
}

void PlnLexer::set_filename(const string& filename)
{
	this->filename = filename;
}

void PlnLexer::push_typename(string name)
{
	// sort for binary_search. note: Trie is better.
	auto ti=typenames.begin();
	for(; ti != typenames.end(); ++ti)
		if (*ti > name)  break;
	typenames.insert(ti, name);
}
