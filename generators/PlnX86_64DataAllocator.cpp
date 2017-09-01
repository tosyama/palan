/// x86-64 (Linux) data place management class definition.
///
/// @file	PlnX86_64DataAllocator.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu

#include <cstddef>
#include <boost/assert.hpp>
#include "PlnX86_64DataAllocator.h"

static const int ARG_TBL[] = { RAX, RDI, RSI, RDX, RCX, R8, R9 };
static const int SYSARG_TBL[] = { RAX, RDI, RSI, RDX, R10, R8, R9 };

void PlnX86_64DataAllocator::reset()
{
	for (auto& preg: place_regs)
		preg.resize(0);

	for (auto& sreg: save_regs)
		sreg = NULL;
	
	tmp_stack.resize(0);
	arg_stack.resize(0);
	alldp.resize(0);
}

void PlnX86_64DataAllocator::pushReg(int reg, PlnDataPlace* dp)
{
	PlnDataPlace* pre_reg = NULL;

	if (place_regs[reg].size() > 0)
		pre_reg = place_regs[reg].back();

	dp->type = dp->place_type = DP_REG;
	dp->index = dp->place_index = reg;
	place_regs[reg].push_back(dp);

	if (save_regs[reg]) {
		// cant use the register for save. so, need to change save place;
		PlnDataPlace* sav_dp = save_regs[reg];
		save_regs[reg] = NULL;
		assignAnother(sav_dp);
	}

	if (pre_reg) {
		if (pre_reg->type == DP_REG && pre_reg->index==reg)
			assignAnother(pre_reg);
	}
}


void PlnX86_64DataAllocator::pushArgStack(int index, PlnDataPlace* dp)
{
	PlnDataPlace* pre_arg = NULL;

	if (arg_stack.size() < index+1)
		arg_stack.resize(index+1);
	if (arg_stack[index].size() > 0)
		pre_arg = arg_stack[index].back();

	dp->type = dp->place_type = DP_ARG_STK;
	dp->index = dp->place_index = index;
	arg_stack[index].push_back(dp);

	if (pre_arg)
		if (pre_arg->type==DP_ARG_STK && pre_arg->index == index)
			assignAnother(pre_arg);
}

void PlnX86_64DataAllocator::assignAnother(PlnDataPlace* dp)
{
	static const int search_regs[] = {RDX, RCX, RSI, RDI, R11, R9, R8}; // for dev
	for (auto r: search_regs)
		if (place_regs[r].size() == 0 && save_regs[r] == NULL) {
			dp->type = DP_REG;
			dp->index = r;
			save_regs[r] = dp;
			return ;
		}

	int i;
	for(i=0; i<tmp_stack.size(); ++i)
		if (!tmp_stack[i]) break;
	if (i==tmp_stack.size()) tmp_stack.push_back(dp);
	else tmp_stack[i] = dp;

	dp->type = DP_TEMP_STK;
	dp->index = i;
}

void PlnX86_64DataAllocator::pushArgDp(int index, PlnDataPlace* dp)
{
	BOOST_ASSERT(dp->sign != DPS_FLOAT);
	alldp.push_back(dp);

	dp->use = PU_ARG;

	if (index <= 6) pushReg(ARG_TBL[index], dp);
	else pushArgStack(index-7, dp);
}

PlnDataPlace* PlnX86_64DataAllocator::popArgDp(int index)
{
	int idx;
	PlnDataPlace* dp;
	if (index <= 6) {
		idx = ARG_TBL[index];
		dp = place_regs[idx].back();
		place_regs[idx].pop_back();
	} else {
		idx = index - 7;
		dp = arg_stack[idx].back();
		arg_stack[idx].pop_back();
	}

	return dp;
}

void PlnX86_64DataAllocator::pushSysArgDp(int index, PlnDataPlace* dp)
{
	BOOST_ASSERT(dp->sign != DPS_FLOAT);
	BOOST_ASSERT(index <= 6);

	alldp.push_back(dp);
	dp->use = PU_ARG;
	pushReg(SYSARG_TBL[index], dp);
}

PlnDataPlace* PlnX86_64DataAllocator::popSysArgDp(int index)
{
	int idx;
	PlnDataPlace* dp;

	BOOST_ASSERT(index <= 6);

	idx = SYSARG_TBL[index];
	dp = place_regs[idx].back();
	place_regs[idx].pop_back();

	return dp;
}

void PlnX86_64DataAllocator::funcCalled()
{
	static const int destoroy_regs[] = {RAX, RCX, RDX, RSI, RDI, R8, R9, R10, R11};
	// save data that would be destoroyed to memory.
	for (auto reg: destoroy_regs) {
		auto& regs = place_regs[reg];
		if (regs.size() > 0) {
			PlnDataPlace* dp = regs.back();
			if (dp->type == DP_REG && dp->index == reg) {
				int i;
				for(i=0; i<tmp_stack.size(); ++i)
					if (!tmp_stack[i]) break;
				if (i==tmp_stack.size()) tmp_stack.push_back(dp);
				else tmp_stack[i] = dp;

				dp->type = DP_TEMP_STK;
				dp->index = i;
			}
		}
		if (save_regs[reg] != NULL) {
			PlnDataPlace* dp = save_regs[reg];
			save_regs[reg] = NULL;
			int i;
			for(i=0; i<tmp_stack.size(); ++i)
				if (!tmp_stack[i]) break;
			if (i==tmp_stack.size()) tmp_stack.push_back(dp);
			else tmp_stack[i] = dp;

			dp->type = DP_TEMP_STK;
			dp->index = i;
		}
	}
}
