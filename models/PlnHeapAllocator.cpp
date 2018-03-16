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
			arr_item->values[0].lval_type = LVL_MOVE;
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

// PlnArrayHeapFreer
class PlnArrayHeapFreer : public PlnHeapAllocator
{
public:
	PlnVariable *arr_var;
	PlnBlock* block;

	PlnArrayHeapFreer(PlnVariable* var);
	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void dump(ostream& os, string indent="") override;
	void gen(PlnGenerator& g) override;
};

PlnArrayHeapFreer::PlnArrayHeapFreer(PlnVariable* var)
	: arr_var(var), block(NULL)
{
	vector<PlnType*> item_type = var->var_type;
	item_type.pop_back();

	block = new PlnBlock();

	if (item_type.back()->data_type != DT_OBJECT_REF) {
		palan::free(block, arr_var);
		return;
	}
	
	BOOST_ASSERT(item_type.back()->inf.obj.is_fixed_size);
	// if arr_var {
	{
		auto ifblock = new PlnBlock();
		auto if_arr = new PlnIfStatement(new PlnExpression(arr_var), ifblock, NULL, block);

		// {
		//  uint64 __cnt = 0;
		PlnVariable *cnt_var = palan::declareUInt(ifblock, "__cnt", 0);

		// while __cnt < item_num {
		auto arr_inf = &arr_var->var_type.back()->inf.fixedarray;
		uint64_t item_num = arr_inf->alloc_size / arr_inf->item_size;
		auto wblock = palan::whileLess(ifblock, cnt_var, item_num);
		{
			// The block will create HeapFree for owner varialbes.
			// So, the moved array item will free when exit block.
			{
				// ? __item <<= __arr[__cnt];
				static string item_name = "__item";
				auto item_var_type = item_type;
				auto item_var = wblock->declareVariable(item_name, item_var_type, true);
				vector<PlnValue> vars = { item_var };
				vars[0].lval_type = LVL_MOVE;

				auto arr_item = palan::rawArrayItem(arr_var, cnt_var);
				vector<PlnExpression*> inis = { arr_item };

				wblock->statements.push_back(new PlnStatement(new PlnVarInit(vars, inis), wblock));

				// __cnt+1 -> __cnt;
				palan::incrementUInt(wblock, cnt_var, 1);
			}

			// free(__arr)
			palan::free(ifblock, arr_var);
		}

		block->statements.push_back(if_arr);
	}
	// }
}

void PlnArrayHeapFreer::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	block->finish(da, si);
}

void PlnArrayHeapFreer::dump(ostream& os, string indent)
{
	os << indent << "ArrayHeapFreer" << endl;
}

void PlnArrayHeapFreer::gen(PlnGenerator& g)
{
	block->gen(g);
}

PlnHeapAllocator* PlnHeapAllocator::createHeapAllocation(PlnValue var_val)
{
	auto var = var_val.inf.var;
	PlnType* vt = var->var_type.back();
	if (var->ptr_type & PTR_OWNERSHIP) {
		if (vt->name == "[]") {
			return new PlnArrayHeapAllocator(var_val);
		}
	}

	return NULL;	
}

PlnHeapAllocator* PlnHeapAllocator::createHeapFree(PlnVariable* var)
{
	PlnType* vt = var->var_type.back();
	if (var->ptr_type & PTR_OWNERSHIP) {
		if (vt->name == "[]") {
			return new PlnArrayHeapFreer(var);
		}
	}
	return NULL;
}
