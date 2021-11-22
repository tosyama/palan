/// x86-64 (Linux) data allocation management class declaration.
///
/// @file	PlnX86_64DataAllocator.h
/// @copyright	2017-2020 YAMAGUCHI Toshinobu

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
	RIP, REG_NUM,
};

enum {	// for custom_inf
	IS_RETVAL = 1
};

class PlnX86_64DataAllocator: public PlnDataAllocator
{
	void destroyRegsByFuncCall();
	bool tryMoveDp2Reg(PlnDataPlace* dp, int regid);

protected:
	void setArgDps(int func_type, vector<PlnDataPlace*> &arg_dps, bool is_callee);
	void setRetValDps(int func_type, vector<PlnDataPlace*> &retval_dps, bool is_callee);

public:
	PlnX86_64DataAllocator();

	void funcCalled(vector<PlnDataPlace*>& args, int func_type, bool never_return) override;

	void prepareMemCopyDps(PlnDataPlace* &dst, PlnDataPlace* &src, PlnDataPlace* &len) override;
	void memCopyed(PlnDataPlace* dst, PlnDataPlace* src, PlnDataPlace* len) override;

	PlnDataPlace* prepareAccumulator(int data_type, int data_size) override;
	PlnDataPlace* added(PlnDataPlace* ldp, PlnDataPlace *rdp) override;
	PlnDataPlace* multiplied(PlnDataPlace* ldp, PlnDataPlace* rdp) override;
	PlnDataPlace* divided(PlnDataPlace* ldp, PlnDataPlace* rdp, bool is_mod) override;

	// for array item
	PlnDataPlace* prepareObjBasePtr() override;
	PlnDataPlace* prepareObjIndexPtr() override;
	PlnDataPlace* prepareObjIndexPtr(int staticIndex) override;

	// for optimization
	void optimizeRegAlloc() override;

	// for debug
	void checkDataLeak() override;
};

