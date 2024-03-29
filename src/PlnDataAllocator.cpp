/// Register/Stack allocation class definition.
///
/// @file	PlnDataAllocator.cpp
/// @copyright	2017-2021 YAMAGUCHI Toshinobu

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
	all.insert(all.end(), data_stack.begin(), data_stack.end());
	data_stack.resize(0);

	all.insert(all.end(), arg_stack.begin(), arg_stack.end());
	arg_stack.resize(0);

	all.insert(all.end(), sub_dbs.begin(), sub_dbs.end());
	sub_dbs.resize(0);

	all.insert(all.end(), regs.begin(), regs.end());
	for (auto& reg: regs) reg = NULL;

	for (auto dp: all) delete dp;
	all.resize(0);

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

	// over spec code. no case to match now.
	// while (dp->previous) {
	// 	if (dp->alloc_step > release_step
	// 		&& alloc_step > dp->previous->release_step) {
	// 		return true;
	// 	} else if (dp->alloc_step <= release_step
	// 		|| alloc_step >=  dp->previous->release_step) {
	// 		return false;
	// 	}
	// 	dp = dp->previous;
	// }

	// if (dp->alloc_step > release_step)
	// 	return true;
	
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

				BOOST_ASSERT(!aft_dp);
				dp->previous = data_stack[i];
				data_stack[i] = dp;
				return;
			}
		}

		dp->data.stack.idx = stack_size;
		dp->data.stack.children = NULL;
		dp->previous = NULL;
		data_stack.push_back(dp);
		return;

	} else if (size > 8) {
		vector<PlnDataPlace *> *children = new vector<PlnDataPlace *>();
		int num = (size+7) / 8;
		for (int i=0; i<num; i++) {
			PlnDataPlace* rsv_dp = new PlnDataPlace(8, DT_OBJECT);
			rsv_dp->type = DP_STK_RESERVE_BP;
			rsv_dp->size = 8;
			rsv_dp->data.originalDp = dp;
			rsv_dp->status = DS_ASSIGNED;
			rsv_dp->alloc_step = alloc_step;
			rsv_dp->release_step = release_step;

			children->push_back(rsv_dp);
			data_stack.push_back(rsv_dp);
		}

		dp->data.stack.children = children;
		data_stack.push_back(dp);
		return;
	}

	// size < 8
	int free_index = -1;
	PlnDataPlace* aft_dp = NULL; 

	for (int i=0; i<stack_size; ++i) {
		PlnDataPlace* p_aft_dp = data_stack[i]; 
		if (free_index < 0 && canAlloc(p_aft_dp, alloc_step, release_step)) {
			free_index = i;
			aft_dp = p_aft_dp;
		} else if (data_stack[i]->type == DP_BYTES
				&& data_stack[i]->tryAllocBytes(dp)) {
			dp->data.bytes.idx = i;
			return;
		}
	}

	// Create new DP_BYTES data place.
	auto dp_ctnr = new PlnDataPlace(8, DT_UNKNOWN);
	static string cmt = "bytes";
	dp_ctnr->type = DP_BYTES;
	dp_ctnr->data.bytesData = new vector<PlnDataPlace *>();
	dp_ctnr->comment = &cmt;
	dp_ctnr->data.bytesData->push_back(dp);
	dp_ctnr->updateBytesDpStatus();
	dp->data.bytes.offset = 0;
	dp->data.bytes.parent_dp = dp_ctnr;

	if (free_index >= 0) {
		dp->data.bytes.idx = free_index;
		BOOST_ASSERT(!aft_dp);
		dp_ctnr->previous = data_stack[free_index];
		data_stack[free_index] = dp_ctnr;

	} else {
		dp->data.bytes.idx = stack_size;
		dp_ctnr->previous = NULL;
		data_stack.push_back(dp_ctnr);
	}
}

PlnDataPlace* PlnDataAllocator::prepareLocalVar(int size, int data_type)
{
	PlnDataPlace* new_dp = new PlnDataPlace(size, data_type);
	new_dp->type = DP_STK_BP;
	new_dp->data.stack.offset = 0;
	new_dp->data.stack.children = NULL;
	new_dp->status = DS_READY_ASSIGN;

	return new_dp;
}

