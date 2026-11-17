/// C lexer: splits a C header into preprocessing tokens.
///
/// @file CLexer.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <vector>
#include <string>
#include <fstream>
#include <boost/assert.hpp>
#include <cctype>
#include <regex>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <map>

using namespace std;
#include "CFileInfo.h"
#include "CToken.h"
#include "CLexer.h"

vector<string> CLexer::keywords;

// GCC's alternate keyword spellings. GCC takes them as keywords whether or
// not __GNUC__ is defined, and Linux kernel headers rely on that.
static const map<string, CTokenKeyword> gnuAlternates = {
	{"__const", TK_CONST}, {"__const__", TK_CONST},
	{"__volatile", TK_VOLATILE}, {"__volatile__", TK_VOLATILE},
	{"__inline", TK_INLINE}, {"__inline__", TK_INLINE},
	{"__signed", TK_SIGNED}, {"__signed__", TK_SIGNED},
	{"__restrict", TK_RESTRICT}, {"__restrict__", TK_RESTRICT},
	{"__complex__", TK_COMPLEX},
};

static int read_number(string& str, int n);
static int read_id(string& str, int n);

CLexer::CLexer(CFileInfo &infile) : infile(move(infile)), no(-1)
{
	if (keywords.size() == 0) {
		keywords.resize(TK_NOT_KEYWORD);
		keywords[TK_TYPEDEF] = "typedef";
		keywords[TK_EXTERN] = "extern";
		keywords[TK_STATIC] = "static";
		keywords[TK_CONST] = "const";
		keywords[TK_SIZEOF] = "sizeof";
		keywords[TK_ALIGNOF] = "_Alignof";
		keywords[TK_GNU_ALIGNOF] = "__alignof__";
		keywords[TK_SIGNED] = "signed";
		keywords[TK_UNSIGNED] = "unsigned";
		keywords[TK_SHORT] = "short";
		keywords[TK_LONG] = "long";
		keywords[TK_INT] = "int";
		keywords[TK_CHAR] = "char";
		keywords[TK_FLOAT] = "float";
		keywords[TK_DOUBLE] = "double";
		keywords[TK_STRUCT] = "struct";
		keywords[TK_UNION] = "union";
		keywords[TK_ENUM] = "enum";
		keywords[TK_VOID] = "void";
		keywords[TK_BOOL] = "_Bool";
		keywords[TK_COMPLEX] = "_Complex";
		keywords[TK_INT128] = "__int128";
		keywords[TK_INLINE] = "inline";
		keywords[TK_RESTRICT] = "restrict";
		keywords[TK_VOLATILE] = "volatile";
		// A keyword, as in GCC, so the "#define __attribute__(xyz)" that
		// sys/cdefs.h issues for non-GNU compilers never expands.
		keywords[TK_ATTRIBUTE] = "__attribute__";
	}
}

