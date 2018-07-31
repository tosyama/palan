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

PlnChainCall::PlnChainCall(PlnFunction* f, vector<PlnExpression*> &in_args, vector<PlnExpression*> &args)
	: PlnExpression(ET_CHAINCALL), fcall(new PlnFunctionCall(f)), in_args(move(in_args)), args(move(args))
{
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
			a->data_places.push_back(fcall->arg_dps[i]);
			i++;
		}
		a->finish(da, si);
	}

	for (auto a: args) {
		a->data_places.push_back(fcall->arg_dps[i]);
		i++;
		a->finish(da, si);
	}

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