PlnDataPlace* PlnDataAllocator::prepareGlobalVar(const string& name, int size, int data_type)
{
	PlnDataPlace* new_dp = new PlnDataPlace(size, data_type);
	new_dp->type = DP_GLBL;
	new_dp->data.varName = new string(name);
	new_dp->status = DS_ASSIGNED;

	return new_dp;
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
	dp->alloc_step = step;
	if (proceed_step) step++;
}

void PlnDataAllocator::releaseDp(PlnDataPlace* dp)
{
	BOOST_ASSERT(dp->status != DS_RELEASED);
	dp->status = DS_RELEASED;
	dp->release_step = step;
	if (dp->save_place) {
		dp->save_place->status = DS_RELEASED;
		dp->save_place->release_step = step;
	}

	if (dp->size > 8 && dp->type == DP_STK_BP) {
		BOOST_ASSERT(dp->data.stack.children);
		for (PlnDataPlace *cdp: *dp->data.stack.children) {
			cdp->status = DS_RELEASED;
			cdp->release_step = step;
		}

	} else if (dp->size < 8 && dp->type == DP_STK_BP && dp->data.bytes.parent_dp) {
		dp->data.bytes.parent_dp->updateBytesDpStatus();

	} else if (dp->type == DP_INDRCT_OBJ) {
		if (auto bdp = dp->data.indirect.base_dp) {
			releaseDp(bdp);
		}
		if (auto idp = dp->data.indirect.index_dp) {
			releaseDp(idp);
		}
	}

	step++;
}

