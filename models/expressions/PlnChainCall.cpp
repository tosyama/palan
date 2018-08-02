/// Chain call model class definition.
///
/// PlnChainCall call function with assignment syntax.
/// e.g. ) a,b -> func(c) -> d;
///
/// @file	PlnChainCall.cpp
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "../PlnType.h"
#include "PlnChainCall.h"
#include "PlnFunctionCall.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnScopeStack.h"

PlnChainCall::PlnChainCall(PlnFunction* f, vector<PlnExpression*> &in_args, vector<PlnExpression*> &args)
	: PlnExpression(ET_CHAINCALL), fcall(new PlnFunctionCall(f)), in_args(move(in_args)), args(move(args))
{
	for (auto v: fcall->values)
		values.push_back(v);
}

void PlnChainCall::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	vector<int> arg_data_types;
	for (auto a: in_args) {
		for (auto v: a->values) {
			PlnType* t = v.getType();
			arg_data_types.push_back(t->data_type);
		}
	}

	for (auto a: args) {
		PlnType* t = a->values[0].getType();
		arg_data_types.push_back(t->data_type);
	}

	fcall->loadArgDps(da, arg_data_types);

	int i = 0;
	for (auto a: in_args) {
		for (auto v: a->values) {
			if (v.asgn_type == ASGN_MOVE && v.type == VL_VAR) {
				fcall->arg_dps[i]->do_clear_src = true;
			}
			a->data_places.push_back(fcall->arg_dps[i]);
			i++;
		}
		a->finish(da, si);

		for (auto v: a->values) {
			if (v.asgn_type == ASGN_MOVE && v.type == VL_VAR) {
				// Mark as freed variable.
				auto var = v.inf.var;
				if (si.exists_current(var))
					si.set_lifetime(var, VLT_FREED);
			}
		}
	}

	for (auto a: args) {
		a->data_places.push_back(fcall->arg_dps[i]);
		i++;
		a->finish(da, si);
	}

	for (auto dp: data_places)
		fcall->data_places.push_back(dp);

	fcall->finish(da, si);
}

void PlnChainCall::gen(PlnGenerator& g)
{
	for (auto a: in_args)
		a->gen(g);
	for (auto a: args)
		a->gen(g);

	fcall->gen(g);
}
