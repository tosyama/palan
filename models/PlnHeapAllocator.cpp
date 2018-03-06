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
	arr_var->var_type.push_back(PlnType::getObject());
	arr_var->var_type.push_back(PlnType::getRawArray());
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

		// alloced address -> __arr[__cnt];
		{
			PlnHeapAllocator* h_alloc = createHeapAllocation(item_type);
			BOOST_ASSERT(h_alloc);

			vector<PlnExpression*> lvals, exps, inds;
			auto arr_ex = new PlnExpression(arr_var);
			auto cnt_ex = new PlnExpression(cnt_var);
			inds.push_back(cnt_ex);
			auto arr_item = new PlnArrayItem(arr_ex, inds);
			auto ten_ex = new PlnExpression((uint64_t) 10);

			lvals.push_back(arr_item); exps.push_back(h_alloc);
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
		arr_var->place = da.allocData(DT_OBJECT_REF, 8);
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

 PlnHeapAllocator* PlnHeapAllocator::createHeapAllocation(vector<PlnType*> &var_type)
{
	PlnType* vt = var_type.back();
	if (vt->name == "[]") {
		return new PlnArrayHeapAllocator(var_type);
	}

	return NULL;	
}
