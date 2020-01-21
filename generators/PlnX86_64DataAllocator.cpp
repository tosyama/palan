/// x86-64 (Linux) data place management class definition.
///
/// @file	PlnX86_64DataAllocator.cpp
/// @copyright	2017-2020 YAMAGUCHI Toshinobu

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
static const int FDSTRY_TBL[] = { XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7,
									XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15};
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
	for (int regid: FDSTRY_TBL) {
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
			int scores[8] = {0};
			for (auto bdp: *(dp->data.bytesData)) {
				if (!bdp->need_address) {
					BOOST_ASSERT(bdp->data.bytes.offset<8);
					scores[bdp->data.bytes.offset] += bdp->access_score;
				}
			}
			int max_score = 0;
			for (int s: scores) {
				if (s > max_score)
					max_score = s;
			}
			score += max_score;

		} else if (!dp->need_address) {
			score += dp->access_score;
		}
		dp = dp->previous;
	}
	return score;
}

static PlnDataPlace* divideBytesDps(PlnDataPlace* &root_dp, int regid)
{
	vector<PlnDataPlace*> divDps;
	PlnDataPlace* dp = root_dp;
	PlnDataPlace* pdp = NULL;
	while (dp) {
		if (dp->type == DP_BYTES) {
			vector<PlnDataPlace*> divBytesDps;
			auto& bytesData = *dp->data.bytesData;
			auto bdpi = bytesData.begin();

			// get offset to reg alloc.
			int offset = 0;
			{
				int scores[8] = {0};
				for (auto bdp: bytesData) {
					if (!bdp->need_address) {
						BOOST_ASSERT(bdp->data.bytes.offset<8);
						scores[bdp->data.bytes.offset] += bdp->access_score;
					}
				}
				int max_score = 0;
				for (int i=0; i<8; i++) {
					if (scores[i] > max_score) {
						max_score = scores[i];
						offset = i;
					}
				}
			}

			while(bdpi != bytesData.end()) {
				BOOST_ASSERT((*bdpi)->type == DP_STK_BP);
				BOOST_ASSERT((*bdpi)->type < 8);
				BOOST_ASSERT((*bdpi)->data.bytes.parent_dp == dp);
				if ((*bdpi)->need_address) {
					divBytesDps.push_back(*bdpi);
					bdpi = bytesData.erase(bdpi);
				} else if ((*bdpi)->data.bytes.offset != offset) {
					divBytesDps.push_back(*bdpi);
					bdpi = bytesData.erase(bdpi);
				} else {
					(*bdpi)->type = DP_REG;
					(*bdpi)->data.reg.id = regid;
					(*bdpi)->data.reg.offset = 0;
					bdpi++;
				}
			}
			if (divBytesDps.size()) {
				PlnDataPlace* bytesDps = new PlnDataPlace(8, DT_UNKNOWN);
				static string cmt = "bytes";
				bytesDps->type = DP_BYTES;
				bytesDps->data.bytesData = new vector<PlnDataPlace *>();
				for (auto bdp: divBytesDps) {
					bool success = bytesDps->tryAllocBytes(bdp);
					BOOST_ASSERT(success);
				}
				// (*bytesDps->data.bytesData) = move(divBytesDps);
				bytesDps->comment = &cmt;
				// bytesDps->updateBytesDpStatus();
				divDps.push_back(bytesDps);
			}
		} else if (dp->need_address) {
			if (pdp) pdp->previous = dp->previous;
			else root_dp = dp->previous;
			divDps.push_back(dp);

		} else {
			dp->type = DP_REG;
			dp->data.reg.id = regid;
			dp->data.reg.offset = 0;
			pdp = dp;
		}
		dp = dp->previous;
	}

	if (divDps.size()) {
		for (int i=0; i<divDps.size()-1; ++i)
			divDps[i]->previous = divDps[i+1];
		divDps.back()->previous = NULL;
		return divDps[0];
	} else
		return NULL;
}

void PlnX86_64DataAllocator::optimizeRegAlloc()
{
	static vector<int> no_save_regids = {RAX, RDI, RSI, RDX, RCX, R8, R9, R10};
	static vector<int> save_regids = {RBX, R12, R13, R14, R15};

	vector<int> scores(data_stack.size());
	for (int i=0; i<data_stack.size(); i++) {
		scores[i] = calcAccessScore(data_stack[i]);
	}

	while (data_stack.size()) {
		int max_score_index = 0;
		int max_score = 0;
		for (int i=0; i<data_stack.size(); i++) {
			int score = scores[i];
			if (score > max_score) {
				max_score_index = i;
				max_score = score;
			}
		}

		// select register
		int regid = -1;
		for (int id: no_save_regids) {
			if (!regs[id]) {
				regid = id;
				break;
			}
		}

		if (regid == -1 && max_score > 25) { // this case it can use the reg needs to save
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
				scores[index] = calcAccessScore(bytesDps);
				data_stack[index] = bytesDps;

			} else {
				scores.erase(scores.begin()+index);
				data_stack.erase(data_stack.begin()+index);
			}
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
