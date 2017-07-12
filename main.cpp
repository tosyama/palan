#include "PlnModel.h"
#include "PlnX86_64Generator.h"

using std::cout;

int main()
{
	PlnModule modu;

	// void exit(int status);
	PlnFunction ext("sys_exit");
	ext.type = FT_SYS;
	ext.inf.syscall.id = 60;
	
	PlnVariable ext_p1;
	ext_p1.type = VT_INT;
	ext_p1.name = "status";
	ext.addParam(ext_p1);
	
	modu.addFunc(ext);

	// int main()
	// {return 0; }
	PlnFunction f1("main");
	f1.type = FT_PLN;
	PlnBlock b;
	PlnStatement s;
	PlnFunctionCall fc;
	fc.function = &ext;

	PlnExpression ev;
	ev.type = ET_VALUE;
	ev.value.type = VL_LIT_INT;
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
