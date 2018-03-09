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
	PlnDataPlace *ret_dp;
	PlnVariable *arr_var;
	vector<PlnType*> var_type;
	PlnBlock* block;

	PlnArrayHeapAllocator(vector<PlnType*> &var_type);
	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void dump(ostream& os, string indent="") override;
	void gen(PlnGenerator& g) override;
};

PlnArrayHeapAllocator::PlnArrayHeapAllocator(vector<PlnType*> &var_type)
	: ret_dp(NULL), var_type(var_type), block(NULL)
{
	// setup return value info.
	PlnValue v;
	v.type = VL_WORK;
	v.lval_type = NO_LVL;
	v.inf.wk_type = var_type.back();
	values.push_back(v);

	BOOST_ASSERT(var_type.size() >= 2);
	vector<PlnType*> item_type = var_type;
	item_type.pop_back();

	if (item_type.back()->data_type != DT_OBJECT_REF)
		return;

	// Construct model tree for allocate array member.
	
	BOOST_ASSERT(item_type.back()->inf.obj.is_fixed_size);
	arr_var = new PlnVariable();
	arr_var->name = "__arr";
	arr_var->var_type = var_type;
	arr_var->var_type.back() = PlnType::getRawArray();
	block = new PlnBlock();

	// {
	//  uint64 __cnt = 0;
	PlnVariable* cnt_var = palan::declareUInt(block, "__cnt", 0);

	{	// while __cnt < item_num {
		PlnCmpOperation *cmp_op;
		{
			uint64_t item_num = 1;
			for (int sz: *var_type.back()->inf.fixedarray.sizes)
				item_num = item_num * sz;

			auto cnt_ex = new PlnExpression(cnt_var);
			auto n_ex = new PlnExpression((uint64_t) item_num);
			cmp_op = new PlnCmpOperation(cnt_ex, n_ex, CMP_L);
		}

		auto wblock = new PlnBlock();
		auto whl = new PlnWhileStatement(cmp_op, wblock, block);

		// allocated address -> __arr[__cnt];
		{
			PlnHeapAllocator* h_alloc = createHeapAllocation(item_type);
			BOOST_ASSERT(h_alloc);

			vector<PlnExpression*> lvals, exps, inds;
			auto arr_ex = new PlnExpression(arr_var);
			auto cnt_ex = new PlnExpression(cnt_var);
			inds.push_back(cnt_ex);
			auto arr_item = new PlnArrayItem(arr_ex, inds);

			lvals.push_back(arr_item);
			exps.push_back(h_alloc);
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
	BOOST_ASSERT(data_places.size());
	auto ainf = &var_type.back()->inf.fixedarray;

	BOOST_ASSERT(ainf->is_fixed_size);
	da.memAlloced();
	if (block) {
		arr_var->place = da.allocData(8, DT_OBJECT_REF);
		arr_var->place->comment = &arr_var->name;
		ret_dp = arr_var->place;	
		block->finish(da, si);
	} else {
		ret_dp = da.prepareAccumulator(DT_OBJECT_REF);
		da.allocDp(ret_dp);
	}
	da.pushSrc(data_places[0], ret_dp);

}

void PlnArrayHeapAllocator::dump(ostream& os, string indent)
{
	os << indent << "ArrayHeapAllocator" << endl;
}

void PlnArrayHeapAllocator::gen(PlnGenerator& g)
{
	auto ainf = &var_type.back()->inf.fixedarray;

	auto e = g.getEntity(ret_dp);
	g.genMemAlloc(e.get(), ainf->alloc_size, "alloc array");
	if (block) block->gen(g);
	g.genSaveSrc(data_places[0]);
}

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
				wblock->statements.push_back(new PlnStatement(new PlnVarInit(vars, inis), block));

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

PlnHeapAllocator* PlnHeapAllocator::createHeapAllocation(vector<PlnType*> &var_type)
{
	PlnType* vt = var_type.back();
	if (vt->name == "[]") {
		return new PlnArrayHeapAllocator(var_type);
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
