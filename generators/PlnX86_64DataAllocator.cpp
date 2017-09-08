/// x86-64 (Linux) data place management class definition.
///
/// @file	PlnX86_64DataAllocator.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu
#include <iostream>
#include <cstddef>
#include <boost/assert.hpp>
#include <limits.h>
#include "PlnX86_64DataAllocator.h"

using namespace std;
static const int ARG_TBL[] = { RDI, RSI, RDX, RCX, R8, R9 };
static const int DSTRY_TBL[] = { RAX, RDI, RSI, RDX, RCX, R8, R9, R10, R11 };
static const int SYSARG_TBL[] = { RDI, RSI, RDX, R10, R8, R9 };

PlnX86_64DataAllocator::PlnX86_64DataAllocator()
	: PlnDataAllocator(16)
{
}

vector<PlnDataPlace*> PlnX86_64DataAllocator::allocArgs(
	vector<PlnParameter*>& params, vector<PlnVariable*>& rets, int func_type)
{
	int param_ind;
	vector<PlnDataPlace*> dps;

	if (rets.size() == 0) param_ind = 0;
	else param_ind = rets.size()-1;

	for (auto par: params) {
		PlnDataPlace* dp = new PlnDataPlace();

		if (param_ind < 6) {
			int regid;
			if (func_type == DPF_PLN || func_type == DPF_C)
				regid = ARG_TBL[param_ind];
			else if (func_type == DPF_SYS)
				regid = SYSARG_TBL[param_ind];
			else
				BOOST_ASSERT(false);
			
			PlnDataPlace* pdp = regs[regid];
			dp->type = DP_REG;
			dp->size = 8;	// TODO get from type.
			dp->status = DS_ARGUMENT;

			dp->data.reg.id = regid;
			dp->data.reg.offset = 0;

			dp->accessCount = 0;
			dp->previous = pdp;
			dp->alloc_step = step++;
	 		dp->release_step = INT_MAX;
			dp->save_place = NULL;
			
			regs[regid] = dp;

			if (pdp && pdp->status != DS_RELEASED)
				allocSaveData(pdp);

			dps.push_back(dp);
		} else {	// param_ind >= 6
			BOOST_ASSERT(func_type != DPF_SYS);
			int ind = param_ind-6;
			while (arg_stack.size() <= ind) 
				arg_stack.push_back(NULL);
			PlnDataPlace* pdp = arg_stack[ind];

			dp->type = DP_STK_SP;
			dp->size = 8;	// TODO get from type.
			dp->status = DS_ARGUMENT;

			dp->data.stack.idx = ind;
			dp->data.stack.offset = ind*8;

			dp->accessCount = 0;
			dp->previous = pdp;
			dp->alloc_step = step++;
	 		dp->release_step = INT_MAX;
			dp->save_place = NULL;

			arg_stack[ind] = dp;

			if (pdp && pdp->status != DS_RELEASED)
				allocSaveData(pdp);

			dps.push_back(dp);
		}
		param_ind++;
	}
	return dps;
}

static bool checkExistsActiveDP(PlnDataPlace* root, PlnDataPlace* dp)
{
	PlnDataPlace *curdp = root;
	while (curdp != dp) {
		BOOST_ASSERT(curdp != NULL);
		if (curdp->status != DS_DESTROYED && curdp->status != DS_RELEASED)
			return true;
		curdp = curdp->previous;
	}
	return false;
}

void PlnX86_64DataAllocator::funcCalled(
	vector<PlnDataPlace*>& args, vector<PlnVariable*>& rets, int func_type)
{
	for (auto dp: args) {
		dp->status = DS_RELEASED;
		dp->release_step = step;
		if (PlnDataPlace* pdp = dp->save_place) {
			pdp->status = DS_RELEASED;
			pdp->release_step = step;
		}

		// check invalid state.
		if (dp->type == DP_REG) 
			BOOST_ASSERT(!checkExistsActiveDP(regs[dp->data.reg.id], dp));
		else if (dp->type == DP_STK_SP)
			BOOST_ASSERT(!checkExistsActiveDP(arg_stack[dp->data.stack.idx], dp));
	}
	for (int regid: DSTRY_TBL) {
		PlnDataPlace* pdp = regs[regid];
		if (!pdp || (pdp->release_step != step)) {
			PlnDataPlace* dp = new PlnDataPlace();
			dp->type = DP_REG;
			dp->size = 8;
			dp->status = DS_DESTROYED;
			dp->alloc_step = dp->release_step = step;
			dp->previous = pdp;
			regs[regid] = dp;
			if (pdp && pdp->status != DS_RELEASED && pdp->status != DS_DESTROYED)
				if (!pdp->save_place) 
					allocSaveData(pdp);
		}
	}
	step++;
}

