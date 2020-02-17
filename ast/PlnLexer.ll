%{
/// Palan Lexer.
///
/// Anarize token of input soruce code.
/// Palan Lexer (PlnLexer.cpp) is created
/// from this definition file by flex.
///
/// @file	PlnLexer.ll
/// @copyright	2017-2020 YAMAGUCHI Toshinobu 

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
	UINT		= PlnParser::token::UINT,
	FLOAT		= PlnParser::token::FLOAT,
	STR			= PlnParser::token::STR,
	ID			= PlnParser::token::ID,
	FUNC_ID		= PlnParser::token::FUNC_ID,
	KW_TYPE		= PlnParser::token::KW_TYPE,
	KW_FUNC		= PlnParser::token::KW_FUNC,
	KW_CCALL	= PlnParser::token::KW_CCALL,
	KW_SYSCALL	= PlnParser::token::KW_SYSCALL,
	KW_RETURN	= PlnParser::token::KW_RETURN,
	KW_WHILE	= PlnParser::token::KW_WHILE,
	KW_BREAK	= PlnParser::token::KW_BREAK,
	KW_CONTINUE	= PlnParser::token::KW_CONTINUE,
	KW_IF		= PlnParser::token::KW_IF,
	KW_ELSE		= PlnParser::token::KW_ELSE,
	KW_CONST	= PlnParser::token::KW_CONST,
	KW_AUTOTYPE = PlnParser::token::KW_AUTOTYPE,
	KW_VARLENARG	= PlnParser::token::KW_VARLENARG,
	KW_EXTERN	= PlnParser::token::KW_EXTERN,
	OPE_EQ		= PlnParser::token::OPE_EQ,
	OPE_NE		= PlnParser::token::OPE_NE,
	OPE_LE		= PlnParser::token::OPE_LE,
	OPE_GE		= PlnParser::token::OPE_GE,
	OPE_AND		= PlnParser::token::OPE_AND,
	OPE_OR		= PlnParser::token::OPE_OR,
	DBL_LESS	= PlnParser::token::DBL_LESS,
	DBL_GRTR	= PlnParser::token::DBL_GRTR,
	ARROW		= PlnParser::token::ARROW,
	DBL_ARROW	= PlnParser::token::DBL_ARROW,
	EQ_ARROW	= PlnParser::token::EQ_ARROW,
	AT_EXCL		= PlnParser::token::AT_EXCL
};

static string& unescape(string& str);

%}
%option c++
%option yyclass="PlnLexer"
%option noyywrap

DIGIT	[0-9]+
UDIGIT	[0-9]+"u"
FLOAT	[0-9]+"."[0-9]+
FLO_EX	[0-9]+"."[0-9]+"e"("+"|"-")?[0-9]+
DIGIT_MIN	"-9223372036854775808"
ID	[a-zA-Z_][0-9a-zA-Z_]*
DBL_LESS	"<<"
DBL_GRTR	">>"
ARROW		"->"
DBL_ARROW	"->>"
EQ_ARROW	"=>"
AT_EXCL		"@!"
DELIMITER	"{"|"}"|"("|")"|"["|"]"|","|";"|":"|"="|"+"|"-"|"*"|"/"|"%"|"<"|">"|"!"|"?"|"&"|"@"|"."
STRING	"\""(\\.|\\\n|[^\\\"])*"\""
COMMENT1	\/\/[^\n]*\n
POST_KW ([ \t\r\n(]|{COMMENT1})*		/* To keep priority than FUNC_ID. */

%%
%{
	loc.begin.filename = &filenames[cur_fid];
	loc.end.filename = &filenames[cur_fid];
	loc.step();
%}

{COMMENT1}	{ loc.lines(); loc.step(); }
{UDIGIT}	{
			lval.build<uint64_t>() = std::stoull(yytext);
			return UINT;
	}
{DIGIT_MIN} {
		lval.build<int64_t>() = -9223372036854775807-1;
		return INT;
	}
{DIGIT}	{
		int len = strlen(yytext);
		if (len < 19 || len == 19 && strcmp(yytext, "9223372036854775807")<=0) {
			lval.build<int64_t>() = std::stoll(yytext);
			return INT;
		} else {
			cerr << "Lexer: Overflow number: \"" << yytext << "\"" << endl;
			lval.build<int64_t>() = 0;
			return INT;
		}
	}
{FLOAT}|{FLO_EX}	{
			lval.build<double>() = std::stod(yytext);
			return FLOAT;
		}
type/{POST_KW}		{ return KW_TYPE; }
ccall/{POST_KW}		{ return KW_CCALL; }
syscall/{POST_KW}	{ return KW_SYSCALL; }
func/{POST_KW}		{ return KW_FUNC; }
return/{POST_KW}	{ return KW_RETURN; }
while/{POST_KW}		{ return KW_WHILE; }
break/{POST_KW}		{ return KW_BREAK; }
continue/{POST_KW}	{ return KW_CONTINUE; }
if/{POST_KW}		{ return KW_IF; }
else/{POST_KW}		{ return KW_ELSE; }
const/{POST_KW} 	{ return KW_CONST; }
var/{POST_KW} 	{ return KW_AUTOTYPE; }
extern/{POST_KW} 	{ return KW_EXTERN; }
"..."	{ return KW_VARLENARG; }
"=="	{ return OPE_EQ; }
"!="	{ return OPE_NE; }
"<="	{ return OPE_LE; }
">="	{ return OPE_GE; }
"&&"	{ return OPE_AND; }
"||"	{ return OPE_OR; }
{ID}/([ \t\r\n]|{COMMENT1})*"("	{
		string fid = yytext;
		lval.build<string>() = move(fid);
		return FUNC_ID;
	}
{ID}	{
		string id = yytext;
		lval.build<string>() = move(id);
		return ID;
	}
{STRING}	{
		string str(yytext+1,yyleng-2); 
		lval.build<string>() = unescape(str);
		return STR;
	}
{DBL_ARROW} {
		return DBL_ARROW;
	}
{ARROW} {
		return ARROW;
	}
{EQ_ARROW} {
		return EQ_ARROW;
	}
{DBL_LESS}	{
		return DBL_LESS;
	}
{DBL_GRTR}	{
		return DBL_GRTR;
	}
{AT_EXCL}	{
		return AT_EXCL;
	}
{DELIMITER}	{ return yytext[0]; }
[ \t]+		{ loc.step(); }
\r\n|\r|\n	{ loc.lines(); loc.step(); }
.	{
		cerr << "Lexer: Unrecognized char \"" << yytext[0] << "\"" << endl;
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
	filenames.push_back(filename);
	cur_fid = filenames.size()-1;
}

