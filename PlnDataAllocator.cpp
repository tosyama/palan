/// Register/Stack allocation class definition.
///
/// @file	PlnDataAllocator.cpp
/// @copyright	2017 YAMAGUCHI Toshinobu

#include <iostream>
#include <stdlib.h>
#include <limits.h>
#include <boost/assert.hpp>

#include "PlnDataAllocator.h"
#include "PlnConstants.h"

PlnDataAllocator::PlnDataAllocator(int regnum)
	: regnum(regnum)
{
	regs.resize(regnum);
	reset();
}

void PlnDataAllocator::reset()
{
	all.insert(all.end(),data_stack.begin(),data_stack.end());
	data_stack.resize(0);
	all.insert(all.end(),arg_stack.begin(),arg_stack.end());
	arg_stack.resize(0);
	all.insert(all.end(),regs.begin(),regs.end());
	for (auto& reg: regs) reg = NULL;
	stack_size = 0;
	step = 0;
}

void PlnDataAllocator::allocDataWithDetail(PlnDataPlace* dp, int alloc_step, int release_step)
{
	dp->type = DP_STK_BP;
	dp->status = DS_ASSIGNED;
	dp->alloc_step = alloc_step;
	dp->release_step = release_step;
	int size = dp->size;

	if (size < 8) {
		// TODO search alloc some bytes place first.
	//	BOOST_ASSERT(false);
	}

	int s=data_stack.size();
	for (int i=0; i<s; i++) {
		PlnDataPlace *pdp = data_stack[i];
		if (pdp->status == DS_RELEASED) {
			dp->data.stack.idx = i;
			data_stack[i] = dp;
			dp->previous = pdp;
			return;
		}
	}

	dp->data.stack.idx = data_stack.size();
	data_stack.push_back(dp);
}

PlnDataPlace* PlnDataAllocator::allocData(int size, int data_type)
{
	PlnDataPlace* new_dp = new PlnDataPlace(size, data_type);
	allocData(new_dp);
	return new_dp;
}

void PlnDataAllocator::allocData(PlnDataPlace *new_dp)
{
	allocDataWithDetail(new_dp, step++, INT_MAX);
}

void PlnDataAllocator::allocSaveData(PlnDataPlace* dp)
{
	BOOST_ASSERT(dp->save_place == NULL);
	PlnDataPlace *save_dp = new PlnDataPlace(dp->size, dp->data_type);
	allocDataWithDetail(save_dp, dp->alloc_step, dp->release_step);
	dp->save_place = save_dp;
	static string cmt = "(save)";
	dp->save_place->comment = &cmt;
}

void PlnDataAllocator::releaseData(PlnDataPlace* dp)
{
	dp->status = DS_RELEASED;
	dp->release_step = step;
	if (dp->save_place) {
		dp->save_place->status = DS_RELEASED;
		dp->save_place->release_step = step;
	}

	step++;
}

void PlnDataAllocator::allocDp(PlnDataPlace *dp)
{
	PlnDataPlace *pdp;
	if (dp->type == DP_REG) {
		int regid = dp->data.reg.id;
		pdp = regs[regid];
		regs[regid] = dp;
	} else if (dp->type == DP_STK_SP) {
		int idx = dp->data.stack.idx;
		while (arg_stack.size() <= idx) 
			arg_stack.push_back(NULL);
		pdp = arg_stack[idx];
		arg_stack[idx] = dp;
	} else if (dp->type == DP_STK_BP
			&& dp->data.stack.offset >= 16) {
		pdp = NULL;
		all.push_back(dp);
	} else
		BOOST_ASSERT(false);

	dp->previous = pdp;
	dp->alloc_step = step++;
	if (pdp && pdp->status != DS_RELEASED)
		allocSaveData(pdp);
}

vector<PlnDataPlace*> PlnDataAllocator::prepareArgDps(int ret_num, int arg_num, int func_type, bool is_callee)
{
	int param_ind = ret_num > 0 ? ret_num : 1;
	int end_ind = param_ind + arg_num;

	vector<PlnDataPlace*> dps;
	for (int i=param_ind; i<end_ind; ++i) {
		auto dp = createArgDp(func_type, i, is_callee);
		static string cmt="arg";
		dp->comment = &cmt;
		dps.push_back(dp);
	}

	return dps;
}

vector<PlnDataPlace*> PlnDataAllocator::prepareRetValDps(int ret_num, int func_type, bool is_callee)
{
	vector<PlnDataPlace*> dps;

	for (int i=0; i<ret_num; ++i) {
		static string cmt="return";
		auto dp = createArgDp(func_type, i, is_callee);
		dp->comment = &cmt;
		dps.push_back(dp);
	}

	return dps;
}

PlnDataPlace* PlnDataAllocator::getLiteralIntDp(int intValue)
{
	PlnDataPlace* dp = new PlnDataPlace(8, DT_SINT);
	dp->type = DP_LIT_INT;
	dp->status = DS_ASSIGNED;
	dp->data.intValue = intValue;
	dp->alloc_step = step;
	dp->release_step = step;
	static string cmt = "$";
	dp->comment = &cmt;
	all.push_back(dp);
	return dp;
}

PlnDataPlace* PlnDataAllocator::getReadOnlyDp(int index)
{
	PlnDataPlace* dp = new PlnDataPlace(8, DT_OBJECT_REF);
	dp->type = DP_RO_DATA;
	dp->status = DS_ASSIGNED;
	dp->data.index = index;
	dp->alloc_step = step;
	dp->release_step = step;
	static string cmt = "\"..\"";
	dp->comment = &cmt;
	all.push_back(dp);
	return dp;
}

void PlnDataAllocator::finish()
{
	int offset = 0;
	for (auto dp: data_stack) {
		offset -= 8;
		dp->data.stack.offset = offset;
		PlnDataPlace* dpp = dp->previous;
		while (dpp) {
			dpp->data.stack.offset = offset;
			dpp = dpp->previous;
		}
	}
	offset = 0;
	for (auto dp: arg_stack) {
		PlnDataPlace* dpp = dp;
		while (dpp) {
			dpp->data.stack.offset = offset;
			dpp = dpp->previous;
		}
		offset += 8;
	}
	int stk_size = data_stack.size() + arg_stack.size();
	if (stk_size % 2) stk_size++;  // for 16byte align.
	stack_size = stk_size * 8;
}

PlnDataPlace::PlnDataPlace(int size, int data_type)
	: status(DS_ASSIGNED), accessCount(0), alloc_step(0), release_step(INT_MAX),
	 previous(NULL), save_place(NULL), size(size), data_type(data_type)
{
	static string emp="";
	comment = &emp;
}

int PlnDataPlace::allocable_size()
{
	if (type != DP_BYTES) {
		if (status == DS_RELEASED) return size;
		else return 0;
	}

	int max_size = 0;
	for (auto child: (*data.bytesData)) {
		int s = child->allocable_size();
		if (max_size < s) size = s; 
	}
	return max_size;
}