vector<CToken0>& CLexer::scan()
{
	int line_no = 0;
	bool in_comment = false;
	bool in_str_literal = false;
	int start_pos;	// for comment, string literal

	for (string& line : infile.lines) {
		// use for macro detection
		int token_num = 0;	// in a line. comment is not counted.
		bool in_brace_str = false;
		bool macro_mode = false;
		bool include_file_mode = false;
		bool define_mode = false;

		line_no++;
		start_pos = 0;
		token_num = 0;

		if (tokens.size()) {
			tokens.back().is_eol = true;
		}

		for (int n = 0; n < line.size(); ) {
			char c0 = line[n];
			char c1 = (n+1 < line.size()) ? line[n+1] : '\0';
			if (token_num >= 3) {
				include_file_mode = false;
				define_mode = false;
			}

			if (in_comment) {	// Comment
				if (c0 == '*') {
					if (c1 == '/') {	// Block comment end
						in_comment = false;
						n += 2;
						int len = n - start_pos;
						tokens.emplace_back(TT0_COMMENT, line_no, start_pos, len);
						continue;
					}
				}
				n++;
				continue;
			}

			if (in_str_literal) {	// String literal
				if (c0 == '\\' && c1) {
					n += 2;
					continue;
				}

				if (c0 == '\"') {
					in_str_literal = false;
					n++;
					int len = n - start_pos;
					token_num++;
					tokens.emplace_back(TT0_STR, line_no, start_pos, len);
					continue;
				}
				n++;
				continue;
			}

			// Space
			if (isspace(c0)) {
				n++;
				continue;
			}

			if (c0 == '/') {
				if (c1 == '/') {	// Line comment
					int len = line.size() - n;
					tokens.emplace_back(TT0_COMMENT, line_no, n, len);
					break;
				}

				if (c1 == '*') {	// Block comment start
					in_comment = true;
					start_pos = n;
					n++;
					continue;
				}
			}

			// Number
			if (isdigit(c0) || c0 == '.' && isdigit(c1)) {
				int len = read_number(line, n);
				tokens.emplace_back(TT0_NUMBER, line_no, n, len);
				n += len;
				token_num++;
				continue;
			}

			// String literal start
			if (c0 == '\"') {
				in_str_literal = true;
				start_pos = n;
				n++;
				continue;
			}

			// Identifier
			if (isalpha(c0) || c0 == '_') {
				int len = read_id(line, n);
				tokens.emplace_back(TT0_ID, line_no, n, len);
				token_num++;
				if (macro_mode && token_num == 2) {
					// detect "include"
					if (len == 7) {
						const char* idstr = line.c_str() + n;
						if (!strncmp(idstr, "include", len)) {
							include_file_mode = true;
						}
					} else if (len == 6) {
						const char* idstr = line.c_str() + n;
						if (!strncmp(idstr, "define", len)) {
							define_mode = true;
						}
					}
				} else if (define_mode && token_num == 3) {
					int next_pos = n+len;
					if (next_pos < line.size()) {
						if (line[n+len] == '(') {
							tokens.emplace_back(TT0_MACRO_ARGS, line_no, n+len, 1);
							token_num++;
							len++;
						}
					}
				}

				n += len;
				continue;
			}

			if (include_file_mode) {	// Special flow for "#include <...>"
				if (token_num == 2 && c0 == '<') {
					int s = n;
					int len = 0;
					for (; n<line.size(); n++) {
						len++;
						if (line[n] == '>') {
							n++;
							break;
						}
					}
				
					tokens.emplace_back(TT0_STR, line_no, s, len);
					token_num++;
					continue;
				}
			}

			// Charactor Constant
			if (c0=='\'') { 
				int s = n;
				n++;
				for (; n < line.size(); n++) {
					char c0 = line[n];
					if (c0 == '\\') {
						n++;
						continue;
					} else if (c0 == '\'') {
						n++;
						break;
					}
				}
				tokens.emplace_back(TT0_CHAR, line_no, s, n-s);
				token_num++;
				continue;
			}

			int16_t c = (c0 << 8) | c1;

			// Punctuator
			{
				int len = 1;
				if (c == '<<' || c == '>>') {	// also "<<=", ">>=", "..."
					char c2 = (n+2 < line.size()) ? line[n+2] : '\0';
					len = 2;
					if (c2 == '=') {
						len = 3;
					}

				} else if (c == '==' || c == '!=' || c == '<=' || c == '>='
						|| c == '+=' || c == '-=' || c == '*=' || c == '/=' || c == '%='
						|| c == '&=' || c == '|=' || c == '^='
						|| c == '&&' || c == '||'
						|| c == '##') {
					len = 2;

				} else if (c == '..') {
					char c2 = (n+2 < line.size()) ? line[n+2] : '\0';
					if (c2 == '.') {
						len = 3;
					}
				}

				if (token_num==0 && c0=='#' && len==1) {
					tokens.emplace_back(TT0_MACRO, line_no, n, 1);
					macro_mode = true;

				} else {
					tokens.emplace_back(TT0_PUNCTUATOR, line_no, n, len);
				}

				n += len;
				token_num++;
				continue;
			}
			BOOST_ASSERT(false);
		}

		if (in_comment) { // Block comment continues next line
			int len = line.size() - start_pos;
			tokens.emplace_back(TT0_COMMENT, line_no, start_pos, len);
		}

		if (in_str_literal) { // String Literal continues next line
			int len = line.size() - start_pos;
			tokens.emplace_back(TT0_STR, line_no, start_pos, len);
		}
	}

	if (tokens.size())
		tokens.back().is_eol = true;

	return tokens;
}

static regex rex_numberpart("^[0-9A-Za-z_\\.]+");
static regex rex_sign("^[+-][0-9A-Za-z_\\.]+");

int read_number(string& str, int n)
{	// Note: this exclude number-like token include invalid format.
	cmatch m;
	const char* target_str = str.c_str() + n;
	regex_search(target_str,  m, rex_numberpart);
	BOOST_ASSERT(m.size() == 1);
	int len = m.length(0);
	BOOST_ASSERT(len > 0);

	char c = str[n+len-1];
	if ((c == 'E' || c == 'e' || c == 'P' || c == 'p') && (n+len)<str.size() ) {
		const char* target_str_e = str.c_str() + n + len;
		regex_search(target_str_e,  m, rex_sign);
		if (m.size() >= 1) {
			len += m.length(0);
		}
	}

	return len;
}

