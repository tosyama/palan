%{
/// Palan Lexical analyzer.
/// 
/// PlnLexer.cpp will be created by flex.
///
/// @file PlnLexer.ll
/// @copyright 2024-2026 YAMAGUCHI Toshinobu

#include <string>
#include <fstream>
#include "PlnParser.h"
#include "PlnLexer.h"
#include "PlnGenAstMessage.h"
#include "../common/PlnStrLit.h"

#include <boost/assert.hpp>

using namespace std;
using namespace palan;

#undef	YY_DECL
#define	YY_DECL int PlnLexer::yylex(PlnParser::value_type& lval, PlnParser::location_type& loc)
#define YY_USER_ACTION	loc.columns(yyleng);

enum {
	INT =	PlnParser::token::INT,
	CHAR_LIT =	PlnParser::token::CHAR_LIT,
	UINT =	PlnParser::token::UINT,
	ID =	PlnParser::token::ID,
	STRING =	PlnParser::token::STRING,
	PATH =	PlnParser::token::PATH,
	INCLUDE_FILE =	PlnParser::token::INCLUDE_FILE,
	KW_LIBRARY =	PlnParser::token::KW_LIBRARY,
	KW_EXPORT =	PlnParser::token::KW_EXPORT,
	KW_CEXPORT =	PlnParser::token::KW_CEXPORT,
	KW_IMPORT =	PlnParser::token::KW_IMPORT,
	KW_CINCLUDE =	PlnParser::token::KW_CINCLUDE,
	KW_FROM =	PlnParser::token::KW_FROM,
	KW_AS	=	PlnParser::token::KW_AS,
	KW_FUNC =	PlnParser::token::KW_FUNC,
	KW_TYPE =	PlnParser::token::KW_TYPE,
	KW_ENUM =	PlnParser::token::KW_ENUM,
	KW_OPAQUE =	PlnParser::token::KW_OPAQUE,
	KW_RELEASE =	PlnParser::token::KW_RELEASE,
	KW_CONST =	PlnParser::token::KW_CONST,
	KW_GLOBAL =	PlnParser::token::KW_GLOBAL,
	KW_VOID =	PlnParser::token::KW_VOID,
	KW_RETURN = PlnParser::token::KW_RETURN,
	KW_FOR =	PlnParser::token::KW_FOR,
	KW_WHILE =	PlnParser::token::KW_WHILE,
	KW_IF = PlnParser::token::KW_IF,
	KW_ELSE = PlnParser::token::KW_ELSE,
	FLO =	PlnParser::token::FLO,
	KW_BREAK = PlnParser::token::KW_BREAK,
	KW_CONTINUE = PlnParser::token::KW_CONTINUE,
	KW_SYSCALL = PlnParser::token::KW_SYSCALL,
	KW_SIZEOF = PlnParser::token::KW_SIZEOF,
	KW_TRUE = PlnParser::token::KW_TRUE,
	KW_FALSE = PlnParser::token::KW_FALSE,
	OPE_LE =	PlnParser::token::OPE_LE,
	OPE_GE =	PlnParser::token::OPE_GE,
	DBL_GRTR =	PlnParser::token::DBL_GRTR,
	OPE_SHL =	PlnParser::token::OPE_SHL,
	OPE_SHR =	PlnParser::token::OPE_SHR,
	OPE_APPEND =	PlnParser::token::OPE_APPEND,
	ARROW =	PlnParser::token::ARROW,
	DBL_ARROW =	PlnParser::token::DBL_ARROW,
	OPE_TAKE =	PlnParser::token::OPE_TAKE,
	AT_EXCL =	PlnParser::token::AT_EXCL,
	OPE_EQ =	PlnParser::token::OPE_EQ,
	OPE_NE =	PlnParser::token::OPE_NE,
	OPE_AND =	PlnParser::token::OPE_AND,
	OPE_OR =	PlnParser::token::OPE_OR,
	ELLIPSIS =	PlnParser::token::ELLIPSIS
};

struct LitError { PlnMessageCode code; string arg; };
static bool unescape(string& str, LitError& err);
static long charLiteralCode(const string& text, LitError& err);

PlnLexer::PlnLexer(const string& input_file)
	: inputFile(input_file), inStream(input_file), yyFlexLexer()
{
	if (!inStream) {
		throw runtime_error(PlnGenAstMessage::getMessage(E_CouldNotOpenFile, input_file));
	}
	this->switch_streams(&inStream);
}

%}

%option c++
%option yyclass="PlnLexer"
%option noyywrap

