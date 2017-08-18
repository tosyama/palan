/// Palan Lexer class declaration.
///
/// @file	PlnLexer.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#ifndef FLEX_SCANNER
#include <FlexLexer.h>
#endif

class PlnLexer : public yyFlexLexer {
	std::string	filename;
public:
	void set_filename(const std::string& filename);
	int yylex(palan::PlnParser::semantic_type& lval, palan::PlnParser::location_type& loc);
};
