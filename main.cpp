#include "PlnParser.hpp"
#include "PlnLexer.h"
#include "PlnModel.h"
#include "PlnX86_64Generator.h"
#include <boost/assign.hpp>
#include <sstream>

using std::cout;
using std::stringstream;
using namespace boost::assign;
using palan::PlnParser;

void loadSystemCalls(PlnModule& module);

int main()
{
	PlnLexer lexer;
	stringstream str(
		"void main()\n"
		"{\n"
		"	sys_write(1,\"Hello World!\\n\", 13);\n"
		"	sys_exit(0);\n"
		"}"
	);
	lexer.switch_streams(&str,&cout);
	lexer.set_filename("test.palan");

	PlnModule modu;
	loadSystemCalls(modu);

	PlnParser parser(lexer, modu);
	parser.parse();

	modu.dump(cout);

	PlnX86_64Generator generator(cout);
	modu.gen(generator);

	return 0;
}

void loadSystemCall(PlnModule& module,
	const char *fname, int id,
	vector<PlnVarType>& pt, vector<const char*>& pn)
{	
	PlnFunction* f = new PlnFunction(fname);
	f->type = FT_SYS;
	f->inf.syscall.id = id;
	for (int i=0; i<pt.size(); ++i) {
		PlnVariable* p = new PlnVariable();
		p->type = pt[i];
		p->name = pn[i];
		f->addParam(*p);
	}
	
	module.addFunc(*f);
}

void loadSystemCalls(PlnModule& module)
{
	const char *fname;
	int id;
	vector<PlnVarType> pt;
	vector<const char*> pn;
	
	// void exit(int status);
	fname = "sys_exit";
	id = 60;
	pt += VT_INT8;
	pn += "error_code";
	loadSystemCall(module, fname, id, pt, pn);

	pt.resize(0);	
	pn.resize(0);	

	fname = "sys_write";
	id = 1;
	pt += VT_UINT8, VT_OBJ, VT_UINT8;
	pn += "fd", "buf", "count";
	loadSystemCall(module, fname, id, pt, pn);
}
