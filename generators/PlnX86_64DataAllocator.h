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
	void destroyRegsByFuncCall();

protected:
	PlnDataPlace* createArgDp(int func_type, int index, bool is_callee);
	vector<int> getRegsNeedSave();

public:
	PlnX86_64DataAllocator();

	void funcCalled(vector<PlnDataPlace*>& args, vector<PlnVariable*>& rets, int func_type);
	void returnedValues(vector<PlnDataPlace*>& ret_dps, int func_type);

	void memAlloced();
	void memFreed();
	void prepareMemCopyDps(PlnDataPlace* &dst, PlnDataPlace* &src);
	void memCopyed(PlnDataPlace* dst, PlnDataPlace* src);

	PlnDataPlace* prepareAccumulator(int data_type); 
	bool isAccumulator(PlnDataPlace* dp);
	PlnDataPlace* added(PlnDataPlace* ldp, PlnDataPlace *rdp);
	PlnDataPlace* multiplied(PlnDataPlace* tgt);
	void divided(PlnDataPlace** quotient, PlnDataPlace** reminder);

	// for array item
	PlnDataPlace* prepareObjBasePtr();
	PlnDataPlace* prepareObjIndexPtr();
};

