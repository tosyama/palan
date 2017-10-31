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

	int stack_size=data_stack.size();

	if (size == 8) {
		for (int i=0; i<stack_size; ++i) {
			if (data_stack[i]->status == DS_RELEASED) {
				dp->data.stack.idx = i;
				dp->previous = data_stack[i];
				data_stack[i] = dp;
				return;
			}
		}

		dp->data.stack.idx = stack_size;
		dp->previous = NULL;
		data_stack.push_back(dp);
		return;
	}

	// size < 8
	int free_index = -1;
	for (int i=0; i<stack_size; ++i) {
		if (free_index < 0 && data_stack[i]->status == DS_RELEASED) 
			free_index = i;
			
		else if (data_stack[i]->status == DS_ASSIGNED_SOME
				&& data_stack[i]->tryAllocBytes(dp)) {
			dp->data.bytes.idx = i;
			return;
		}
	}

	// Create new DP_BYTES data place.
	auto dp_ctnr = new PlnDataPlace(8, DT_UNKNOWN);
	dp_ctnr->type = DP_BYTES;
	dp_ctnr->data.bytesData = new vector<PlnDataPlace *>();
	dp_ctnr->status = DS_ASSIGNED_SOME;
	dp_ctnr->alloc_step = alloc_step;
	dp_ctnr->release_step = release_step;
	dp_ctnr->data.bytesData->push_back(dp);
	dp->data.bytes.offset = 0;

	if (free_index >= 0) {
		dp->data.bytes.idx = free_index;
		dp_ctnr->previous = data_stack[free_index];
		data_stack[free_index] = dp_ctnr;
	} else {
		dp->data.bytes.idx = stack_size;
		dp_ctnr->previous = NULL;
		data_stack.push_back(dp_ctnr);
	}
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

	if (dp->size < 8) {
		auto parent_dp = data_stack[dp->data.stack.idx];
		BOOST_ASSERT(parent_dp->type == DP_BYTES);
		BOOST_ASSERT(parent_dp->status == DS_ASSIGNED_SOME);
		if (parent_dp->allocable_size() == parent_dp->size) {
			parent_dp->status = DS_RELEASED;
			parent_dp->release_step = step;
		}
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

PlnDataPlace* PlnDataAllocator::getLiteralIntDp(int64_t intValue)
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
	// Set offset from base stack pointer.
	for (auto dp: data_stack) {
		offset -= 8;
		for (auto dpp=dp; dpp; dpp = dpp->previous) {
			if (dpp->type == DP_BYTES) {
				for (auto child: *dpp->data.bytesData) 
					child->data.stack.offset = offset + child->data.bytes.offset;
			} else
				dpp->data.stack.offset = offset;
		}
	}

	// Set offset from stack pointer.
	offset = 0;
	for (auto dp: arg_stack) {
		for (auto dpp = dp; dpp; dpp = dpp->previous)
			dpp->data.stack.offset = offset;
		offset += 8;
	}

	// Set total stack size.
	int stk_itm_num = data_stack.size() + arg_stack.size();
	if (stk_itm_num & 0x1) stk_itm_num++;  // for 16byte align.
	stack_size = stk_itm_num * 8;
}

// PlnDataPlace
PlnDataPlace::PlnDataPlace(int size, int data_type)
	: type(DP_UNKNOWN), status(DS_ASSIGNED), accessCount(0), alloc_step(0), release_step(INT_MAX),
	 previous(NULL), save_place(NULL), size(size), data_type(data_type)
{
	static string emp="";
	comment = &emp;
}

unsigned int PlnDataPlace::getAllocBytesBits()
{
	// return 8bit flg. e.g) 0000 0100: a byte of offset 2 byte is alloced.
	BOOST_ASSERT(type == DP_BYTES);
	unsigned int alloc_bytes = 0;
	for (auto child: (*data.bytesData)) 
		if (child->status != DS_RELEASED) 
			switch (child->size) {
				case 1:	
					alloc_bytes |= 0x1 << child->data.bytes.offset; break;
				case 2:
					alloc_bytes |= 0x3 << child->data.bytes.offset; break;
				case 4:
					alloc_bytes |= 0xf << child->data.bytes.offset; break;
			}

	return alloc_bytes;
}

bool PlnDataPlace::tryAllocBytes(PlnDataPlace* dp)
{
	BOOST_ASSERT(type == DP_BYTES);
	BOOST_ASSERT(dp->size <= 4);

	int dp_size  = dp->size;
	unsigned int alloc_bytes = getAllocBytesBits();
	unsigned int flg;

	switch (dp_size) {
		case 4: flg = 0xf; break;
		case 2: flg = 0x3; break;
		case 1: flg = 0x1; break;
	}

	for (int i=0; i<size; i+=dp_size) {
		if ((alloc_bytes & (flg << i)) == 0) {
			dp->data.bytes.offset = i;
			data.bytesData->push_back(dp);
			return true;
		}
	}

	return false;
}

int PlnDataPlace::allocable_size()
{
	if (status == DS_RELEASED) return size;
	if (type != DP_BYTES) return 0;

	// DP_BYTES
	unsigned int alloc_bytes = getAllocBytesBits();
	if (alloc_bytes == 0) return 8;
	if ((alloc_bytes & 0x0F) == 0 || (alloc_bytes & 0xF0) == 0)
		return 4;

	unsigned flg = 0x3;
	for (int i=0; i<4; ++i) {
		if ((alloc_bytes & flg) == 0) return 2;
		flg <<= 2;
	}
	return 1;
}

