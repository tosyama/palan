/// Heap allocation class definition.
///
/// @file	PlnHeapAllocator.cpp
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "PlnHeapAllocator.h"
#include "PlnType.h"
#include "PlnBlock.h"
#include "PlnStatement.h"
#include "PlnVariable.h"
#include "PlnLoopStatement.h"
#include "expressions/PlnArrayItem.h"
#include "expressions/PlnAssignment.h"
#include "expressions/PlnCmpOperation.h"
#include "expressions/PlnAddOperation.h"
#include "expressions/PlnFunctionCall.h"
#include "PlnConditionalBranch.h"
#include "../PlnDataAllocator.h"
#include "../PlnGenerator.h"
#include "../PlnConstants.h"
#include "../PlnBuildTreeHelper.h"

PlnHeapAllocator::PlnHeapAllocator()
	: PlnExpression(ET_HP_ALLOC)
{
}

class PlnArrayHeapAllocator : public PlnHeapAllocator
{
public:
	PlnBlock* block;

	PlnArrayHeapAllocator(PlnValue var_val);
	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void dump(ostream& os, string indent="") override;
	void gen(PlnGenerator& g) override;
};

PlnArrayHeapAllocator::PlnArrayHeapAllocator(PlnValue var_val)
{
	BOOST_ASSERT(var_val.type == VL_VAR);
	vector<PlnType*> var_type = var_val.inf.var->var_type;

	BOOST_ASSERT(var_type.size() >= 2);
	vector<PlnType*> item_type = var_type;
	item_type.pop_back();

	auto arr_inf = &var_type.back()->inf.fixedarray;
	block = new PlnBlock();

	BOOST_ASSERT(arr_inf->is_fixed_size);
	palan::malloc(block, var_val.inf.var, arr_inf->alloc_size);

	if (item_type.back()->data_type != DT_OBJECT_REF) {
		// The item's type is primitive type. Do not allocate any more.
		return;
	}

	// Construct model tree for allocating array items.
	//  uint64 __cnt = 0;
	PlnVariable* cnt_var = palan::declareUInt(block, "__cnt", 0);

	// while __cnt < item_num {
	uint64_t item_num = arr_inf->alloc_size / arr_inf->item_size;
	auto wblock = palan::whileLess(block, cnt_var, item_num);
	{
		// declare variable: __item;
		// VarInit will create array item's heap allocator.
		PlnVariable *item_var;
		{
			static string item_name = "__item";	
			auto item_var_type = item_type;
			item_var = wblock->declareVariable(item_name, item_var_type, true);
			vector<PlnValue> vars = {item_var};

			wblock->statements.push_back(new PlnStatement(new PlnVarInit(vars), wblock));
		}

		// __item ->> __arr[__cnt];
		{
			auto arr_item = palan::rawArrayItem(var_val.inf.var, cnt_var);
			arr_item->values[0].asgn_type = ASGN_MOVE;
			vector<PlnExpression*> lvals = { arr_item };
			vector<PlnExpression*> exps = { new PlnExpression(item_var) };

			auto assign = new PlnAssignment(lvals, exps);
			wblock->statements.push_back(new PlnStatement(assign, wblock));
		}

		// __cnt+1 -> __cnt;
		palan::incrementUInt(wblock, cnt_var, 1);
	} // }
}

void PlnArrayHeapAllocator::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	block->finish(da, si);
}

void PlnArrayHeapAllocator::dump(ostream& os, string indent)
{
	os << indent << "ArrayHeapAllocator" << endl;
}

void PlnArrayHeapAllocator::gen(PlnGenerator& g)
{
	block->gen(g);
}

PlnHeapAllocator* PlnHeapAllocator::createHeapAllocation(PlnValue var_val)
{
	auto var = var_val.inf.var;
	PlnType* vt = var->var_type.back();
	if (var->ptr_type & PTR_OWNERSHIP) {
		if (vt->name.back() == ']') {
			return new PlnArrayHeapAllocator(var_val);
		}
	}

	return NULL;	
}

