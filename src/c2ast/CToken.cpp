/// C token definitions.
///
/// @file CToken.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <vector>
#include <string>
#include <boost/assert.hpp>
using namespace std;
#include "CToken.h"

CToken0::CToken0(CToken0Type type, int line_no, int pos, int len)
	: type(type), is_eol(false)
{
	this->line_no = line_no;
	this->pos = pos;
	this->len = len;
}

CToken::CToken(CTokenType type, int lexer_no, int token0_no)
	: type(type), lexer_no(lexer_no), token0_no(token0_no), hide_set(NULL)
{
}

CToken::CToken(const CToken& token)
	: type(token.type), lexer_no(token.lexer_no), token0_no(token.token0_no)
{
	switch (type) {
		case TT_ID:
			info.id = new string(*token.info.id); break; // LCOV_EXCL_BR_LINE

		case TT_KEYWORD:
			info.keyword = token.info.keyword; break;

		case TT_PUNCTUATOR:
			info.punc = token.info.punc; break;

		case TT_PP_NUMBER:
		case TT_CHAR:
		case TT_STR:
			info.str = new string(*token.info.str); break; // LCOV_EXCL_BR_LINE
	
		default:
			BOOST_ASSERT(false);
	}

	if (token.hide_set) {
		hide_set = new vector<CMacro*>(*token.hide_set); // LCOV_EXCL_BR_LINE
	} else {
		hide_set = NULL;
	}

}

CToken::CToken(const string& s, int lexer_no, int token0_no)
	: type(TT_STR), lexer_no(lexer_no), token0_no(token0_no), hide_set(NULL)
{
	info.str = new string(s);
}

CToken::~CToken()
{
	if (hide_set) {
		delete hide_set;
	}
}

void CToken::add_to_hide_set(CMacro* m)
{
	if (type != TT_ID)
		return;	// only ID tokens have hide set

	if (!hide_set) {
		hide_set = new vector<CMacro*>();
	}
	hide_set->push_back(m);
}

bool CToken::does_hide(CMacro* m)
{
	if (hide_set) {
		for (CMacro* hm: *hide_set) {
			if (hm == m) {
				return true;
			}
		}
	}
	return false;
}