int PlnDataAllocator::setIndirectObjDp(PlnDataPlace* dp, PlnDataPlace* base_dp, PlnDataPlace* index_dp, int displacement)
{
	BOOST_ASSERT(dp->size < 65536);
	int need_to_mul = 1;

	dp->type = DP_INDRCT_OBJ;
	dp->status = DS_ASSIGNED;

	dp->data.indirect.displacement = displacement;
	dp->data.indirect.base_dp = base_dp;
	dp->data.indirect.index_dp = index_dp;
	dp->data.indirect.base_id = base_dp->data.reg.id;
	dp->data.indirect.index_id = -1;
	dp->data.indirect.scale = 1;

	if (index_dp) {
		if (index_dp->type == DP_REG) {
			dp->data.indirect.index_id = index_dp->data.reg.id;
			// for X86 CPU. need to move to x86dataallocator for future.
			if (dp->size == 1 || dp->size == 2 || dp->size == 4 || dp->size == 8) {
				dp->data.indirect.scale = dp->size;
			} else {
				dp->data.indirect.scale = 1;
				need_to_mul = dp->size;
			}

		} else if (index_dp->type == DP_LIT_INT) {
			BOOST_ASSERT(displacement == 0);
			dp->data.indirect.displacement = index_dp->data.intValue * dp->size;

		} else
			BOOST_ASSERT(false);
	}

	dp->alloc_step = step;
	dp->release_step = step;
	all.push_back(dp);

	return need_to_mul;
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

PlnDataPlace* PlnDataAllocator::getLiteralFloDp(double floValue)
{
	PlnDataPlace* dp = new PlnDataPlace(8, DT_FLOAT);
	dp->type = DP_LIT_FLO;
	dp->status = DS_ASSIGNED;
	dp->data.floValue = floValue;
	dp->alloc_step = step;
	dp->release_step = step;
	static string cmt = "$f";
	dp->comment = &cmt;
	all.push_back(dp);
	return dp;
}

PlnDataPlace* PlnDataAllocator::getROStrArrayDp(string &str)
{
	PlnDataPlace* dp = new PlnDataPlace(8, DT_OBJECT_REF);
	dp->type = DP_RO_STR;
	dp->status = DS_ASSIGNED;
	dp->data.rostr = new string(str);
	dp->alloc_step = step;
	dp->release_step = step;
	static string cmt = "\"..\"";
	dp->comment = &cmt;
	all.push_back(dp);
	return dp;
}

PlnDataPlace* PlnDataAllocator::getRODataDp(vector<PlnRoData>& rodata)
{
	PlnDataPlace* dp = new PlnDataPlace(8, DT_OBJECT_REF);
	dp->type = DP_RO_DATA;
	dp->status = DS_ASSIGNED;
	dp->data.rodata = new vector<PlnRoData>();
	*(dp->data.rodata) = move(rodata);
	dp->alloc_step = step;
	dp->release_step = step;
	static string cmt = "\"{..}\"";
	dp->comment = &cmt;
	all.push_back(dp);
	return dp;
}

PlnDataPlace* PlnDataAllocator::getSeparatedDp(PlnDataPlace* dp)
{
	BOOST_ASSERT(dp->type != DP_SUBDP && dp->type != DP_INDRCT_OBJ);

	auto sub_dp = new PlnDataPlace(dp->size, dp->data_type);
	sub_dp->type = DP_SUBDP;
	sub_dp->data_type = dp->data_type;
	sub_dp->size = dp->size;
	sub_dp->data.originalDp = dp;
	sub_dp->comment = dp->comment;

	sub_dbs.push_back(sub_dp);
	return sub_dp;
}

void PlnDataAllocator::finish()
{
	int offset = 0;
	// Set offset from base stack pointer.
	for (auto dp: data_stack) {
		offset -= 8;
		for (auto dpp=dp; dpp; dpp = dpp->previous) {
			if (dpp->type == DP_BYTES) {
				for (auto child: *dpp->data.bytesData) {
					child->data.stack.offset = offset + child->data.bytes.offset;
				}
			} else if (dpp->type == DP_STK_BP) {
				dpp->data.stack.offset = offset;
			} else
				BOOST_ASSERT(dpp->type == DP_STK_RESERVE_BP);
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
			if (auto base_dp = dp->data.indirect.base_dp) {
				int base_id = base_dp->data.reg.id;
				if (regs[base_id] != base_dp) return true;
			}
			if (auto index_dp = dp->data.indirect.index_dp) {
				if (index_dp->type == DP_REG) {
					int index_id = index_dp->data.reg.id;
					if (regs[index_id] != index_dp) return true;
				}
			}
			return false;
		}
		case DP_GLBL:
		case DP_RO_STR:
		case DP_RO_DATA:
		case DP_LIT_INT:
		case DP_LIT_FLO:
		case DP_BYTES:
		case DP_STK_BP:
			return false;
	}
	// not implemented.
	BOOST_ASSERT(false);
} // LCOV_EXCL_LINE

bool tryAccelerateAlloc(PlnDataPlace *dp, int push_step)
{
	if (dp->type == DP_REG) {
		if (dp->status == DS_ASSIGNED || dp->status == DS_RELEASED) {
			if (dp->alloc_step <= push_step) return true;

			// if alloc_step > push_step
			if (!dp->previous || dp->previous->release_step < push_step) {
				dp->alloc_step = push_step;
				return true;
			}
		} else
			BOOST_ASSERT(false);
	}
	// TODO: DP_INDRCT_OBJ
	return false;
}

void updateReleaseStep(PlnDataPlace *dp, int new_release_step)
{
	BOOST_ASSERT(dp->status == DS_RELEASED);
	BOOST_ASSERT(dp->alloc_step <= new_release_step);
	dp->release_step = new_release_step;

	if (dp->type == DP_INDRCT_OBJ) {
		if (auto base_dp = dp->data.indirect.base_dp) {
			base_dp->alloc_step = base_dp->release_step = new_release_step;
		}
		if (auto index_dp = dp->data.indirect.index_dp) {
			index_dp->alloc_step = index_dp->release_step = new_release_step;
		}
	}
}

void PlnDataAllocator::popSrc(PlnDataPlace* dp)
{
	BOOST_ASSERT(dp->src_place);
	auto src_place = dp->src_place;

	src_place->access(step);

	// Release source if flag on.
	if (dp->release_src_pop) {
		releaseDp(src_place);
	}

	bool is_src_destroyed = isDestroyed(src_place);

	// Assign dst data place if ready.
	if (dp->status == DS_READY_ASSIGN) {
		allocDp(dp, false);
	}
	dp->access(step);

	// check src data would be destory.
	if (!dp->save_place) {
		if (is_src_destroyed) {
			if (tryAccelerateAlloc(dp, dp->push_src_step)) {
				if (dp->release_src_pop) {
					updateReleaseStep(src_place, dp->push_src_step);
				}
				dp->save_place = dp;
			} else { 
				allocSaveData(dp, src_place->alloc_step, step);
				dp->save_place->comment = new string("(save-" + dp->cmt() + ")");
				dp->save_place->access(step);
			}
		}
	}
	
	// Updated save & src place status;
	if (dp->save_place) {
		if (dp->release_src_pop) {
			src_place->release_step = dp->push_src_step;
		}
		if (dp->save_place != dp) {
			releaseDp(dp->save_place);
		} 
	} 

	step++;
}

void PlnDataAllocator::checkDataLeak()
{
	for (PlnDataPlace* dp: data_stack) {
		if ((*(dp->comment))[0] == '(') {
			BOOST_ASSERT(dp->status == DS_RELEASED);
		}
	}
	for (PlnDataPlace* dp: arg_stack) {
		BOOST_ASSERT(dp->status == DS_RELEASED);
	}
}

// PlnDataPlace
PlnDataPlace::PlnDataPlace(int size, int data_type)
	: type(DP_UNKNOWN), size(size), data_type(data_type), status(DS_UNKNOWN),
		release_src_pop(true), load_address(false),
		need_address(false), do_clear_src(false),
		alloc_step(0), release_step(INT_MAX),
		previous(NULL), save_place(NULL), src_place(NULL),
		access_score(0), custom_inf(0)
{
	static string emp = "";
	comment = &emp;
}

PlnDataPlace::~PlnDataPlace()
{
	if (type == DP_RO_STR)
		delete data.rostr;
	else if (type == DP_RO_DATA)
		delete data.rodata;
}

unsigned int PlnDataPlace::getAllocBytesBits(int alloc_step, int release_step)
{
	// return 8bit flg. e.g) 0000 0100: a byte of offset 2 byte is alloced.
	BOOST_ASSERT(type == DP_BYTES);
	unsigned int alloc_bytes = 0;
	for (auto child: (*data.bytesData))  {
		// exists overlaped span
		if (child->alloc_step <= release_step
				&& child->release_step >= alloc_step)
			switch (child->size) {
				case 1:	
					alloc_bytes |= 0x1 << child->data.bytes.offset; break;
				case 2:
					alloc_bytes |= 0x3 << child->data.bytes.offset; break;
				case 4:
					alloc_bytes |= 0xf << child->data.bytes.offset; break;
			}
	}

	return alloc_bytes;
}

bool PlnDataPlace::tryAllocBytes(PlnDataPlace* dp)
{
	BOOST_ASSERT(type == DP_BYTES);
	BOOST_ASSERT(dp->size <= 4);

	int dp_size = dp->size;
	unsigned int alloc_bytes = getAllocBytesBits(dp->alloc_step, dp->release_step);
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
			dp->data.bytes.parent_dp = this;
			updateBytesDpStatus();
			return true;
		}
	}

	return false;
}

void PlnDataPlace::updateBytesDpStatus()
{
	BOOST_ASSERT(type == DP_BYTES);
	BOOST_ASSERT(data.bytesData->size());
	bool is_released = true;
	int alloc_step = INT_MAX;
	int release_step = 0;
	for (auto child: (*data.bytesData)) {
		if (child->status != DS_RELEASED)
			is_released = false;
		if (alloc_step > child->alloc_step)
			alloc_step = child->alloc_step;
		if (release_step < child->release_step)
			release_step = child->release_step;

	}
	status = is_released ? DS_RELEASED : DS_ASSIGNED_SOME;
	this->alloc_step = alloc_step;
	this->release_step = release_step;
}

void PlnDataPlace::access(int32_t step)
{
	access_score+=10;
	if (type == DP_SUBDP) {
		data.originalDp->access_score+=10;
	}
	if (load_address) {
		// for register allocation info
		BOOST_ASSERT(src_place);
		if (src_place->type == DP_SUBDP)
			src_place->data.originalDp->need_address = true;
		else 
			src_place->need_address = true;
	}
}

