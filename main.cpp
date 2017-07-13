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
	ext_p1.name = "error_code";
	ext.addParam(ext_p1);
	
	modu.addFunc(ext);
	
	PlnFunction wrt("sys_write");
	wrt.type = FT_SYS;
	wrt.inf.syscall.id = 1;
	PlnVariable wrt_p1;
	wrt_p1.type = VT_UINT;
	wrt_p1.name = "fd";
	wrt.addParam(wrt_p1);
	
	PlnVariable wrt_p2;
	wrt_p2.type = VT_OBJ;
	wrt_p2.name = "buf";
	wrt.addParam(wrt_p2);

	PlnVariable wrt_p3;
	wrt_p3.type = VT_UINT;
	wrt_p3.name = "count";
	wrt.addParam(wrt_p3);

	modu.addFunc(wrt);

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
	fc1.function = &wrt;
	PlnExpression warg1;
	warg1.type = ET_VALUE;
	warg1.value.type = VL_LIT_INT;
	warg1.value.inf.intValue = 1;
	fc1.addArgument(warg1);

	PlnExpression warg2;
	warg2.type = ET_VALUE;
	warg2.value.type = VL_RO_DATA;
	warg2.value.inf.rod = &slit;
	fc1.addArgument(warg2);

	PlnExpression warg3;
	warg3.type = ET_VALUE;
	warg3.value.type = VL_LIT_INT;
	warg3.value.inf.intValue = 14;
	fc1.addArgument(warg3);
	
	s1.type = ST_EXPRSN;
	s1.inf.expression = &fc1;
	b.addStatement(s1);

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
