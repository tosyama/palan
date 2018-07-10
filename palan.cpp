/// Palan compiler CUI.
///
/// Call lexcer, parser, generator, assembler(as) and linker(ld)
/// depend on command-line options.
///
/// @file	palan.cpp
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include <boost/program_options.hpp>
#include <boost/algorithm/string/join.hpp>
#include <fstream>
#include <sstream>
#ifdef __GNUC__
	#include <ext/stdio_sync_filebuf.h>    
	typedef __gnu_cxx::stdio_sync_filebuf<char> popen_filebuf;
#endif

#include "PlnMessage.h"
#include "models/PlnModule.h"
#include "generators/PlnX86_64DataAllocator.h"
#include "generators/PlnX86_64Generator.h"
#include "PlnModelTreeBuilder.h"
#include "PlnException.h"

using std::cout;
using std::cerr;
using std::endl;
using std::exception;
using std::ifstream;
using std::istream;
using std::stringstream;
using boost::algorithm::join;

namespace po = boost::program_options;

static string getDirName(string fpath);
static string getFileName(string& fpath);
static string getExtention(string& fpath);
static int getStatus(int ret_status);

// ErrorCode
const int COMPILE_ERR = 1;

/// Main function for palan compiler CUI.
int main(int argc, char* argv[])
{
	po::options_description opt("Options");
	po::positional_options_description p_opt;
	po::variables_map vm;
	bool do_asm = true;
	bool do_compile = false;
	bool do_link = false;
	string out_file;
	vector<string> object_files;
	string cmd_dir = getDirName(argv[0]);

	opt.add_options()
		("help,h", "Display this help")
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

	if (vm.count("compile")) {
		//TODO: error when set with -o option.
		do_asm = false;
		do_compile = true;
		do_link = false;
	}

	if (vm.count("input-file")){
		vector<string> files(vm["input-file"].as< vector<string> >());

		for (string& fname: files) {
			if (getExtention(fname) == "o") continue;

			PlnModule *module;
			{
				FILE* outf;
				json j;
				vector<string> files;

				string cmd =  cmd_dir + "pat \"" + fname + "\"";
				outf = popen(cmd.c_str(), "r");
				if (outf) {
					popen_filebuf p_buf(outf);
					istream pat_output(&p_buf);
					if (pat_output.peek() != std::ios::traits_type::eof())
						j = json::parse(pat_output);

					int ret = getStatus(pclose(outf));
					if (ret) return ret;

				} else {
					BOOST_ASSERT(false);
				}

				int fid = 0;
				for (auto finf: j["files"]) {
					BOOST_ASSERT(fid == finf["id"].get<int>());
					files.push_back(finf["name"].get<string>());
					fid++;
				}

				if (j["errs"].is_array()) {
					json &err = j["errs"][0];
					PlnLoc loc(err["loc"].get<vector<int>>());

					string error_msg = files[loc.fid] + ":" +to_string(loc.begin_line) + ": " + err["msg"].get<string>(); 
					cerr << error_msg << endl;
					return COMPILE_ERR;
				}

				try {
					PlnModelTreeBuilder modelTreeBuilder;
					module = modelTreeBuilder.buildModule(j["ast"]);

					PlnX86_64DataAllocator allocator;
					module->finish(allocator);

				} catch (PlnCompileError &err) {
					cerr << files[err.loc.fid] << ":" << err.loc.begin_line << ": " << PlnMessage::getErr(err.err_code, err.arg1, err.arg2);
					return COMPILE_ERR;
				}
			}

			if (do_asm) {
				PlnX86_64Generator generator(cout);
				module->gen(generator);
			} 

			if (do_compile) {
				FILE* as;
				string obj_file = getDirName(out_file) + getFileName(fname) + ".o";
				string cmd = "as -o \"" + obj_file + "\"" ;

				as = popen(cmd.c_str(), "w");
				popen_filebuf p_buf(as);
				ostream as_input(&p_buf);

				PlnX86_64Generator generator(as_input);
				module->gen(generator);

				int ret = getStatus(pclose(as));
				if (ret) return ret;

				object_files.push_back(obj_file);
			}
		}
	} 

	if (do_link) {
		string flist = join(object_files, " ");
		cout << "linking: " << out_file << endl;
		string cmd = "ld --dynamic-linker /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 -o " + out_file + " -lc " + flist;
		int ret = getStatus(system(cmd.c_str()));
		// TODO: error message
		if (ret) return ret;
	}

	if (vm.count("input-file")) return 0;

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
	int ext_i = fpath.find_last_of(".");
	if (ext_i < path_i) ext_i = fpath.length();
	return fpath.substr(path_i, ext_i-path_i);
}

string getExtention(string& fpath)
{
	int path_i = fpath.find_last_of("/\\")+1;
	int ext_i = fpath.find_last_of(".");
	if (ext_i < path_i) return "";
	return fpath.substr(ext_i);
}

int getStatus(int ret_status)
{
	if (WIFEXITED(ret_status)) {
		return WEXITSTATUS(ret_status);
	} else {
		return -1;
	}
}
