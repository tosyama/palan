/// x86-64 (Linux) data place management class definition.
///
/// @file	PlnX86_64DataAllocator.cpp
/// @copyright	2017-2019 YAMAGUCHI Toshinobu

#include <iostream>
#include <cstddef>
#include <boost/assert.hpp>
#include <limits.h>

#include "../PlnConstants.h"
#include "../models/PlnVariable.h"
#include "../models/PlnType.h"
#include "PlnX86_64DataAllocator.h"

using namespace std;
static const int ARG_TBL[] = { RDI, RSI, RDX, RCX, R8, R9 };
static const int FARG_TBL[] = { XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7 };
static const int DSTRY_TBL[] = { RAX, RDI, RSI, RDX, RCX, R8, R9, R10, R11 };
static const int SYSARG_TBL[] = { RDI, RSI, RDX, R10, R8, R9 };

PlnX86_64DataAllocator::PlnX86_64DataAllocator()
	: PlnDataAllocator(REG_NUM)
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
		}
	}
}

PlnDataPlace* PlnX86_64DataAllocator::createArgDp
	(int func_type, const vector<int> &ret_dtypes, const vector<int> &arg_dtypes, int index, bool is_callee)
{
	PlnDataPlace* dp = new PlnDataPlace(8, DT_UNKNOWN);
	int freg_ind = 0;
	for (int i=0; i<index; i++)
		if (arg_dtypes[i] == DT_FLOAT)
			freg_ind++;
	
	int reg_ind = index - freg_ind;

	if (arg_dtypes[index] == DT_FLOAT && freg_ind <= 7) {
		int regid;
		dp->type = DP_REG;
		dp->data.reg.id = FARG_TBL[freg_ind];
		dp->data.reg.offset = 0;

	} else if (arg_dtypes[index] != DT_FLOAT && reg_ind <= 5) {
		int regid;
		if (func_type == FT_PLN || func_type == FT_C)
			regid = ARG_TBL[reg_ind];
		else if (func_type == FT_SYS)
			regid = SYSARG_TBL[reg_ind];
		else
			BOOST_ASSERT(false);

		dp->type = DP_REG;
		dp->data.reg.id = regid;
		dp->data.reg.offset = 0;

	} else {	// index >= 5
		BOOST_ASSERT(func_type != FT_SYS);
		int ind = -1;
		if (reg_ind > 5)
			ind += reg_ind-5;

		if (freg_ind > 7)
			ind += freg_ind-7;

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

PlnDataPlace* PlnX86_64DataAllocator::createReturnDp
	(int func_type, const vector<int> &ret_dtypes, const vector<int> &arg_dtypes, int index, bool is_callee)
{
	PlnDataPlace* dp = new PlnDataPlace(8, DT_UNKNOWN);
	int freg_ind = 0;
	for (int i=0; i<index; i++)
		if (ret_dtypes[i] == DT_FLOAT)
			freg_ind++;

	int reg_ind = index - freg_ind;

	if (ret_dtypes[index] == DT_FLOAT && freg_ind <= 7) {
		int regid;
		dp->type = DP_REG;
		dp->data.reg.id = FARG_TBL[freg_ind];
		dp->data.reg.offset = 0;

	} else if (ret_dtypes[index] != DT_FLOAT && reg_ind <= 5) {
		int regid;
		if (func_type == FT_PLN || func_type == FT_C) {
			regid = (ret_dtypes.size() == 1) ? RAX : ARG_TBL[index];

		} else if (func_type == FT_SYS) {
			BOOST_ASSERT(ret_dtypes.size() == 1);
			regid = RAX;
		} else
			BOOST_ASSERT(false);

		dp->type = DP_REG;
		dp->data.reg.id = regid;
		dp->data.reg.offset = 0;
		
	} else {	// index >= 5
		BOOST_ASSERT(func_type != FT_SYS);
		int ind = -1;
		if (reg_ind > 5)
			ind += reg_ind-5;

		if (freg_ind > 7)
			ind += freg_ind-7;

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

static void checkExistsActiveDP(PlnDataPlace* root, PlnDataPlace* dp)
{
	PlnDataPlace *curdp = root;
	while (curdp != dp) {
		BOOST_ASSERT(curdp != NULL);
		BOOST_ASSERT(curdp->status == DS_RELEASED);
		curdp = curdp->previous;
	}
}

void PlnX86_64DataAllocator::funcCalled(vector<PlnDataPlace*>& args, int func_type)
{
	// TODO: use popSrc().
	for (auto dp: args) {
		dp->status = DS_RELEASED;
		dp->release_step = step;
		if (PlnDataPlace* pdp = dp->save_place) {
			pdp->status = DS_RELEASED;
			pdp->release_step = step;
		}

		// check invalid state.
		if (dp->type == DP_REG) 
			checkExistsActiveDP(regs[dp->data.reg.id], dp);
		else if (dp->type == DP_STK_SP)
			checkExistsActiveDP(arg_stack[dp->data.stack.idx], dp);
	}

	destroyRegsByFuncCall();
	step++;
}

void PlnX86_64DataAllocator::prepareMemCopyDps(PlnDataPlace* &dst, PlnDataPlace* &src, PlnDataPlace* &len)
{
	static string dcmt = "copy dst";
	static string scmt = "copy src";
	static string lcmt = "copy len";

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

	len = new PlnDataPlace(8, DT_UINT);
	len->type = DP_REG;
	len->status = DS_READY_ASSIGN;
	len->data.reg.id = RCX;
	len->data.reg.offset = 0;
	len->comment = &lcmt;
}

void PlnX86_64DataAllocator::memCopyed(PlnDataPlace* dst, PlnDataPlace* src, PlnDataPlace* len)
{
	// TODO: It destory RCX, RDI, RCI
	releaseDp(dst);
	releaseDp(src);
	releaseDp(len);
	step++;
}

PlnDataPlace* PlnX86_64DataAllocator::prepareAccumulator(int data_type)
{
	auto dp = new PlnDataPlace(8, data_type);
	dp->type = DP_REG;

	dp->status = DS_READY_ASSIGN;
	if (data_type == DT_FLOAT) {
		dp->data.reg.id = XMM0;
	} else {
		dp->data.reg.id = RAX;
	}
	dp->data.reg.offset = 0;

	static string cmt = "%accm";
	dp->comment = &cmt;

	return dp;
}

PlnDataPlace* PlnX86_64DataAllocator::added(PlnDataPlace* ldp, PlnDataPlace *rdp)
{
	BOOST_ASSERT(ldp->type == DP_REG && ldp->status == DS_ASSIGNED);
	BOOST_ASSERT((rdp->type != DP_SUBDP && rdp->status == DS_ASSIGNED)
		|| (rdp->type == DP_SUBDP && rdp->data.originalDp->status == DS_ASSIGNED));

	rdp->access(step);
	ldp->access(step);
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

	rdp->access(step);
	ldp->access(step);
	releaseDp(rdp);
	releaseDp(ldp);
	auto result = prepareAccumulator(ldp->data_type);
	allocDp(result);
	return result;
}

void PlnX86_64DataAllocator::divided(PlnDataPlace** quotient, PlnDataPlace** reminder, PlnDataPlace* ldp, PlnDataPlace* rdp)
{
	BOOST_ASSERT(ldp->type == DP_REG && ldp->status == DS_ASSIGNED);
	BOOST_ASSERT((rdp->type != DP_SUBDP && rdp->status == DS_ASSIGNED)
		|| (rdp->type == DP_SUBDP && rdp->data.originalDp->status == DS_ASSIGNED));
	rdp->access(step);
	releaseDp(rdp);

	if (ldp->data_type == DT_FLOAT) {
		ldp->access(step);
		releaseDp(ldp);
		*quotient = prepareAccumulator(ldp->data_type);
		*reminder = NULL;
		step++;
		return;
	}

	auto dp = new PlnDataPlace(8,ldp->data_type);
	dp->type = DP_REG;
	dp->data.reg.id = RDX;
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
	auto dp = new PlnDataPlace(8, DT_SINT);
	dp->type = DP_REG;
	dp->status = DS_READY_ASSIGN;

	dp->data.reg.id = RDI;
	dp->data.reg.offset = 0;

	static string cmt = "index";
	dp->comment = &cmt;

	return dp;
}

PlnDataPlace* PlnX86_64DataAllocator::prepareObjIndexPtr(int staticIndex)
{
	return getLiteralIntDp(staticIndex);
}

static int calcAccessScore(PlnDataPlace* dp) {
	int score = 0;
	while (dp) {
		if (dp->type == DP_BYTES) {
			for (auto bdp: *(dp->data.bytesData)) {
				if (bdp->data.bytes.offset == 0)
					score = bdp->access_count;
			}
		} else {
			score += dp->access_count;
		}
		dp = dp->previous;
	}
	return score;
}

static PlnDataPlace* divideBytesDps(PlnDataPlace* dp, int regid)
{
	vector<PlnDataPlace*> divDps;
	while (dp) {
		if (dp->type == DP_BYTES) {
			auto& bytesData = *dp->data.bytesData;
			auto bdpi = bytesData.begin();
			while(bdpi != bytesData.end()) {
				BOOST_ASSERT((*bdpi)->type == DP_STK_BP);
				BOOST_ASSERT((*bdpi)->type < 8);
				BOOST_ASSERT((*bdpi)->data.bytes.parent_dp == dp);
				if ((*bdpi)->data.bytes.offset != 0 || (*bdpi)->data_type == DT_FLOAT) {
					divDps.push_back(*bdpi);
					bdpi = bytesData.erase(bdpi);
				} else {
					(*bdpi)->type = DP_REG;
					(*bdpi)->data.reg.id = regid;
					(*bdpi)->data.reg.offset = 0;
					bdpi++;
				}
			}
		} else {
			dp->type = DP_REG;
			dp->data.reg.id = regid;
			dp->data.reg.offset = 0;
		}
		dp = dp->previous;
	}
	
	PlnDataPlace *bytesDps = NULL;
	if (divDps.size()) {
		bytesDps = new PlnDataPlace(8, DT_UNKNOWN);
		for (auto bdp: divDps)
			bdp->data.bytes.parent_dp = bytesDps;
		static string cmt = "bytes";
		bytesDps->type = DP_BYTES;
		bytesDps->data.bytesData = new vector<PlnDataPlace *>();
		(*bytesDps->data.bytesData) = move(divDps);
		bytesDps->comment = &cmt;
		bytesDps->updateBytesDpStatus();
	}
	return bytesDps;
}

void PlnX86_64DataAllocator::optimizeRegAlloc()
{
	static vector<int> no_save_regids = {RAX, RDI, RSI, RDX, RCX, R8, R9, R10};
	static vector<int> save_regids = {RBX, R12, R13, R14, R15};

	if (data_stack.size()) {
		vector<int> scores(data_stack.size());
		int max_score_index = 0;
		int max_score = 0;
		for (int i=0; i<data_stack.size(); i++) {
			int score = calcAccessScore(data_stack[i]);
			if (score > max_score) {
				max_score_index = i;
				max_score = score;
			}
			scores[i] = score;
		}

		// select register
		int regid = -1;
		for (int id: no_save_regids) {
			if (!regs[id]) {
				regid = id;
				break;
			}
		}

		if (regid == -1 && max_score >= 3) { // this case use the reg needs to save
			for (int id: save_regids) {
				if (!regs[id]) {
					regid = id;
					break;
				}
			}
		}

		if (regid == -1) return;

		int index = max_score_index;
		PlnDataPlace* dp = data_stack[index];

		if (!regs[regid]) {
			PlnDataPlace *bytesDps = divideBytesDps(dp, regid);
			regs[regid] = dp;
			if (bytesDps) {
				data_stack[index] = bytesDps;
			} else {
				data_stack.erase(data_stack.begin()+index);
			}
		}
		return;

		if (dp->type != DP_BYTES && dp->data_type != DT_FLOAT && dp->size == 8) {
			// pop from stack
			data_stack[index] = dp->previous;
			if (!data_stack[index]) {
				data_stack.erase(data_stack.begin()+index);
			}

			// push to reg
			dp->type = DP_REG;
			dp->data.reg.id = regid;
			dp->data.reg.offset = 0;

			dp->previous = regs[regid];
			regs[regid] = dp;
		}
	}
}

void PlnX86_64DataAllocator::checkDataLeak()
{
	PlnDataAllocator::checkDataLeak();
	for (int id: DSTRY_TBL) {
		if (regs[id]) {
			BOOST_ASSERT(regs[id]->status == DS_RELEASED);
		}
	}
}
