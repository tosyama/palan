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
public:
	PlnX86_64DataAllocator();
	vector<PlnDataPlace*> allocArgs(vector<PlnParameter*>& params, vector<PlnVariable*>& rets);
	void funcCalled(vector<PlnDataPlace*>& args, vector<PlnVariable*>& rets);
};


