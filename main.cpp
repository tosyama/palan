#include "PlnParser.hpp"
#include "PlnLexer.h"
#include "PlnModel.h"
#include "PlnX86_64Generator.h"
#include "PlnMessage.h"
#include <boost/assign.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string/join.hpp>
#include <fstream>
#include <sstream>
#ifdef __GNUC__
	#include <ext/stdio_sync_filebuf.h>    
	typedef __gnu_cxx::stdio_sync_filebuf<char> popen_filebuf;
#endif

using std::cout;
using std::cerr;
using std::endl;
using std::exception;
using std::ifstream;
using std::stringstream;
using namespace boost::assign;
using boost::algorithm::join;
using palan::PlnParser;

namespace po = boost::program_options;

void loadSystemCalls(PlnModule& module);
static string getFileName(string& fpath);
static string getExtention(string& fpath);

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
		do_link = true;
		out_file = vm["output"].as<string>();
	}

	if (vm.count("dump")) {
		do_dump = true;
		do_asm = false;
		do_compile = false;
		do_link = false;

	} else if (vm.count("compile")) {
		do_dump = false;
		do_asm = false;
		do_compile = true;
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
				loadSystemCalls(module);

				PlnScopeStack	scopes;
				PlnParser parser(lexer, module, scopes);
				int res = parser.parse();

				if (res) return res;	// parse error

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
		string cmd = "ld -o " + out_file + " " + flist;
		int ret = system(cmd.c_str());
		if (ret) return ret;
	}

	if (vm.count("input-file")) return 0;

	return 0;
}

void loadSystemCall(PlnModule& module,
	const char *fname, int id,
	vector<string>& pt, vector<const char*>& pn)
{	
	PlnFunction* f = new PlnFunction(FT_SYS, fname);
	f->type = FT_SYS;
	f->inf.syscall.id = id;
	for (int i=0; i<pt.size(); ++i) {
		PlnParameter* p = new PlnParameter();
		p->var_type = module.getType(pt[i]);
		p->name = pn[i];
		f->addParam(*p);
	}
	
	module.functions.push_back(f);
}

void loadSystemCalls(PlnModule& module)
{
	const char *fname;
	int id;
	vector<string> pt;
	vector<const char*> pn;
	
	// void exit(int status);
	fname = "sys_exit";
	id = 60;
	pt += "int";
	pn += "error_code";
	loadSystemCall(module, fname, id, pt, pn);

	pt.resize(0);	
	pn.resize(0);	

	fname = "sys_write";
	id = 1;
	pt += "int", "object", "int";
	pn += "fd", "buf", "count";
	loadSystemCall(module, fname, id, pt, pn);
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

