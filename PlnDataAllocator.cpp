/// Register/Stack allocation class definition.
///
/// @file	PlnDataAllocator.cpp
/// @copyright	2017-2018 YAMAGUCHI Toshinobu

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
		if (aft_dp) {
			dp_ctnr->previous = aft_dp->previous;
			aft_dp->previous = dp_ctnr;
		} else {
			dp_ctnr->previous = data_stack[free_index];
			data_stack[free_index] = dp_ctnr;
		}
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
	new_dp->status = DS_READY_ASSIGN;

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
	auto pdp = dp->previous;
	dp->alloc_step = step;
	if (pdp && pdp->status != DS_RELEASED
			&& !pdp->save_place) {
		allocSaveData(pdp, pdp->alloc_step, pdp->release_step);
		pdp->save_place->comment = new string("(savex)");
	}

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

	if (dp->size < 8 && dp->type == DP_STK_BP) {
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

vector<PlnDataPlace*> PlnDataAllocator::prepareArgDps(int func_type, const vector<int> &ret_dtypes, const vector<int> &arg_dtypes, bool is_callee)
{
	int arg_num = arg_dtypes.size();

	vector<PlnDataPlace*> dps;
	for (int i=0; i<arg_num; ++i) {
		auto dp = createArgDp(func_type, ret_dtypes, arg_dtypes, i, is_callee);
		static string cmt="arg";
		dp->comment = &cmt;
		dp->status = DS_READY_ASSIGN;
		dps.push_back(dp);
	}

	return dps;
}

vector<PlnDataPlace*> PlnDataAllocator::prepareRetValDps(int func_type, vector<int> &ret_dtypes, vector<int> &arg_dtypes, bool is_callee)
{
	int ret_num = ret_dtypes.size();
	vector<PlnDataPlace*> dps;

	for (int i=0; i<ret_num; ++i) {
		static string cmt="return";
		auto dp = createReturnDp(func_type, ret_dtypes, arg_dtypes, i, is_callee);
		dp->comment = &cmt;
		dp->status = DS_READY_ASSIGN;
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
	if (index_dp)
		dp->data.indirect.index_id = index_dp->data.reg.id;
	else 
		dp->data.indirect.index_id = -1;

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

PlnDataPlace* PlnDataAllocator::getReadOnlyDp(int index)
{
	PlnDataPlace* dp = new PlnDataPlace(8, DT_OBJECT_REF);
	dp->type = DP_RO_DATA;
	dp->status = DS_ASSIGNED;
	dp->data.ro.index = index;
	dp->data.ro.item_size = 1;
	dp->data.ro.int_array = NULL;
	dp->alloc_step = step;
	dp->release_step = step;
	static string cmt = "\"..\"";
	dp->comment = &cmt;
	all.push_back(dp);
	return dp;
}

PlnDataPlace* PlnDataAllocator::getROIntArrayDp(vector<int64_t> int_array, int item_size)
{
	PlnDataPlace* dp = new PlnDataPlace(8, DT_OBJECT_REF);
	dp->type = DP_RO_DATA;
	dp->status = DS_ASSIGNED;
	dp->data.ro.index = -1;
	dp->data.ro.item_size = item_size;
	dp->data.ro.int_array = new vector<int64_t>(move(int_array));
	dp->data.ro.flo_array = NULL;
	dp->alloc_step = step;
	dp->release_step = step;
	static string cmt = "\"[..]\"";
	dp->comment = &cmt;
	all.push_back(dp);
	return dp;
}

PlnDataPlace* PlnDataAllocator::getROFloArrayDp(vector<double> flo_array, int item_size)
{
	PlnDataPlace* dp = new PlnDataPlace(8, DT_OBJECT_REF);
	dp->type = DP_RO_DATA;
	dp->status = DS_ASSIGNED;
	dp->data.ro.index = -1;
	dp->data.ro.item_size = item_size;
	dp->data.ro.int_array = NULL;
	dp->data.ro.flo_array = new vector<double>(move(flo_array));
	dp->alloc_step = step;
	dp->release_step = step;
	static string cmt = "\"[..]\"";
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
				int index_id = index_dp->data.reg.id;
				if (regs[index_id] != index_dp) return true;
			}
			return false;
		}
		case DP_RO_DATA:
		case DP_LIT_INT:
		case DP_LIT_FLO:
		case DP_BYTES:
		case DP_STK_BP:
			return false;
		default: // not implemented.
			BOOST_ASSERT(false);
	}
}

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

static void prePopSrc(PlnDataAllocator& da, PlnDataPlace *dp)
{
	if (dp->type == DP_INDRCT_OBJ) {
		if (auto base_dp = dp->data.indirect.base_dp) {
			da.popSrc(base_dp);
		}
		if (auto index_dp = dp->data.indirect.index_dp) {
			da.popSrc(index_dp);
		}
	}
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

	prePopSrc(*this, src_place);
	src_place->access(step);

	// Release source if flag on.
	if (dp->release_src_pop) {
		releaseDp(src_place);
	}

	prePopSrc(*this, dp);

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
	: type(DP_UNKNOWN), status(DS_UNKNOWN),
		access_count(0), alloc_step(0), release_step(INT_MAX),
		previous(NULL), save_place(NULL), src_place(NULL),
		size(size), data_type(data_type), release_src_pop(true), load_address(false), do_clear_src(false)
{
	static string emp="";
	comment = &emp;
}

PlnDataPlace::~PlnDataPlace()
{
	if (type == DP_RO_DATA && data.ro.int_array) {
		delete data.ro.int_array;
	}
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

	int dp_size  = dp->size;
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
	access_count++;
	if (type == DP_SUBDP) {
		data.originalDp->access_count++;
	}
}

