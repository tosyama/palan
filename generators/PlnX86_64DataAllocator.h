/// x86-64 (Linux) data allocation management class declaration.
///
/// @file	PlnX86_64DataAllocator.h
/// @copyright	2017- YAMAGUCHI Toshinobu

#include "../PlnDataAllocator.h"

enum {
	PU_ARG
};

enum {
	RAX, RBX, RCX, RDX,
	RDI, RSI, RBP, RSP,
	R8, R9, R10, R11,
	R12, R13, R14, R15
};

class PlnX86_64DataAllocator: public PlnDataAllocator
{
	vector<PlnDataPlace*> place_regs[16];
	PlnDataPlace* save_regs[16];
	vector<PlnDataPlace*> tmp_stack;
	vector<vector<PlnDataPlace*>> arg_stack;
	vector<PlnDataPlace*> alldp;

	void pushReg(int reg, PlnDataPlace* dp);
	void pushArgStack(int index, PlnDataPlace* dp);
	void assignAnother(PlnDataPlace* dp);

public:
	void reset();
	void refine();

	void reserveReg4Var(PlnDataPlace *place);
	void releaseReg4Var(PlnDataPlace *place);
	void push2Work(PlnDataPlace* place);
	void pop2Work(PlnDataPlace* place);

	void pushParam(int index);
	void popParam(int index, PlnDataPlace* place);

	void pushArgDp(int index, PlnDataPlace* place);
	PlnDataPlace* popArgDp(int index);

	void pushSysArgDp(int index, PlnDataPlace* place);
	PlnDataPlace* popSysArgDp(int index);

	void funcCalled();
};

