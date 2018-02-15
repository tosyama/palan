/// x86-64 (Linux) data allocation management class declaration.
///
/// @file	PlnX86_64DataAllocator.h
/// @copyright	2017-2018 YAMAGUCHI Toshinobu

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
	PlnDataPlace* createArgDp(int func_type, int index, bool is_callee) override;
	vector<int> getRegsNeedSave() override;

public:
	PlnX86_64DataAllocator();

	void funcCalled(vector<PlnDataPlace*>& args, vector<PlnVariable*>& rets, int func_type) override;
	void returnedValues(vector<PlnDataPlace*>& ret_dps, int func_type) override;

	void memAlloced() override;
	void memFreed() override;
	void prepareMemCopyDps(PlnDataPlace* &dst, PlnDataPlace* &src) override;
	void memCopyed(PlnDataPlace* dst, PlnDataPlace* src) override;

	PlnDataPlace* prepareAccumulator(int data_type) override;
	bool isAccumulator(PlnDataPlace* dp) override;
	PlnDataPlace* added(PlnDataPlace* ldp, PlnDataPlace *rdp) override;
	PlnDataPlace* multiplied(PlnDataPlace* ldp, PlnDataPlace* rdp) override;
	void divided(PlnDataPlace** quotient, PlnDataPlace** reminder, PlnDataPlace* ldp, PlnDataPlace* rdp) override;

	// for array item
	PlnDataPlace* prepareObjBasePtr() override;
	PlnDataPlace* prepareObjIndexPtr() override;
};