DIGIT	[0-9]+
UDIGIT	[0-9]+"u"
FLOAT	[0-9]+\.[0-9]+([eE][+-]?[0-9]+)?|[0-9]+[eE][+-]?[0-9]+
BADNUM	[0-9]+(\.[0-9]+)?[a-zA-Z_][0-9a-zA-Z_]*
ID	[a-zA-Z_][0-9a-zA-Z_]*
DEMILITER	"{"|"}"|"("|")"|"["|"]"|","|";"|":"|"="|"+"|"-"|"*"|"/"|"%"|"<"|">"|"!"|"?"|"&"|"@"|"."|"$"|"|"|"^"|"~"
STRING	"\""(\\.|\\\n|[^\\\"])*"\""
CHAR	"'"(\\.|[^\\'\n])*"'"
PATH	"\""[^\"\n]*"\""
INCLUDE_FILE	"<"[^>\n]*">"
COMMENT1	\/\/[^\n]*\n

%x import
%%
%{
	loc.step();
%}

<*>{FLOAT} {
		lval.build<string>() = yytext;
		return FLO;
	}
<*>{UDIGIT} {
		lval.build<string>() = string(yytext, yyleng-1);
		return UINT;
	}
<*>{DIGIT} {
		lval.build<string>() = yytext;
		return INT;
	}
<*>{BADNUM} {
		throw runtime_error(PlnGenAstMessage::locatedError(inputFile,
			loc.begin.line, loc.begin.column,
			PlnGenAstMessage::getMessage(E_InvalidNumberLiteral, yytext)));
	}
<*>"library" { return KW_LIBRARY; }
<*>"export" { return KW_EXPORT; }
<*>"cexport" { return KW_CEXPORT; }
<*>"import" {
		BEGIN(import);
		return KW_IMPORT;
	}
<*>"cinclude" {
		BEGIN(import);
		return KW_CINCLUDE;
	}
<import>"from"	{ return KW_FROM; }
<import>{PATH}	{
		BEGIN(0);
		string str(yytext+1, yyleng-2);
		lval.build<string>() = move(str);
		return PATH;
	}
<import>{INCLUDE_FILE}	{
		BEGIN(0);
		string str(yytext+1, yyleng-2);
		lval.build<string>() = move(str);
		return INCLUDE_FILE;
	}
<*>"as"	{ return KW_AS; }
<*>"func" { return KW_FUNC; }
<*>"type" { return KW_TYPE; }
<*>"enum" { return KW_ENUM; }
<*>"opaque" { return KW_OPAQUE; }
<*>"release" { return KW_RELEASE; }
<*>"const" { return KW_CONST; }
<*>"global" { return KW_GLOBAL; }
<*>"void" { return KW_VOID; }
<*>"return" { return KW_RETURN; }
<*>"for" { return KW_FOR; }
<*>"while" { return KW_WHILE; }
<*>"if" { return KW_IF; }
<*>"else" { return KW_ELSE; }
<*>"break" { return KW_BREAK; }
<*>"continue" { return KW_CONTINUE; }
<*>"syscall" { return KW_SYSCALL; }
<*>"sizeof" { return KW_SIZEOF; }
<*>"true" { return KW_TRUE; }
<*>"false" { return KW_FALSE; }
<*>{ID} {
		string id = yytext;
		lval.build<string>() = move(id);
		return ID;
	}
<INITIAL>{STRING}	{
		string str(yytext+1, yyleng-2);
		LitError err;
		if (!unescape(str, err))
			throw runtime_error(PlnGenAstMessage::locatedError(inputFile,
				loc.begin.line, loc.begin.column, PlnGenAstMessage::getMessage(err.code, err.arg)));
		lval.build<string>() = move(str);
		return STRING;
	}
<INITIAL>{CHAR}	{
		LitError err;
		long code = charLiteralCode(string(yytext, yyleng), err);
		if (code < 0)
			throw runtime_error(PlnGenAstMessage::locatedError(inputFile,
				loc.begin.line, loc.begin.column, PlnGenAstMessage::getMessage(err.code, err.arg)));
		lval.build<string>() = to_string(code);
		return CHAR_LIT;
	}
<*>{COMMENT1}	{ loc.lines(); loc.step(); }
<*>{DEMILITER} { return yytext[0]; }
<*>"<="	{ return OPE_LE; }
<*>">="	{ return OPE_GE; }
<*>">>"	{ return DBL_GRTR; }
<*>"<<<"	{ return OPE_SHL; }
<*>">>>"	{ return OPE_SHR; }
<*>"<<"	{ return OPE_APPEND; }
<*>"->"	{ return ARROW; }
<*>"->>"	{ return DBL_ARROW; }
<*>"<<="	{ return OPE_TAKE; }
<*>"@!"	{ return AT_EXCL; }
<*>"=="	{ return OPE_EQ; }
<*>"!="	{ return OPE_NE; }
<*>"&&"	{ return OPE_AND; }
<*>"||"	{ return OPE_OR; }
<*>"..."	{ return ELLIPSIS; }
<*>[ \t]+	{ loc.step(); }
<*>\r\n|\r|\n	{ loc.lines(); loc.step(); }
<*>.	{
		// Printing a lone byte of a UTF-8 sequence would garble the terminal.
		unsigned char c = yytext[0];
		string ch(1, c);
		if (c < 0x20 || c > 0x7e) {
			char buf[8];
			snprintf(buf, sizeof(buf), "\\x%02X", c);
			ch = buf;
		}
		// Not PlnParser::syntax_error: glr2.cc reports a scanner-thrown
		// syntax_error a second time when the parser stack is split.
		throw runtime_error(PlnGenAstMessage::locatedError(inputFile,
			loc.begin.line, loc.begin.column,
			PlnGenAstMessage::getMessage(E_UnexpectedChar, ch)));
	}
<*><<EOF>>	{ return 0; }

%%

static int hexc(int c)
{
	if (c>='0' && c<='9') return c-'0';
	if (c>='a' && c<='f') return 10+c-'a';
	if (c>='A' && c<='F') return 10+c-'A';
	return -1;
}

// `\u{X}` at s[i] (the backslash): its code point, with i moved to its last
// character; -1 when it is malformed or names no Unicode scalar value.
static long codePointEscape(const string& s, size_t& i, LitError& err)
{
	size_t start = i;
	size_t k = i + 2;
	long cp = 0;
	int digits = 0;
	bool open = k - 1 < s.size() && s[k - 1] == 'u' && k < s.size() && s[k] == '{';
	if (open) {
		for (++k; k < s.size() && hexc(s[k]) >= 0; ++k, ++digits)
			if (digits < 7) cp = cp * 16 + hexc(s[k]);
	}
	bool closed = open && k < s.size() && s[k] == '}';
	if (!closed || digits < 1 || digits > 6 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
		size_t end = closed ? k + 1 : min(k, s.size());
		err = {E_InvalidCodePointEscape, s.substr(start, max(end, start + 2) - start)};
		return -1;
	}
	i = k;
	return cp;
}

static void appendUtf8(string& out, long cp)
{
	if (cp < 0x80) {
		out += char(cp);
	} else if (cp < 0x800) {
		out += char(0xC0 | (cp >> 6));
		out += char(0x80 | (cp & 0x3F));
	} else if (cp < 0x10000) {
		out += char(0xE0 | (cp >> 12));
		out += char(0x80 | ((cp >> 6) & 0x3F));
		out += char(0x80 | (cp & 0x3F));
	} else {
		out += char(0xF0 | (cp >> 18));
		out += char(0x80 | ((cp >> 12) & 0x3F));
		out += char(0x80 | ((cp >> 6) & 0x3F));
		out += char(0x80 | (cp & 0x3F));
	}
}

static bool unescape(string& str, LitError& err)
{
	string out;
	for (size_t s = 0; s < str.size(); ++s) {
		if (str[s] != '\\') {
			out += str[s];
			continue;
		}
		if (str[s + 1] == 'u') {
			long cp = codePointEscape(str, s, err);
			if (cp < 0) return false;
			appendUtf8(out, cp);
			continue;
		}
		++s;
		switch (str[s]) {
			case 'a': out += '\a'; break;
			case 'b': out += '\b'; break;
			case 'n': out += '\n'; break;
			case 'r': out += '\r'; break;
			case 't': out += '\t'; break;
			case 'v': out += '\v'; break;
			case '0': out += '\0'; break;

			case 'x': {
				int h1 = hexc(str[s+1]), h2 = hexc(str[s+2]);
				if (h1 >= 0 && h2 >= 0) {
					out += char(16*h1 + h2); s+=2;
				} else {
					out += 'x';
				}
			} break;

			default: out += str[s];
		}
	}
	str = move(out);
	return true;
}

// One character of a character literal's text at text[i], moving i past it.
static long charLiteralUnit(const string& text, size_t& i, LitError& err)
{
	unsigned char c = text[i];
	if (c == '\\') {
		if (text[i + 1] == 'u') {
			long cp = codePointEscape(text, i, err);
			++i;
			return cp;
		}
		i += 2;
		switch (text[i - 1]) {
			case 'n': return '\n';
			case 't': return '\t';
			case 'r': return '\r';
			case '0': return 0;
			case '\\': return '\\';
			case '\'': return '\'';
		}
		err = {E_InvalidCharLiteral, ""};
		return -1;
	}
	if (c < 0x80) {
		++i;
		if (c < 0x20 || c > 0x7e) {
			err = {E_InvalidCharLiteral, ""};
			return -1;
		}
		return c;
	}
	size_t n = utf8SeqAt(text, i);
	if (n == 0) {
		err = {E_CharLitInvalidUtf8, ""};
		return -1;
	}
	long cp = c & (0x7F >> n);
	for (size_t k = 1; k < n; ++k)
		cp = cp << 6 | (text[i + k] & 0x3F);
	i += n;
	return cp;
}

// text includes the surrounding quotes.
static long charLiteralCode(const string& text, LitError& err)
{
	string body = text.substr(1, text.size() - 2);
	size_t i = 0;
	long code = -1;
	if (body.empty()) err = {E_InvalidCharLiteral, ""};
	else code = charLiteralUnit(body, i, err);
	// The rest is read only to tell bytes that are no text from several characters.
	bool several = false;
	while (code >= 0 && i < body.size()) {
		if (charLiteralUnit(body, i, err) < 0) code = -1;
		several = true;
	}
	if (code >= 0 && several) {
		err = {E_CharLitSeveralCodePoints, text};
		code = -1;
	}
	if (code < 0 && err.code == E_InvalidCharLiteral) err.arg = text;
	return code;
}
