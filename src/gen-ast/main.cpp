/// Palan AST JSON Generator
/// Command line tool main.
///
/// @file main.cpp
/// @copyright 2024-2026 YAMAGUCHI Toshinobu

#include <iostream>
#include <vector>
#include <fstream>
#include <getopt.h>
#include <filesystem>

namespace fs = std::filesystem;

#include "../common/PlnDefs.h"
#include "PlnGenAstMessage.h"
#include "PlnGenAstMacroFold.h"
#include "PlnGenAstC2Ast.h"
#include "../common/PlnInterface.h"
#include "PlnParser.h"
#include "PlnLexer.h"

using namespace std;
using namespace palan;

int main(int argc, char* argv[])
{
	struct option long_options[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "version", no_argument, NULL, 'v' },
		{ "output", required_argument, NULL, 'o' },
		{ "indent", no_argument, NULL, 'i' },
		{ "target", required_argument, NULL, 't' },
		{ "interface", required_argument, NULL, 'I' },
		{ 0 }
	};

	int opt, option_index = 0;
	char *output_file = NULL;
	char *interface_file = NULL;
	bool do_indent = false;
	string target = PALAN_TARGET;

	while (0 < (opt = getopt_long(argc, argv, "hvo:i", long_options, NULL))) {
		switch (opt) {
			case 'h':
				cout << PlnGenAstMessage::getMessage(M_Help) << endl;
				return 0;
			case 'v':
				cout << PlnGenAstMessage::getMessage(M_Version) << endl;
				return 0;
			case 'o':
				output_file = optarg;
				break;
			case 'i':
				do_indent = true;
				break;
			case 't':
				target = optarg;
				break;
			case 'I':
				interface_file = optarg;
				break;
			default:
				break;
		}
	}

	if (!isSupportedTarget(target)) {
		cerr << PlnGenAstMessage::getMessage(E_UnsupportedTarget, target) << endl;
		return 1;
	}

	if ((argc-optind) != 1) {
		cerr << PlnGenAstMessage::getMessage(E_NoInputFile) << endl;
		return 1;
	}
	

	fs::path input_file = fs::weakly_canonical(argv[optind]);

	try {
		PlnLexer lexer(input_file.string());
		json ast;
		MacroTable macros;
		CStateHistory cState;
		PlnParser parser(lexer, ast, macros, cState, target);

		int ret;
		ret = parser.parse();
		ast["original"] = input_file;
		ast["target"] = target;
		if (ret == 0) {
			foldMacroConstants(ast["ast"], macros);
			if (ast.contains("export")) foldMacroConstants(ast["export"], macros);
			if (interface_file) {
				string interface = writeInterface(ast);
				ofstream of(interface_file);
				if (!of) {
					cerr << PlnGenAstMessage::getMessage(E_CouldNotOpenOutputFile, interface_file) << endl;
					return 1;
				}
				of << interface;
			}
			string out_str = do_indent ? ast.dump(2) : ast.dump();
			if (output_file) {
				ofstream of(output_file);
				if (!of) {
					cerr << PlnGenAstMessage::getMessage(E_CouldNotOpenOutputFile, output_file) << endl;
					return 1;
				}
				of << out_str;
			} else {
				cout << out_str;
			}
		} else { // 1: parse err, 2: memory err
			return 1;
		}

	} catch(runtime_error& e) {
		cerr << e.what() << endl;
		return 1;
	}

	return 0;
}

