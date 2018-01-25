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

using namespace std;

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

	all.insert(all.end(),sub_dbs.begin(),sub_dbs.end());
	sub_dbs.resize(0);

	all.insert(all.end(),regs.begin(),regs.end());
	for (auto& reg: regs) reg = NULL;

	stack_size = 0;
	step = 0;
}

// return dp == null -> alloc top
// else can alloc between dp and previous.
static bool canAlloc(PlnDataPlace*& dp, int alloc_step, int release_step)
{
	if (alloc_step > dp->release_step) {
		dp = NULL;
		return true;
	}
	while (dp->previous) {
		if (dp->alloc_step > release_step
			&& alloc_step > dp->previous->release_step) {
			return true;
		} else if (dp->alloc_step <= release_step
			|| alloc_step >=  dp->previous->release_step) {
			return false;
		}
		dp = dp->previous;
	}

	if (dp->alloc_step > release_step)
		return true;
	
	return false;
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
			auto aft_dp = data_stack[i]; 
			if (canAlloc(aft_dp, alloc_step, release_step)) {
				dp->data.stack.idx = i;
				if (aft_dp) {
					dp->previous = aft_dp->previous;
					aft_dp->previous = dp;
				} else {
					dp->previous = data_stack[i];
					data_stack[i] = dp;
				}
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

void PlnDataAllocator::allocSaveData(PlnDataPlace* dp, int alloc_step, int release_step)
{
	BOOST_ASSERT(dp->save_place == NULL);
	PlnDataPlace *save_dp = new PlnDataPlace(dp->size, dp->data_type);
	allocDataWithDetail(save_dp, alloc_step, release_step);
	dp->save_place = save_dp;
	static string cmt = "(save)";
	dp->save_place->comment = &cmt;
}

void PlnDataAllocator::releasedBytesDpChiled(PlnDataPlace* dp)
{
	auto parent_dp = data_stack[dp->data.stack.idx];
	BOOST_ASSERT(parent_dp->type == DP_BYTES);
	BOOST_ASSERT(parent_dp->status == DS_ASSIGNED_SOME);
	if (parent_dp->allocable_size() == parent_dp->size) {
		parent_dp->status = DS_RELEASED;
		parent_dp->release_step = step;
	}
}

void PlnDataAllocator::releaseData(PlnDataPlace* dp)
{
	BOOST_ASSERT(dp->status != DS_RELEASED);
	dp->status = DS_RELEASED;
	dp->release_step = step;
	if (dp->save_place) {
		dp->save_place->status = DS_RELEASED;
		dp->save_place->release_step = step;
	}

	if (dp->size < 8 && dp->type == DP_STK_BP) {
		releasedBytesDpChiled(dp);
	}

	step++;
}

void PlnDataAllocator::allocDp(PlnDataPlace *dp, bool proceed_step)
{
	if (dp->type == DP_REG) {
		int regid = dp->data.reg.id;
		dp->previous = regs[regid];
		regs[regid] = dp;

	} else if (dp->type == DP_STK_SP) {
		int idx = dp->data.stack.idx;
		while (arg_stack.size() <= idx) 
			arg_stack.push_back(NULL);
		dp->previous = arg_stack[idx];
		arg_stack[idx] = dp;

	} else if (dp->type == DP_STK_BP) {
		if (dp->data.stack.offset >= 16) {
			// argument: TODO: undepend ABI
			all.push_back(dp);
		} else {
			allocDataWithDetail(dp, step, INT_MAX);
		}

	} else
		BOOST_ASSERT(false);

	dp->status = DS_ASSIGNED;
	auto pdp = dp->previous;
	dp->alloc_step = step;
	if (pdp && pdp->status != DS_RELEASED) {
		allocSaveData(pdp, pdp->alloc_step, pdp->release_step);
	}

	if (proceed_step) step++;
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
		dp->status = DS_READY_ASSIGN;
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

void PlnDataAllocator::setIndirectObjDp(PlnDataPlace* dp, PlnDataPlace* base_dp, PlnDataPlace* index_dp)
{
	dp->type = DP_INDRCT_OBJ;
	dp->status = DS_ASSIGNED;

	dp->data.indirect.displacement = 0;
	dp->data.indirect.base_dp = base_dp;
	dp->data.indirect.index_dp = index_dp;
	dp->data.indirect.base_id = base_dp->data.reg.id;
	dp->data.indirect.index_id = index_dp->data.reg.id;

	dp->alloc_step = step;
	dp->release_step = step;
	all.push_back(dp);
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

PlnDataPlace* PlnDataAllocator::getSeparatedDp(PlnDataPlace* dp)
{
	BOOST_ASSERT(dp->type != DP_SUBDP);

	// indirect obj is already separated and not managed by data allocator.
	if (dp->type == DP_INDRCT_OBJ)
		return dp;

	auto sub_dp = new PlnDataPlace(dp->size, dp->data_type);
	sub_dp->type = DP_SUBDP;
	sub_dp->data.originalDp = dp;
	sub_dp->comment = dp->comment;

	sub_dbs.push_back(sub_dp);
	return sub_dp;
}

void PlnDataAllocator::finish(vector<int> &save_regs, vector<PlnDataPlace*> &save_reg_dps)
{
	// Alloc register value save area.
	save_regs = getRegsNeedSave();
	for (int sr: save_regs) {
		auto dp = new PlnDataPlace(8,DT_UINT);
		dp->type = DP_STK_BP;
		dp->data.stack.idx = data_stack.size();
		dp->previous = NULL;
		data_stack.push_back(dp);
		save_reg_dps.push_back(dp);
	}

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

	// rewrite original data to sub.
	for (auto sub_dp: sub_dbs) {
		auto org_dp = sub_dp->data.originalDp;
		sub_dp->type = org_dp->type;
		sub_dp->data = org_dp->data;
	}
}

void PlnDataAllocator::pushSrc(PlnDataPlace* dp, PlnDataPlace* src_dp,  bool release_src_pop)
{
	BOOST_ASSERT(dp->src_place == NULL);
	dp->src_place = src_dp;
	dp->push_src_step = step;
	dp->release_src_pop = release_src_pop;
	step++;
}

bool PlnDataAllocator::isDestroyed(PlnDataPlace* dp)
{
	if (dp->type == DP_SUBDP)
		dp = dp->data.originalDp;
	switch (dp->type) {
		case DP_REG:
		{
			int regid = dp->data.reg.id;
			return regs[regid] != dp;
		}
		case DP_STK_SP:
		{
			int ind = dp->data.stack.idx;
			return arg_stack[ind] != dp;
		}
		case DP_INDRCT_OBJ:
		{
			auto base_dp = dp->data.indirect.base_dp;
			int base_id = base_dp->data.reg.id;
			auto index_dp = dp->data.indirect.index_dp;
			int index_id = index_dp->data.reg.id;
			return regs[base_id] != base_dp || regs[index_id] != index_dp;
		}
		case DP_RO_DATA:
		case DP_LIT_INT:
		case DP_BYTES:
		case DP_STK_BP:
			return false;
		default: // not implemented.
		BOOST_ASSERT(false);
	}
}

bool tryAccelerateAlloc(PlnDataPlace *dp, int al_step)
{
	if (dp->type == DP_REG) {
		if (dp->status == DS_ASSIGNED || dp->status == DS_RELEASED) {
			if (dp->alloc_step <= al_step) return true;
			if (!dp->previous || dp->previous->release_step < al_step) {
				dp->alloc_step = al_step;
				return true;
			}
		} else
			BOOST_ASSERT(false);
	}
	return false;
}

void PlnDataAllocator::popSrc(PlnDataPlace* dp)
{
	BOOST_ASSERT(dp->src_place);
	auto src_place = dp->src_place;
	bool is_src_destroyed = isDestroyed(src_place);

	// Release source if flag on.
	if (dp->release_src_pop) {
		src_place->release_step = step;
		src_place->status = DS_RELEASED;
	}

	// Assign dst data place if ready.
	if (dp->status == DS_READY_ASSIGN) {
		allocDp(dp, false);
	}
	// check src data would be destory.
	if (!dp->save_place) {
		if (is_src_destroyed) {
			if (tryAccelerateAlloc(dp, dp->push_src_step))
				dp->save_place = dp;
			else
				allocSaveData(dp, src_place->alloc_step, step);
		}
	}
	
	// Updated save & src place status;
	if (dp->save_place) {
		if (dp->release_src_pop) {
			src_place->release_step = dp->push_src_step;
		}
		if (dp->save_place != dp) {
			dp->save_place->alloc_step = dp->push_src_step;
			dp->save_place->release_step = step;
			dp->save_place->status = DS_RELEASED;
		} 
	} 

	step++;
}

// PlnDataPlace
PlnDataPlace::PlnDataPlace(int size, int data_type)
	: type(DP_UNKNOWN), status(DS_UNKNOWN), accessCount(0), alloc_step(0), release_step(INT_MAX),
	 previous(NULL), save_place(NULL), src_place(NULL),
	 size(size), data_type(data_type)
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

