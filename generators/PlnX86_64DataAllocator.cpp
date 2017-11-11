/// x86-64 (Linux) data place management class definition.
///
/// @file	PlnX86_64DataAllocator.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu

#include <iostream>
#include <cstddef>
#include <boost/assert.hpp>
#include <limits.h>

#include "../PlnConstants.h"
#include "../models/PlnVariable.h"
#include "../models/PlnType.h"
#include "PlnX86_64DataAllocator.h"

using namespace std;
static const int ARG_TBL[] = { RAX, RDI, RSI, RDX, RCX, R8, R9 };
static const int DSTRY_TBL[] = { RAX, RDI, RSI, RDX, RCX, R8, R9, R10, R11 };
static const int SYSARG_TBL[] = { RAX, RDI, RSI, RDX, R10, R8, R9 };

PlnX86_64DataAllocator::PlnX86_64DataAllocator()
	: PlnDataAllocator(16)
{
}

void PlnX86_64DataAllocator::destroyRegsByFuncCall()
{
	for (int regid: DSTRY_TBL) {
		PlnDataPlace* pdp = regs[regid];
		if (!pdp || (pdp->release_step != step)) {
			PlnDataPlace* dp = new PlnDataPlace(8, DT_UNKNOWN);
			dp->type = DP_REG;
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
}

PlnDataPlace* PlnX86_64DataAllocator::createArgDp(int func_type, int index, bool is_callee)
{
	PlnDataPlace* dp = new PlnDataPlace(8, DT_UNKNOWN);

	if (index <= 6) {
		int regid;
		if (func_type == FT_PLN || func_type == FT_C)
			regid = ARG_TBL[index];
		else if (func_type == FT_SYS)
			regid = SYSARG_TBL[index];
		else
			BOOST_ASSERT(false);

		dp->type = DP_REG;
		dp->data.reg.id = regid;
		dp->data.reg.offset = 0;
	} else {	// index >= 6
		BOOST_ASSERT(func_type != FT_SYS);
		int ind = index-7;

		if (is_callee) {
			dp->type = DP_STK_BP;
			dp->data.stack.idx = ind;
			dp->data.stack.offset = ind*8+16;

		} else {
			dp->type = DP_STK_SP;

			dp->data.stack.idx = ind;
			dp->data.stack.offset = ind*8;
		}
	}
	dp->status = DS_ASSIGNED;

	return dp;
}

vector<int> PlnX86_64DataAllocator::getRegsNeedSave()
{
	vector<int> save_regs;
	static int regs4save[] = {RBX, R12, R13, R14, R15};
	for (int r: regs4save)
		if (regs[r])
			save_regs.push_back(r);
	return save_regs;
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

	destroyRegsByFuncCall();
	step++;
}

void PlnX86_64DataAllocator::returnedValues(vector<PlnDataPlace*>& ret_dps, int func_type)
{
	if (ret_dps.size() >= 7)
		allocSaveData(ret_dps[0]);	// for use RAX for store return data to stack

	for (auto dp: ret_dps) {
		dp->status = DS_RELEASED;
		dp->release_step = step;

		// check invalid state.
		if (dp->type == DP_REG) 
			BOOST_ASSERT(!checkExistsActiveDP(regs[dp->data.reg.id], dp));
	}

	step++;
}

void PlnX86_64DataAllocator::memAlloced()
{
	destroyRegsByFuncCall();
	step++;
}

void PlnX86_64DataAllocator::memFreed()
{
	destroyRegsByFuncCall();
	step++;
}

void PlnX86_64DataAllocator::prepareMemCopyDps(PlnDataPlace* &dst, PlnDataPlace* &src)
{
	static string dcmt = "copy dst";
	static string scmt = "copy src";

	dst = new PlnDataPlace(8, DT_OBJECT_REF);
	dst->status = DS_ASSIGNED;
	dst->data.reg.id = RDI;
	dst->data.reg.offset = 0;
	dst->type = DP_REG;
	dst->comment = &dcmt;

	src = new PlnDataPlace(8, DT_OBJECT_REF);
	src->type = DP_REG;
	src->status = DS_ASSIGNED;
	src->data.reg.id = RSI;
	src->data.reg.offset = 0;
	src->comment = &scmt;
}

void PlnX86_64DataAllocator::memCopyed(PlnDataPlace* dst, PlnDataPlace* src)
{
	releaseData(dst);
	releaseData(src);

	PlnDataPlace* pdp = regs[RCX];
	PlnDataPlace* dp = new PlnDataPlace(8, DT_UNKNOWN);
	dp->type = DP_REG;
	dp->status = DS_RELEASED;
	dp->alloc_step = dp->release_step = step;
	dp->previous = pdp;
	regs[RCX] = dp;
	if (pdp && pdp->status != DS_RELEASED)
		if (!pdp->save_place)  {
			allocSaveData(pdp);
		}
	step++;
}

PlnDataPlace* PlnX86_64DataAllocator::allocAccumulator(PlnDataPlace* new_dp)
{
	auto dp = new_dp ? new_dp : new PlnDataPlace(8, DT_SINT);
	auto pdp = regs[RAX];
	dp->type = DP_REG;
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

bool PlnX86_64DataAllocator::isAccumulator(PlnDataPlace* dp)
{
	return dp->type == DP_REG && dp->data.reg.id == RAX;
}

PlnDataPlace* PlnX86_64DataAllocator::multiplied(PlnDataPlace* tgt)
{
	BOOST_ASSERT(tgt->type == DP_REG);
	auto regid = tgt->data.reg.id;
	auto pdp = regs[regid];
	BOOST_ASSERT(!pdp || pdp->status == DS_RELEASED);
	auto dp = new PlnDataPlace(8,tgt->data_type);
	dp->type = DP_REG;
	dp->status = DS_RELEASED;
	dp->data.reg.id = regid;
	dp->alloc_step = dp->release_step = step;
	dp->previous = pdp;
	regs[regid] = dp;
	step++;
	return dp;
}

void PlnX86_64DataAllocator::divided(PlnDataPlace** quotient, PlnDataPlace** reminder)
{
	BOOST_ASSERT(regs[RAX]->status==DS_RELEASED);

	for (auto regid: {RAX, RDX}) {
		auto pdp = regs[regid];
		auto dp = new PlnDataPlace(8,DT_SINT);
		// TODO: reconsider DT_SINT/DT_UINT
		dp->type = DP_REG;
		dp->status = DS_RELEASED;
		dp->data.reg.id = regid;
		dp->alloc_step = dp->release_step = step;
		dp->previous = pdp;
		regs[regid] = dp;
		if (pdp && pdp->status != DS_RELEASED)
			if (!pdp->save_place)  {
				allocSaveData(pdp);
			}
	}
	*quotient = regs[RAX];
	*reminder = regs[RDX];

	step++;
}

PlnDataPlace* PlnX86_64DataAllocator::prepareObjBasePtr()
{
	auto dp = new PlnDataPlace(8, DT_OBJECT_REF);
	dp->type = DP_REG;
	dp->status = DS_ASSIGNED;

	dp->data.reg.id = RBX;
	dp->data.reg.offset = 0;

	static string cmt = "base";
	dp->comment = &cmt;

	return dp;
}

PlnDataPlace* PlnX86_64DataAllocator::prepareObjIndexPtr()
{
	auto dp = new PlnDataPlace(8, DT_OBJECT_REF);
	dp->type = DP_REG;
	dp->status = DS_ASSIGNED;

	dp->data.reg.id = RDI;
	dp->data.reg.offset = 0;

	static string cmt = "index";
	dp->comment = &cmt;

	return dp;
}
