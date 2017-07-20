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
		"	sys_write(1,\"Hello World!\\n\", 14);\n"
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

	return 0;
	
	// int main()
	// {
	//	sys_write(1, "Hello, World!\n", 14);
	//	return 0;
	// }
	PlnReadOnlyData slit;
	slit.type = RO_LIT_STR;
	slit.name = "Hello, World!\n";
	modu.addReadOnlyData(slit);
	
	PlnFunction f1("main");
	f1.type = FT_PLN;
	PlnBlock b;
	
	PlnStatement s1;
	PlnFunctionCall fc1;
	fc1.function = modu.getFunc("sys_write");
	PlnExpression warg1;
	warg1.type = ET_VALUE;
	warg1.value.type = VL_LIT_INT8;
	warg1.value.inf.intValue = 1;
	fc1.addArgument(warg1);

	PlnExpression warg2;
	warg2.type = ET_VALUE;
	warg2.value.type = VL_RO_DATA;
	warg2.value.inf.rod = &slit;
	fc1.addArgument(warg2);

	PlnExpression warg3;
	warg3.type = ET_VALUE;
	warg3.value.type = VL_LIT_INT8;
	warg3.value.inf.intValue = 14;
	fc1.addArgument(warg3);
	
	s1.type = ST_EXPRSN;
	s1.inf.expression = &fc1;
	b.addStatement(s1);

	PlnStatement s;
	PlnFunctionCall fc;
	fc.function = modu.getFunc("sys_exit");

	PlnExpression ev;
	ev.type = ET_VALUE;
	ev.value.type = VL_LIT_INT8;
	ev.value.inf.intValue = 0;
	fc.addArgument(ev);

	s.type = ST_EXPRSN;
	s.inf.expression = &fc;
	b.addStatement(s);

	f1.implement = &b; 
	modu.addFunc(f1);

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
