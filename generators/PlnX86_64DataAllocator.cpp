/// x86-64 (Linux) data place management class definition.
///
/// @file	PlnX86_64DataAllocator.cpp
/// @copyright	2017 YAMAGUCHI Toshinobu

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
					allocSaveData(pdp, pdp->alloc_step, pdp->release_step);
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
	// TODO: use pupSrc().
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

void PlnX86_64DataAllocator::memAlloced()
{
	destroyRegsByFuncCall();
	step++;
}

void PlnX86_64DataAllocator::prepareMemCopyDps(PlnDataPlace* &dst, PlnDataPlace* &src)
{
	static string dcmt = "copy dst";
	static string scmt = "copy src";

	dst = new PlnDataPlace(8, DT_OBJECT_REF);
	dst->status = DS_READY_ASSIGN;
	dst->data.reg.id = RDI;
	dst->data.reg.offset = 0;
	dst->type = DP_REG;
	dst->comment = &dcmt;

	src = new PlnDataPlace(8, DT_OBJECT_REF);
	src->type = DP_REG;
	src->status = DS_READY_ASSIGN;
	src->data.reg.id = RSI;
	src->data.reg.offset = 0;
	src->comment = &scmt;
}

void PlnX86_64DataAllocator::memCopyed(PlnDataPlace* dst, PlnDataPlace* src)
{
	releaseDp(dst);
	releaseDp(src);

	PlnDataPlace* pdp = regs[RCX];
	PlnDataPlace* dp = new PlnDataPlace(8, DT_UNKNOWN);
	dp->type = DP_REG;
	dp->status = DS_RELEASED;
	dp->alloc_step = dp->release_step = step;
	dp->previous = pdp;
	regs[RCX] = dp;
	if (pdp && pdp->status != DS_RELEASED)
		if (!pdp->save_place)  {
			allocSaveData(pdp, pdp->alloc_step, pdp->release_step);
		}
	step++;
}

PlnDataPlace* PlnX86_64DataAllocator::prepareAccumulator(int data_type)
{
	auto dp = new PlnDataPlace(8, data_type);
	dp->type = DP_REG;

	dp->status = DS_READY_ASSIGN;
	dp->data.reg.id = RAX;
	dp->data.reg.offset = 0;

	static string cmt = "%accm";
	dp->comment = &cmt;

	return dp;
}

bool PlnX86_64DataAllocator::isAccumulator(PlnDataPlace* dp)
{
	return dp->type == DP_REG && dp->data.reg.id == RAX;
}

PlnDataPlace* PlnX86_64DataAllocator::added(PlnDataPlace* ldp, PlnDataPlace *rdp)
{
	BOOST_ASSERT(ldp->type == DP_REG && ldp->status == DS_ASSIGNED);
	BOOST_ASSERT((rdp->type != DP_SUBDP && rdp->status == DS_ASSIGNED)
		|| (rdp->type == DP_SUBDP && rdp->data.originalDp->status == DS_ASSIGNED));
	releaseDp(rdp);
	releaseDp(ldp);
	auto result = prepareAccumulator(ldp->data_type);
	allocDp(result);
	return result;
}

PlnDataPlace* PlnX86_64DataAllocator::multiplied(PlnDataPlace* ldp, PlnDataPlace* rdp)
{
	BOOST_ASSERT(ldp->type == DP_REG && ldp->status == DS_ASSIGNED);
	BOOST_ASSERT((rdp->type != DP_SUBDP && rdp->status == DS_ASSIGNED)
		|| (rdp->type == DP_SUBDP && rdp->data.originalDp->status == DS_ASSIGNED));
	releaseDp(rdp);
	return ldp;
	step++;
}

void PlnX86_64DataAllocator::divided(PlnDataPlace** quotient, PlnDataPlace** reminder, PlnDataPlace* ldp, PlnDataPlace* rdp)
{
	BOOST_ASSERT(ldp->type == DP_REG && ldp->status == DS_ASSIGNED);
	BOOST_ASSERT((rdp->type != DP_SUBDP && rdp->status == DS_ASSIGNED)
		|| (rdp->type == DP_SUBDP && rdp->data.originalDp->status == DS_ASSIGNED));
	releaseDp(rdp);

	int regid = RDX;
	auto pdp = regs[regid];
	auto dp = new PlnDataPlace(8,ldp->data_type);
	dp->type = DP_REG;
	dp->data.reg.id = regid;
	allocDp(dp, false);

	*quotient = ldp;
	*reminder = dp;
	static string cmt = "%divr";
	(*reminder)->comment = &cmt;

	step++;
}

PlnDataPlace* PlnX86_64DataAllocator::prepareObjBasePtr()
{
	auto dp = new PlnDataPlace(8, DT_OBJECT_REF);
	dp->type = DP_REG;
	dp->status = DS_READY_ASSIGN;

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
	dp->status = DS_READY_ASSIGN;

	dp->data.reg.id = RDI;
	dp->data.reg.offset = 0;

	static string cmt = "index";
	dp->comment = &cmt;

	return dp;
}
