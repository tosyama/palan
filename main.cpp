#include "PlnParser.hpp"
#include "PlnLexer.h"
#include "PlnModel.h"
#include "PlnX86_64Generator.h"
#include "PlnMessage.h"
#include <boost/assign.hpp>
#include <boost/program_options.hpp>
#include <fstream>
#include <sstream>

using std::cout;
using std::cerr;
using std::endl;
using std::exception;
using std::ifstream;
using std::stringstream;
using namespace boost::assign;
using palan::PlnParser;

namespace po = boost::program_options;

void loadSystemCalls(PlnModule& module);

int main(int argc, char* argv[])
{
	po::options_description opt("Options");
	po::positional_options_description p_opt;
	po::variables_map vm;
	bool do_dump = false;
	bool do_asm = true;

	opt.add_options()
		("help,h", "Display this help")
		("dump,d", "Dump semantic tree")
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

	if (vm.count("help")) {
		cout << opt;
		return 0;
	}
	if (vm.count("dump")) {
		do_dump = true;
		do_asm = false;
	}

	if (vm.count("input-file")){
		vector<string> files(vm["input-file"].as< vector<string> >());
		for (string& fname: files) {
			ifstream	f;
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
			} else {
				string msg(PlnMessage::getErr(E_CouldnotOpenFile, fname));
				cerr << "error: " << msg << endl;
			}
		}
		return 0;
	} 

	// test code
	PlnLexer lexer;
	stringstream str(
		"void main()\n"
		"{\n"
		"	int a, b;\n"
		"	{int a, cc = 3,5;}\n"
		"	int c=4;\n"
		"	b = 13;\n"
		"	sys_write(1,\"Hello World!\\n\", b);\n"
		"}"
	);
	lexer.switch_streams(&str,&cout);
	lexer.set_filename("test.palan");

	PlnModule modu;
	loadSystemCalls(modu);

	PlnScopeStack	scopes;
	PlnParser parser(lexer, modu, scopes);
	int res = parser.parse();

	if (res) return res;	// parse error
	if (do_dump) modu.dump(cout);

	if (do_asm) {
		PlnX86_64Generator generator(cout);
		modu.gen(generator);
	}

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
