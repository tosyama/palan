/// Chain call model class definition.
///
/// PlnChainCall call function with assignment syntax.
/// e.g. ) a,b -> func(c) -> d;
///
/// @file	PlnChainCall.cpp
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "../PlnType.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnScopeStack.h"
#include "../../PlnGenerator.h"
#include "../PlnVariable.h"
#include "../PlnFunction.h"
#include "PlnChainCall.h"
#include "PlnFunctionCall.h"

class PlnCloneArg
{
	PlnVariable* var;
	PlnExpression *alloc_ex;
	PlnDataPlace* copy_dst_dp;
	PlnDeepCopyExpression *copy_ex;

public:
	PlnDataPlace *src_dp, *data_place;

	PlnCloneArg(PlnDataAllocator& da, vector<PlnType*> &var_type) {
		var = PlnVariable::createTempVar(da, var_type, "(clone)");
		alloc_ex = var_type.back()->allocator->getAllocEx();
		alloc_ex->data_places.push_back(var->place);

		copy_ex = var_type.back()->copyer->getCopyEx();
		src_dp = copy_ex->srcDp(da);
		copy_dst_dp = copy_ex->dstDp(da);
		data_place = NULL;
	}

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) {
		BOOST_ASSERT(data_place);
		alloc_ex->finish(da, si);
		da.popSrc(var->place);

		da.pushSrc(copy_dst_dp, var->place, false);
		copy_ex->finish(da, si);
		da.pushSrc(data_place, var->place);
	}

	void gen(PlnGenerator& g) {
		alloc_ex->gen(g);
		g.genLoadDp(var->place);
		g.genSaveSrc(copy_dst_dp);
		copy_ex->gen(g);
		g.genSaveSrc(data_place);
	}

};

PlnChainCall::PlnChainCall(PlnFunction* f, vector<PlnExpression*> &in_args, vector<PlnExpression*> &args)
	: PlnExpression(ET_CHAINCALL), fcall(new PlnFunctionCall(f)), args(move(in_args))
{
	for (auto v: fcall->values)
		values.push_back(v);
	
	for (auto a: args)
		this->args.push_back(a);

}

void PlnChainCall::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	vector<int> arg_data_types;
	for (auto a: args) {
		for (auto v: a->values) {
			PlnType* t = v.getType();
			arg_data_types.push_back(t->data_type);
		}
	}

	fcall->loadArgDps(da, arg_data_types);

	int i = 0, j = 0;
	for (auto a: args) {
		for (auto v: a->values) {
			PlnParameter* param = fcall->function->parameters[i];
			if (param->ptr_type == PTR_PARAM_MOVE  && v.type == VL_VAR) {
				fcall->arg_dps[i]->do_clear_src = true;
			}

			PlnCloneArg* clone = NULL;
			if (param->ptr_type == PTR_PARAM_COPY && v.type == VL_VAR) {
				clone = new PlnCloneArg(da, v.inf.var->var_type);
				a->data_places.push_back(clone->src_dp);
			} else {
				a->data_places.push_back(fcall->arg_dps[i]);
			}
			clones.push_back(clone);
			i++;
		}

		a->finish(da, si);

		for (auto v: a->values) {
			PlnParameter* param = fcall->function->parameters[j];
			if (param->ptr_type == PTR_PARAM_MOVE && v.type == VL_VAR) {
				// Mark as freed variable.
				auto var = v.inf.var;
				if (si.exists_current(var))
					si.set_lifetime(var, VLT_FREED);
			}

			if (clones[j]) {
				clones[j]->data_place = fcall->arg_dps[j];
				clones[j]->finish(da, si);
			}

			j++;
		}
	}

	for (auto dp: data_places)
		fcall->data_places.push_back(dp);

	fcall->finish(da, si);
}

void PlnChainCall::gen(PlnGenerator& g)
{
	int i=0;
	for (auto a: args) {
		a->gen(g);
		for (auto v: a->values) {
			if (clones[i]) clones[i]->gen(g);
			i++;
		}
	}

	fcall->gen(g);
}
