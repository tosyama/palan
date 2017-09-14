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
static const int ARG_TBL[] = { RAX, RDI, RSI, RDX, RCX, R8, R9 };
static const int DSTRY_TBL[] = { RAX, RDI, RSI, RDX, RCX, R8, R9, R10, R11 };
static const int SYSARG_TBL[] = { RAX, RDI, RSI, RDX, R10, R8, R9 };

PlnX86_64DataAllocator::PlnX86_64DataAllocator()
	: PlnDataAllocator(16)
{
}

vector<PlnDataPlace*> PlnX86_64DataAllocator::prepareArgDps(
	int argnum, 
	vector<PlnParameter*>& params, vector<PlnVariable*>& rets, int func_type)
{
	int param_ind;
	vector<PlnDataPlace*> dps;

	if (rets.size() == 0) param_ind = 1;
	else param_ind = rets.size();

	for (int i=0; i<argnum; ++i) {
		PlnDataPlace* dp = new PlnDataPlace();
		static string cmt="arg";

		if (param_ind <= 6) {
			int regid;
			if (func_type == DPF_PLN || func_type == DPF_C)
				regid = ARG_TBL[param_ind];
			else if (func_type == DPF_SYS)
				regid = SYSARG_TBL[param_ind];
			else
				BOOST_ASSERT(false);

			dp->type = DP_REG;
			dp->data.reg.id = regid;
			dp->data.reg.offset = 0;
		} else {	// param_ind >= 6
			BOOST_ASSERT(func_type != DPF_SYS);
			int ind = param_ind-7;
			dp->type = DP_STK_SP;

			dp->data.stack.idx = ind;
			dp->data.stack.offset = ind*8;
		}
		dp->size = 8;	// TODO get from type.
		dp->status = DS_ASSIGNED;
		dp->comment = &cmt;
			
		dps.push_back(dp);
		param_ind++;
	}
	return dps;
}

static bool checkExistsActiveDP(PlnDataPlace* root, PlnDataPlace* dp)
{
	PlnDataPlace *curdp = root;
	while (curdp != dp) {
		BOOST_ASSERT(curdp != NULL);
		if (curdp->status != DS_RELEASED)
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
			dp->status = DS_RELEASED;
			dp->alloc_step = dp->release_step = step;
			dp->previous = pdp;
			regs[regid] = dp;
			if (pdp && pdp->status != DS_RELEASED)
				if (!pdp->save_place)  {
					allocSaveData(pdp);
				}
		}
	}
	step++;
}

vector<PlnDataPlace*> PlnX86_64DataAllocator::allocReturnValues(vector<PlnVariable*>& rets, int func_type)
{
	vector<PlnDataPlace*> dps;
	int i = 0;
	for (auto r: rets) {
		PlnDataPlace* dp = new PlnDataPlace();
		if (i<=6) {
			int regid;
			if (func_type == DPF_PLN || func_type == DPF_C)
				regid = ARG_TBL[i];
			else if (func_type == DPF_SYS)
				regid = SYSARG_TBL[i];
			else
				BOOST_ASSERT(false);

			PlnDataPlace* pdp = regs[regid];
			dp->type = DP_REG;
			dp->size = 8;	// TODO get from type.
			dp->status = DS_ASSIGNED;

			dp->data.reg.id = regid;
			dp->data.reg.offset = 0;

			dp->previous = pdp;
			dp->alloc_step = step++;

			static string cmt="return";
			dp->comment = &cmt;
			
			regs[regid] = dp;

			if (pdp && pdp->status != DS_RELEASED) {
				allocSaveData(pdp);
			}

			dps.push_back(dp);
		} else {
			// not impl yet.
			BOOST_ASSERT(false);
		}
		i++;
	}
	return dps;
}

void PlnX86_64DataAllocator::returnedValues(vector<PlnDataPlace*>& ret_dps, int func_type)
{
	for (auto dp: ret_dps) {
		dp->status = DS_RELEASED;
		dp->release_step = step;

		// check invalid state.
		if (dp->type == DP_REG) 
			BOOST_ASSERT(!checkExistsActiveDP(regs[dp->data.reg.id], dp));
	}
	step++;
}

PlnDataPlace* PlnX86_64DataAllocator::allocAccumulator(PlnDataPlace* new_dp)
{
	auto dp = new_dp ? new_dp : new PlnDataPlace();
	auto pdp = regs[RAX];
	dp->type = DP_REG;
	dp->size = 8;
	dp->status = DS_ASSIGNED;

	dp->data.reg.id = RAX;
	dp->data.reg.offset = 0;

	dp->previous = pdp;
	dp->alloc_step = step++;

	static string cmt = "%work";
	dp->comment = &cmt;

	if (pdp && pdp->status != DS_RELEASED)
		if (!pdp->save_place) 
			allocSaveData(pdp);

	regs[RAX] = dp;

	return dp;
}

void PlnX86_64DataAllocator::releaseAccumulator(PlnDataPlace* dp)
{
	releaseData(dp);
}