static regex rex_id("^[A-Za-z_]{1}[A-Za-z0-9_]*");
int read_id(string& str, int n)
{
	cmatch m;
	const char* target_str = str.c_str() + n;
	regex_search(target_str,  m, rex_id);
	BOOST_ASSERT(m.size() == 1);
	return m.length(0);
}

static string& unescape(string& str);
static int ch2int(const string &s, int pos, int len);

CToken* CLexer::createToken(int n)
{
	CToken0 *t0 = &tokens[n];
	CToken *t = NULL;

	if (t0->type == TT0_ID) {
		string str = get_str(t0);

		for (int i=0; i<TK_NOT_KEYWORD; i++) {
			if (str == keywords[i]) {	
				t = new CToken(TT_KEYWORD, no, n); // LCOV_EXCL_BR_LINE
				t->info.keyword = (CTokenKeyword)i;
				return t;
			}
		}
		auto alt = gnuAlternates.find(str);
		if (alt != gnuAlternates.end()) {
			t = new CToken(TT_KEYWORD, no, n); // LCOV_EXCL_BR_LINE
			t->info.keyword = alt->second;
			return t;
		}

		t = new CToken(TT_ID, no, n); // LCOV_EXCL_BR_LINE
		t->info.id = new string(move(str));

		return t;

	} else if (t0->type == TT0_PUNCTUATOR) {
		t = new CToken(TT_PUNCTUATOR, no, n); // LCOV_EXCL_BR_LINE
		t->info.punc = get_ch(t0);

	} else if (t0->type == TT0_NUMBER) {
		t = new CToken(TT_PP_NUMBER, no, n); // LCOV_EXCL_BR_LINE
		t->info.str = new string(get_str(t0)); // LCOV_EXCL_BR_LINE
		return t;

	} else if (t0->type == TT0_CHAR) {
		t = new CToken(TT_CHAR, no, n); // LCOV_EXCL_BR_LINE
		t->info.str = new string(get_str(t0)); // LCOV_EXCL_BR_LINE
		return t;

	} else if (t0->type == TT0_STR) {
		string str = get_str(t0);
		t = new CToken(TT_STR, no, n); // LCOV_EXCL_BR_LINE
		t->info.str = new string(move(str));
		return t;

	} else {
		BOOST_ASSERT(t0->type == TT0_COMMENT);
		return NULL;
	}

	return t;
}

string CLexer::get_str(CToken0* t0)
{
	return infile.lines[t0->line_no-1].substr(t0->pos, t0->len);
}

static int hexc(int c)
{
	if (c>='0' && c<='9') return c-'0';
	if (c>='a' && c<='f') return 10+c-'a';
	if (c>='A' && c<='F') return 10+c-'A';
	return -1;
}

string& unescape(string& str)
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

int ch2int(const string &s, int pos, int len)
{
	BOOST_ASSERT(len <= 8);
	if (len == 1) { return s[pos]; }

	int c = 0;
	for (int i=0; i<len; i++) {
		c <<= 8;
		c |= s[pos+i];
	}

	return c;
}

int CLexer::get_ch(CToken0* t0)
{
	BOOST_ASSERT(t0->type == TT0_PUNCTUATOR);
	BOOST_ASSERT(t0->len <= 3);
	int n = t0->line_no-1;

	return ch2int(infile.lines[n], t0->pos, t0->len);
}

string CLexer::get_oristr(int token0_no)
{
	CToken0 *t0 = &tokens[token0_no];
	return infile.lines[t0->line_no-1].substr(t0->pos, t0->len);
}

string CLexer::get_oristr(const CToken *start, const CToken *end)
{
	BOOST_ASSERT(start->lexer_no == no);
	BOOST_ASSERT(end->lexer_no == no);
	BOOST_ASSERT(start->token0_no <= end->token0_no);

	int start_line = tokens[start->token0_no].line_no-1;
	int start_pos = tokens[start->token0_no].pos;
	int end_line = tokens[end->token0_no].line_no-1;
	int end_len = tokens[end->token0_no].pos + tokens[end->token0_no].len;

	if (start_line == end_line) {
		end_len -= start_pos;
		return infile.lines[start_line].substr(start_pos, end_len);
	} else {
		string s = infile.lines[start_line].substr(start_pos) + "\n";
		for (int n=start_line+1; n<end_line; n++) {
			s += infile.lines[n] + "\n";
		}
		s += infile.lines[end_line].substr(0, end_len);
		return s;
	}

	return "";
}

