/// x86-64 (Linux) data allocation management class declaration.
///
/// @file	PlnX86_64DataAllocator.h
/// @copyright	2017- YAMAGUCHI Toshinobu

#include "../PlnDataAllocator.h"

enum {
	RAX, RBX, RCX, RDX,
	RDI, RSI, RBP, RSP,
	R8, R9, R10, R11,
	R12, R13, R14, R15
};

class PlnX86_64DataAllocator: public PlnDataAllocator
{
protected:
	PlnDataPlace* createArgDp(int func_type, int index, bool is_callee);

public:
	PlnX86_64DataAllocator();

	void funcCalled(vector<PlnDataPlace*>& args, vector<PlnVariable*>& rets, int func_type);
	void returnedValues(vector<PlnDataPlace*>& ret_dps, int func_type);

	PlnDataPlace* allocAccumulator(PlnDataPlace* dp);
	void releaseAccumulator(PlnDataPlace* dp);
	bool isAccumulator(PlnDataPlace* dp);
};

