/// Register/Stack allocation class definition.
///
/// @file	PlnDataAllocator.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu

#include "PlnDataAllocator.h"
#include <stdlib.h>
#include <limits.h>
#include <boost/assert.hpp>

PlnDataAllocator::PlnDataAllocator(int regnum)
	: regnum(regnum)
{
	regs.resize(regnum);
	for (auto& reg: regs)
		reg = NULL;

	step = 0;
}

PlnDataPlace* PlnDataAllocator::allocDataWithDetail(int size, int alloc_step, int release_step)
{
	PlnDataPlace* dp = new PlnDataPlace();
	dp->type = DP_STK_BP;
	dp->size = size;
	dp->status = DS_ASSIGNED;
	dp->accessCount = 0;
	dp->previous = NULL;
	dp->alloc_step = alloc_step;
	dp->release_step = release_step;
	dp->save_place = NULL;

	if (size < 8) {
		// TODO search alloc some bytes place first.
		BOOST_ASSERT(false);
	}

	int s=data_stack.size();
	for (int i=0; i<s; i++) {
		PlnDataPlace *pdp = data_stack[i];
		if (pdp->status == DS_RELEASED) {
			dp->data.stack.idx = i;
			data_stack[i] = dp;
			dp->previous = pdp;
			return dp;
		}
	}

	dp->data.stack.idx = data_stack.size();
	data_stack.push_back(dp);
	return dp;
}

PlnDataPlace* PlnDataAllocator::allocData(int size)
{
	return allocDataWithDetail(size, step++, INT_MAX);
}

void PlnDataAllocator::allocSaveData(PlnDataPlace* dp)
{
	BOOST_ASSERT(dp->save_place == NULL);
	dp->save_place = allocDataWithDetail(dp->size, dp->alloc_step, dp->release_step);
}

void PlnDataAllocator::releaseData(PlnDataPlace* dp)
{
	dp->status = DS_RELEASED;
	dp->release_step = step;
	step++;
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
		dp->data.stack.offset = offset;
		PlnDataPlace* dpp = dp->previous;
		while (dpp) {
			dpp->data.stack.offset = offset;
			dpp = dpp->previous;
		}
		offset += 8;
	}
	int stk_size = data_stack.size() + arg_stack.size();
	if (stk_size % 2) stk_size += 1;  // for 16byte align.
	stack_size = stk_size * 8;
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

