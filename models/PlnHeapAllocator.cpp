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
		return;
	}

	// Construct model tree for allocate array member.

	// {
	//  uint64 __cnt = 0;
	PlnVariable* cnt_var = palan::declareUInt(block, "__cnt", 0);

	{	// while __cnt < item_num {
		PlnCmpOperation *cmp_op;
		{
			uint64_t item_num = arr_inf->alloc_size / arr_inf->item_size;

			auto cnt_ex = new PlnExpression(cnt_var);
			auto n_ex = new PlnExpression((uint64_t) item_num);
			cmp_op = new PlnCmpOperation(cnt_ex, n_ex, CMP_L);
		}

		auto wblock = new PlnBlock();
		auto whl = new PlnWhileStatement(cmp_op, wblock, block);

		// ? __item;
		PlnVariable *item_var;
		{
			static string item_name = "__item";
			auto item_var_type = item_type;
			item_var = wblock->declareVariable(item_name, item_var_type);
			vector<PlnValue> vars = {item_var};

			wblock->statements.push_back(new PlnStatement(new PlnVarInit(vars), wblock));
		}

		// __item -> __arr[__cnt];
		{
			auto arr_ex = new PlnExpression(var_val);
			auto cnt_ex = new PlnExpression(cnt_var);
			vector<PlnExpression*> inds = { cnt_ex };

			vector<PlnType*> arr_type = var_type;
			arr_type.back() = PlnType::getRawArray(); // [,,..] => [?]

			auto arr_item = new PlnArrayItem(arr_ex, inds, arr_type);
			vector<PlnExpression*> lvals = { arr_item };
			
			vector<PlnExpression*> exps = { new PlnExpression(item_var) };
			auto assign = new PlnAssignment(lvals, exps);
			wblock->statements.push_back(new PlnStatement(assign, wblock));
		}

		// __cnt+1 -> __cnt;
		palan::incrementUInt(wblock, cnt_var, 1);

		block->statements.push_back(whl);
	}	// }
	// }

}

void PlnArrayHeapAllocator::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	BOOST_ASSERT(block);
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

		{	// while __cnt < item_num {
			PlnCmpOperation *cmp_op;
			{
				uint64_t item_num = 1;
				for (int sz: *arr_var->var_type.back()->inf.fixedarray.sizes)
					item_num = item_num * sz;

				auto cnt_ex = new PlnExpression(cnt_var);
				auto n_ex = new PlnExpression((uint64_t) item_num);
				cmp_op = new PlnCmpOperation(cnt_ex, n_ex, CMP_L);
			}

			// If the block is child, the block will create HeapFree for varialbes.
			// So, the moved array item will free when exit block.
			// Block will create Freer of child itrms.
			auto wblock = new PlnBlock();
			wblock->setParent(block);
			{
				// ? __item <<= __arr[__cnt];
				static string item_name = "__item";
				auto item_var_type = item_type;
				auto item_var = wblock->declareVariable(item_name, item_var_type);
				vector<PlnValue> vars = {item_var};

				vars[0].lval_type = LVL_MOVE;

				vector<PlnType*> arr_type = arr_var->var_type;
				arr_type.back() = PlnType::getRawArray(); // [,,..] => [?]
				auto arr_ex = new PlnExpression(arr_var);
				auto cnt_ex = new PlnExpression(cnt_var);
				vector<PlnExpression*> inds = {cnt_ex};
				auto arr_item = new PlnArrayItem(arr_ex, inds, arr_type);

				vector<PlnExpression*> inis;
				inis.push_back(arr_item);
				wblock->statements.push_back(new PlnStatement(new PlnVarInit(vars, inis), wblock));

				// __cnt+1 -> __cnt;
				palan::incrementUInt(wblock, cnt_var, 1);
			}

			auto whl = new PlnWhileStatement(cmp_op, wblock, ifblock);
			ifblock->statements.push_back(whl);

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
	PlnType* vt = var_val.inf.var->var_type.back();
	if (vt->name == "[]") {
		return new PlnArrayHeapAllocator(var_val);
	}

	return NULL;	
}

PlnHeapAllocator* PlnHeapAllocator::createHeapFree(PlnVariable* var)
{
	PlnType* vt = var->var_type.back();
	if (vt->name == "[]") {
		return new PlnArrayHeapFreer(var);
	}

	return NULL;
}
