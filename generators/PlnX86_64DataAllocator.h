/// x86-64 (Linux) data allocation management class declaration.
///
/// @file	PlnX86_64DataAllocator.h
/// @copyright	2017-2018 YAMAGUCHI Toshinobu

#include "../PlnDataAllocator.h"

enum {
	RAX, RBX, RCX, RDX,
	RDI, RSI, RBP, RSP,
	R8, R9, R10, R11,
	R12, R13, R14, R15,
	XMM0, XMM1, XMM2, XMM3,
	XMM4, XMM5, XMM6, XMM7,
	XMM8, XMM9, XMM10, XMM11,
	XMM12, XMM13, XMM14, XMM15,
	REG_NUM
};

class PlnX86_64DataAllocator: public PlnDataAllocator
{
	void destroyRegsByFuncCall();

protected:
	PlnDataPlace* createArgDp(int func_type, const vector<int> &ret_dtypes, const vector<int> &arg_dtypes, int index, bool is_callee) override;
	PlnDataPlace* createReturnDp(int func_type, const vector<int> &ret_dtypes, const vector<int> &arg_dtypes, int index, bool is_callee) override;
	vector<int> getRegsNeedSave() override;

public:
	PlnX86_64DataAllocator();

	void funcCalled(vector<PlnDataPlace*>& args, vector<PlnVariable*>& rets, int func_type) override;

	void prepareMemCopyDps(PlnDataPlace* &dst, PlnDataPlace* &src, PlnDataPlace* &len) override;
	void memCopyed(PlnDataPlace* dst, PlnDataPlace* src, PlnDataPlace* len) override;

	PlnDataPlace* prepareAccumulator(int data_type) override;
	PlnDataPlace* added(PlnDataPlace* ldp, PlnDataPlace *rdp) override;
	PlnDataPlace* multiplied(PlnDataPlace* ldp, PlnDataPlace* rdp) override;
	void divided(PlnDataPlace** quotient, PlnDataPlace** reminder, PlnDataPlace* ldp, PlnDataPlace* rdp) override;

	// for array item
	PlnDataPlace* prepareObjBasePtr() override;
	PlnDataPlace* prepareObjIndexPtr() override;
	PlnDataPlace* prepareObjIndexPtr(int staticIndex) override;

	// for debug
	void checkDataLeak() override;
};

