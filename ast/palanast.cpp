/// Palan AST tool.
///
/// Call lexcer, parser and generate AST json.
///
/// @file	palanast.cpp
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#include "PlnParser.hpp"
#include "PlnLexer.h"
#include "../libs/json/single_include/nlohmann/json.hpp"

using std::cout;
using std::cerr;
using std::endl;
using std::exception;
using std::ifstream;
using std::stringstream;
using palan::PlnParser;
using json = nlohmann::json;

int main(int argc, char* argv[])
{
	vector<string> files {
		"../test/pacode/002_varint64.pa"
	};

	for (string& fname: files) {
		ifstream f;
		f.open(fname);
		if (f) {
			PlnLexer	lexer;
			lexer.set_filename(fname);
			lexer.switch_streams(&f, &cout);

			json ast;
			PlnParser parser(lexer,ast);
			int res = parser.parse();

			cout << ast.dump() << endl;
		}
	}

	return 0;
}
