/// Palan compiler CUI.
///
/// Call lexcer, parser, generator, assembler(as) and linker(ld)
/// depend on command-line options.
///
/// @file	palan.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <boost/program_options.hpp>
#include <boost/algorithm/string/join.hpp>
#include <fstream>
#include <sstream>
#ifdef __GNUC__
	#include <ext/stdio_sync_filebuf.h>    
	typedef __gnu_cxx::stdio_sync_filebuf<char> popen_filebuf;
#endif

#include "PlnParser.hpp"
#include "PlnLexer.h"
#include "models/PlnModule.h"
#include "PlnMessage.h"
#include "generators/PlnX86_64DataAllocator.h"
#include "generators/PlnX86_64Generator.h"

using std::cout;
using std::cerr;
using std::endl;
using std::exception;
using std::ifstream;
using std::stringstream;
using boost::algorithm::join;
using palan::PlnParser;

namespace po = boost::program_options;

static string getFileName(string& fpath);
static string getExtention(string& fpath);

/// Main function for palan compiler CUI.
int main(int argc, char* argv[])
{
	po::options_description opt("Options");
	po::positional_options_description p_opt;
	po::variables_map vm;
	bool do_dump = false;
	bool do_asm = true;
	bool do_compile = false;
	bool do_link = false;
	string out_file;
	vector<string> object_files;

	opt.add_options()
		("help,h", "Display this help")
		("dump,d", "Dump semantic tree")
		("compile,c", "Compile and create object file")
		("output,o", po::value<string>(), "Output executable file")
		("input-file", po::value<vector<string>>(), "Input file");

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

	if (vm.count("help") || !vm.count("input-file")) {
		cout << opt;
		return 0;
	}

	if (vm.count("output")) {
		do_compile = true;
		do_asm = false;
		do_link = true;
		out_file = vm["output"].as<string>();
	}

	if (vm.count("dump")) {
		//TODO: error when set with -o option.
		do_dump = true;
		do_asm = false;
		do_compile = false;
		do_link = false;

	} else if (vm.count("compile")) {
		//TODO: error when set with -o option.
		do_dump = false;
		do_asm = false;
		do_compile = true;
		do_link = false;
	}

	if (vm.count("input-file")){
		vector<string> files(vm["input-file"].as< vector<string> >());

		for (string& fname: files) {
			if (getExtention(fname) == "o") continue;
			
			ifstream f;
			f.open(fname);

			if (f) {
				PlnLexer	lexer;
				lexer.set_filename(fname);
				lexer.switch_streams(&f, &cout);
				PlnModule module;

				PlnScopeStack	scopes;
				PlnParser parser(lexer, module, scopes);
				int res = parser.parse();

				if (res) return res;	// parse error
				PlnX86_64DataAllocator allocator;
				module.finish(allocator);

				if (do_dump) module.dump(cout);

				if (do_asm) {
						PlnX86_64Generator generator(cout);
						module.gen(generator);
				} 

				if (do_compile) {
					FILE* as;
					string obj_file = getFileName(fname) + ".o";
					string cmd = "as -o \"" + obj_file + "\"" ;

					as = popen(cmd.c_str(), "w");
					popen_filebuf p_buf(as);
					ostream as_input(&p_buf);

					PlnX86_64Generator generator(as_input);
					module.gen(generator);
					
					int ret = pclose(as);
					// TODO: error message
					if (ret) return ret;

					object_files.push_back(obj_file);
				}
			} else {
				string msg(PlnMessage::getErr(E_CouldnotOpenFile, fname));
				cerr << "error: " << msg << endl;
				return -1;
			}
		}
	} 

	if (do_link) {
		string flist = join(object_files, " ");
		cout << "linking: " << flist << endl;
		string cmd = "ld --dynamic-linker /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 -o " + out_file + " -lc " + flist;
		int ret = system(cmd.c_str());
		// TODO: error message
		if (ret) return ret;
	}

	if (vm.count("input-file")) return 0;

	return 0;
}

static string getFileName(string& fpath)
{
	int path_i = fpath.find_last_of("\\")+1;
	int ext_i = fpath.find_last_of(".");
	if (ext_i < path_i) ext_i = fpath.length();
	return fpath.substr(path_i, ext_i-path_i);
}

static string getExtention(string& fpath)
{
	int path_i = fpath.find_last_of("\\")+1;
	int ext_i = fpath.find_last_of(".");
	if (ext_i < path_i) return "";
	return fpath.substr(ext_i);
}

