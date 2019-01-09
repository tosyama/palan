/// Palan AST tool.
///
/// Call lexcer, parser and generate AST json.
///
/// @file	palanast.cpp
/// @copyright	2018-2019 YAMAGUCHI Toshinobu 

#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>
#include <boost/program_options.hpp>

#include "PlnParser.hpp"
#include "PlnLexer.h"
#include "../PlnMessage.h"
#include "../libs/json/single_include/nlohmann/json.hpp"

using std::cout;
using std::cerr;
using std::endl;
using std::exception;
using std::ifstream;
using std::ofstream;
using std::ostream;
using std::stringstream;
using palan::PlnParser;
using json = nlohmann::json;

namespace po = boost::program_options;

static string getDirName(string fpath);
static string getFileName(string& fpath);

int main(int argc, char* argv[])
{
	po::options_description opt("Options");
	po::positional_options_description p_opt;
	po::variables_map vm;

	opt.add_options()
		("help,h", "Display this help")
		("output,o", po::value<string>(), "Output AST json file")
		("indent,i", "Output indented json")
		("input-file", po::value<vector<string>>(), "Input palan file");

	p_opt.add("input-file", -1);

	try {
		po::store(po::command_line_parser(argc, argv)
				.options(opt).positional(p_opt).run(), vm);
	} catch (exception &e) {
		cerr << "error: " << e.what() << endl;
		cerr << opt;
		return -1;
	}

	po::notify(vm);

	if (vm.count("help") || vm.count("input-file")!=1) {
		cout << opt;
		return 0;
	}

	int indent = -1;
	if (vm.count("indent")) {
		indent = 2;
	}

	ostream *jout = &cout;
	ofstream of;
	if (vm.count("output")) {
		string out_file = vm["output"].as<string>();
		of.open(out_file);
		if (of) {
			jout = &of;
		}
	}

	vector<string> files(vm["input-file"].as< vector<string> >());
	string fname = files[0];
	ifstream f;
	f.open(fname);
	if (f) {
		PlnLexer	lexer;
		lexer.set_filename(fname);
		lexer.switch_streams(&f, &cout);

		json ast;
		
		PlnParser parser(lexer,ast);
		int res = parser.parse();

		// set src files infomation.
		json files;
		int id = 0;
		for (auto s_path: lexer.filenames) {
			json src_info = {
				{"id", id},
				{"name", getFileName(s_path)}
			};
			string dir_path = getDirName(s_path);
			if (dir_path != "") {
				src_info["dir"] = dir_path;
			}
			files.push_back(src_info);
		}
		ast["files"] = files;

		(*jout) << std::setw(indent) << ast << endl;

	} else {
		string msg(PlnMessage::getErr(E_CouldnotOpenFile, fname));
		cerr << "pat: error: " << msg << endl;
		return 1;
	}

	return 0;
}

string getDirName(string fpath)
{
	int path_i = fpath.find_last_of("/\\")+1;
	return fpath.substr(0, path_i);
}

string getFileName(string& fpath)
{
	int path_i = fpath.find_last_of("/\\")+1;
	return fpath.substr(path_i, fpath.length());
}
