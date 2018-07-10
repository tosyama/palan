/// Palan Lexer class declaration.
///
/// @file	PlnLexer.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#ifndef FLEX_SCANNER
#include <FlexLexer.h>
#endif

class PlnLexer : public yyFlexLexer {
	vector<string> typenames;

public:
	vector<string>	filenames;
	int cur_fid;

	void set_filename(const std::string& filename);
	void push_typename(string name);
	int yylex(palan::PlnParser::semantic_type& lval, palan::PlnParser::location_type& loc);
};
