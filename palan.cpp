/// Palan compiler CUI.
///
/// Call parser, generator, assembler(as) and linker(ld)
/// depend on command-line options.
///
/// @file	palan.cpp
/// @copyright	2017-2018 YAMAGUCHI Toshinobu 

#include <boost/program_options.hpp>
#include <boost/algorithm/string/join.hpp>
#include <fstream>
#include <sstream>
#include <cstdlib>
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
static string usage();
static string note();

// ErrorCode
const int COMPILE_ERR = 1;
const int PARAM_ERR = -1;

/// Main function for palan compiler CUI.
int main(int argc, char* argv[])
{
	const char* ver_str = "Palan compiler 0.2.0a";
	bool show_asm = false;
	bool do_asm = true;
	bool do_link = true;
	bool do_exec = true;
	bool rm_objs = true;

	string out_file = "a.out";
	vector<string> object_files;
	string cmd_dir = getDirName(argv[0]);

	po::options_description opt("Options");
	po::positional_options_description p_opt;
	po::variables_map vm;

	opt.add_options()
		("help,h", PlnMessage::getHelp(H_Help))
		("version,v", PlnMessage::getHelp(H_Version))
		("assembly,S", PlnMessage::getHelp(H_Assembly))
		("compile,c", PlnMessage::getHelp(H_Compile))
		("output,o", po::value<string>(), PlnMessage::getHelp(H_Output))
		("execute,x", PlnMessage::getHelp(H_Execute))
		("input-file", po::value<vector<string>>(), PlnMessage::getHelp(H_Input));

	p_opt.add("input-file", -1);
	
	try {
		po::store(po::command_line_parser(argc, argv)
				.options(opt).positional(p_opt).run(), vm);

	} catch (exception &e) {
		string what = e.what();
		what[0] = toupper(what[0]);
		cerr << what << endl;
		cerr << usage() << opt << note();
		return PARAM_ERR;
	}

	po::notify(vm);

	{
		bool help = vm.count("help");
		bool assembly = vm.count("assembly");
		bool compile = vm.count("compile");
		bool output = vm.count("output");
		bool execute = vm.count("execute");
		bool input_file = vm.count("input-file");

		// Process infomation.
		if (vm.count("version")) {
			cout << ver_str << endl;
			if (!(compile||output||execute||assembly||input_file||help))
				return 0;
		}

		if (help) {
			cout << usage() << opt << note();
			return 0;
		}

		// Validation
		if (!input_file) {
			cerr << PlnMessage::getErr(E_CUI_NoInputFile) << endl;
			cerr << usage() << opt << note();
			return PARAM_ERR;
		}

		if (compile && output || compile && assembly || output && assembly) {
			cerr << PlnMessage::getErr(E_CUI_IncompatibleOpt) << endl;
			cerr << usage() << opt << note();
			return PARAM_ERR;
		}

		if (execute && (compile || assembly)) {
			cerr << PlnMessage::getErr(E_CUI_InvalidExecOpt) << endl;
			return PARAM_ERR;
		}

		// Set up process flags.
		if (assembly) {
			show_asm = true;
			do_asm = false;
			do_link = false;
			do_exec = false;
			rm_objs = false;

		} else if (compile) {
			show_asm = false;
			do_asm = true;
			do_link = false;
			do_exec = false;
			rm_objs = false;

		} else if (output) {
			show_asm = false;
			do_asm = true;
			do_link = true;
			out_file = vm["output"].as<string>();
			do_exec = execute;
			rm_objs = false;
		}
		// else default (output & execute & out_file = "a.out")
	}

	vector<string> files(vm["input-file"].as< vector<string> >());

	for (string& fname: files) {
		if (getExtention(fname) == "o") continue;

		PlnModule *module;
		{
			FILE* outf;
			json j;

			// Get AST json from pat command.
			string cmd =  cmd_dir + "pat \"" + fname + "\"";
			outf = popen(cmd.c_str(), "r");
			if (outf) {
				popen_filebuf p_buf(outf);
				istream pat_output(&p_buf);
				if (pat_output.peek() != std::ios::traits_type::eof()) {
					try {
						j = json::parse(pat_output);

					} catch (json::exception& e) {
						cerr << e.what() << endl;
						BOOST_ASSERT(false);
					}
				}

				int ret = getStatus(pclose(outf));
				if (ret) return ret;

			} else {
				BOOST_ASSERT(false);
			}

			// Get source file infomation.
			vector<string> files;
			int fid = 0;
			for (auto finf: j["files"]) {
				BOOST_ASSERT(fid == finf["id"].get<int>());
				files.push_back(finf["name"].get<string>());
				fid++;
			}

			// Check parse errors.
			if (j["errs"].is_array()) {
				json &err = j["errs"][0];
				PlnLoc loc(err["loc"].get<vector<int>>());

				string error_msg = files[loc.fid] + ":" +to_string(loc.begin_line) + ": " + err["msg"].get<string>(); 
				cerr << error_msg << endl;
				return COMPILE_ERR;
			}

			FILE *as = NULL;	// as process
			try {
				// Build palan model tree from AST.
				PlnModelTreeBuilder modelTreeBuilder;
				module = modelTreeBuilder.buildModule(j["ast"]);
				// free json object memory.
				j.clear();

				if (show_asm) {
					PlnX86_64DataAllocator allocator;
					PlnX86_64Generator generator(cout);
					module->gen(allocator, generator);

				} else if (do_asm) {
					PlnX86_64DataAllocator allocator;
					string obj_file = getDirName(fname) + getFileName(fname) + ".o";
					string cmd = "as -o \"" + obj_file + "\"" ;

					as = popen(cmd.c_str(), "w");
					popen_filebuf p_buf(as);
					ostream as_input(&p_buf);

					PlnX86_64Generator generator(as_input);
					module->gen(allocator, generator);

					int ret = getStatus(pclose(as));
					if (ret) return ret;

					object_files.push_back(obj_file);
				} else {
					BOOST_ASSERT(false);
				}

			} catch (PlnCompileError &err) {
				if (as) pclose(as);
				cerr << files[err.loc.fid] << ":" << err.loc.begin_line << ": " << PlnMessage::getErr(err.err_code, err.arg1, err.arg2);
				cerr << endl;
				return COMPILE_ERR;
	
			} // catch (json::exception& e) {
			//	BOOST_ASSERT(false); // need to detect error before json error.
			// }
		}
	}

	if (do_link) {
		string flist = join(object_files, " ");
		if (!do_exec)
			cout << "linking: " << out_file << endl;
		string cmd = "ld --dynamic-linker /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 -o " + out_file + " -lc " + flist + " -lm";

		int ret = getStatus(system(cmd.c_str()));
		if (ret) return ret;
	}

	if (do_exec) {
		string cmd = "./" + out_file;
		int ret = getStatus(system(cmd.c_str()));
		if (rm_objs) {
			remove(object_files.front().c_str());
			remove(out_file.c_str());
		}
		return ret;
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

string usage()
{
	string usage_str = "Usage:\n";
	string usages[] = {
		"pac -h | --help",
		"pac -v | --version",
		"pac -S <input-file>",
		"pac -c <input-file>",
		"pac [-x] -o <output-file> <input-file>",
		"pac <input-file>",
	};

	for (auto usage: usages)
		usage_str = usage_str + "  " + usage + "\n";
	usage_str += "\n";

	return usage_str;
}

string note()
{
	string note = "  No option is the same as;\n"
		"    pac -x -o a.out <input-file.pa>\n"
		"    rm a.out input-file.o\n";
	
	return note;
}
