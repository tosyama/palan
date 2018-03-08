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
#include "PlnConditionalBranch.h"
#include "../PlnDataAllocator.h"
#include "../PlnGenerator.h"
#include "../PlnConstants.h"

PlnHeapAllocator::PlnHeapAllocator()
	: PlnExpression(ET_HP_ALLOC)
{
}

class PlnArrayHeapAllocator : public PlnHeapAllocator
{
	void initArrayVar();
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

void PlnArrayHeapAllocator::initArrayVar()
{
	arr_var = new PlnVariable();
	arr_var->name = "__arr";
	arr_var->var_type = var_type;
	arr_var->var_type.back() = PlnType::getRawArray();
}

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

	// Construct ATS for allocate array member.
	
	BOOST_ASSERT(item_type.back()->inf.obj.is_fixed_size);
	initArrayVar();
	block = new PlnBlock();
	// {
	//  uint64 __cnt = 0;
	vector<PlnType*> tv;
	static string cnt_name = "__cnt";
	tv.push_back(PlnType::getSint());
	vector<PlnValue> vars;
	PlnVariable *cnt_var = block->declareVariable(cnt_name, tv);
	vars.push_back(cnt_var);

	vector<PlnExpression*> inis;
	auto zero_ex = new PlnExpression((uint64_t) 0);
	inis.push_back(zero_ex);
	block->statements.push_back(new PlnStatement(new PlnVarInit(vars, inis), block));

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
		{
			auto cnt_ex1 = new PlnExpression(cnt_var);
			auto one_ex = new PlnExpression((uint64_t) 1);
			auto add = new PlnAddOperation(cnt_ex1, one_ex);
			vector<PlnExpression*> lvals, exps;
			auto cnt_ex2 = new PlnExpression(cnt_var);
			lvals.push_back(cnt_ex2); exps.push_back(add);
			auto inc = new PlnAssignment(lvals, exps);
			wblock->statements.push_back(new PlnStatement(inc, wblock));
		}

		auto whl = new PlnWhileStatement(cmp_op, wblock, block);

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

	if (item_type.back()->data_type != DT_OBJECT_REF)
		return;
	
	BOOST_ASSERT(item_type.back()->inf.obj.is_fixed_size);
	block = new PlnBlock();
	// if arr_var {
	{
		auto ifblock = new PlnBlock();
		auto if_arr = new PlnIfStatement(new PlnExpression(arr_var), ifblock, NULL, block);
		// {
		//  uint64 __cnt = 0;
		PlnVariable *cnt_var;
		{
			vector<PlnType*> tv;
			static string cnt_name = "__cnt";
			tv.push_back(PlnType::getSint());
			vector<PlnValue> vars;
			cnt_var = ifblock->declareVariable(cnt_name, tv);
			vars.push_back(cnt_var);

			vector<PlnExpression*> inis;
			auto zero_ex = new PlnExpression((uint64_t) 0);
			inis.push_back(zero_ex);
			ifblock->statements.push_back(new PlnStatement(new PlnVarInit(vars, inis), block));
		}

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
			// So if array item is moved to block variable, it will free when exit block.
			auto wblock = new PlnBlock();
			wblock->setParent(block);
			auto whl = new PlnWhileStatement(cmp_op, wblock, ifblock);

			// ? __item <<= __arr[__cnt];
			PlnVariable *item_var;
			{
				static string item_name = "__item";
				auto item_var_type = item_type;
				vector<PlnValue> vars;
				item_var = wblock->declareVariable(item_name, item_var_type);
				vars.push_back(item_var);
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
			}

			// __cnt+1 -> __cnt;
			{
				auto cnt_ex1 = new PlnExpression(cnt_var);
				auto one_ex = new PlnExpression((uint64_t) 1);
				auto add = new PlnAddOperation(cnt_ex1, one_ex);
				vector<PlnExpression*> lvals, exps;
				auto cnt_ex2 = new PlnExpression(cnt_var);
				lvals.push_back(cnt_ex2); exps.push_back(add);
				auto inc = new PlnAssignment(lvals, exps);
				wblock->statements.push_back(new PlnStatement(inc, wblock));
			}

			ifblock->statements.push_back(whl);
		}

		block->statements.push_back(if_arr);
	}
	// }
}

void PlnArrayHeapFreer::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	if (block) block->finish(da, si);
}

void PlnArrayHeapFreer::dump(ostream& os, string indent)
{
	os << indent << "ArrayHeapFreer" << endl;
}

void PlnArrayHeapFreer::gen(PlnGenerator& g)
{
	if (block)	block->gen(g);

	auto e = g.getEntity(arr_var->place);
	g.genMemFree(e.get(), arr_var->name, false);
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
