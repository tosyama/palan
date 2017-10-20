/// Function call model class definition.
///
/// PlnFunctionCall call function with arguments
/// and get return values.
/// e.g. ) a = func(c, d);
///
/// @file	PlnFunctionCall.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>

#include "../../PlnConstants.h"
#include "../../PlnModel.h"
#include "../PlnFunction.h"
#include "PlnFunctionCall.h"
#include "../PlnVariable.h"
#include "../PlnType.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "PlnClone.h"

using std::endl;

// PlnFunctionCall
PlnFunctionCall:: PlnFunctionCall(PlnFunction* f, vector<PlnExpression*>& args)
	: PlnExpression(ET_FUNCCALL),
	function(f),
	arguments(move(args))
{
	// arg == void
	if (arguments.size() == 1 && arguments[0]==NULL && f->parameters.size() == 0) 
			arguments.pop_back();

	// TODO: set dafault arguments if arg == NULL

	int i=0;
	for (auto rv: f->return_vals) {
		PlnVariable* ret_var = new PlnVariable();
		ret_var->name = rv->name;
		ret_var->inf.index = i;
		ret_var->var_type = rv->var_type;

		values.push_back(PlnValue(ret_var));
		++i;
	}

	// insert clone expression if needed.
	for (i=0; i<arguments.size(); ++i) {
		if (i < f->parameters.size()) {
			if (f->parameters[i]->ptr_type & PTR_CLONE) {
				auto clone_ex = new PlnClone(arguments[i]);
				arguments[i] = clone_ex;
			}
		}
	}
}

void PlnFunctionCall::finish(PlnDataAllocator& da)
{
	int func_type = function->type;
	arg_dps = da.prepareArgDps(function->return_vals.size(), arguments.size(), func_type, false);

	int i = 0;
	for (auto p: function->parameters) {
		arg_dps[i]->data_type = p->var_type.back()->data_type;
		++i;
	}

	i = 0;
	for (auto a: arguments) {
		auto t = a->values[0].getType();
		// in the case declaration parameters omited or variable length parameter
		if (arg_dps[i]->data_type == DT_UNKNOWN) {
			arg_dps[i]->data_type = t->data_type;
		}

		a->data_places.push_back(arg_dps[i]);
		a->finish(da);

		da.allocDp(arg_dps[i]);
		++i;
	}
	da.funcCalled(arg_dps, function->return_vals, func_type);

	auto rdps = da.prepareRetValDps(function->return_vals.size(), func_type, false);
	i = 0;
	for (auto r: function->return_vals) {
		rdps[i]->data_type = r->var_type.back()->data_type;
		++i;
	}
	
	for (i=0; i<rdps.size(); ++i) {
		if (i < data_places.size()) {
			BOOST_ASSERT(data_places[i]->save_place == NULL);
			rdps[i]->alloc_step = data_places[i]->alloc_step;
			data_places[i]->save_place = rdps[i];
		} else {
			if (function->return_vals[i]->ptr_type & PTR_OWNERSHIP)
				free_dps.push_back(rdps[i]);
			else
				delete rdps[i];

		}
	}
	if (free_dps.size()) da.memFreed();
}

void PlnFunctionCall:: dump(ostream& os, string indent)
{
	os << indent << "FunctionCall: " << function->name << endl;
	os << indent << " Arguments: " << arguments.size() << endl;
	for (auto a: arguments) {
		if (a) a->dump(os, indent + "  ");
		else os << indent + "  NULL" << endl;
	}
}

void PlnFunctionCall::gen(PlnGenerator &g)
{
	switch (function->type) {
		case FT_PLN:
		{
			for (auto arg: arguments)
				arg->gen(g);

			for (auto dp: arg_dps)
				g.getPopEntity(dp);

			g.genCCall(function->name);
			for (auto dp: data_places)
				g.getPopEntity(dp);

			for (auto fdp: free_dps) {
				auto fe = g.getPopEntity(fdp);
				static string cmt="unused return";
				g.genMemFree(fe.get(), cmt, false);
			}

			break;
		}
		case FT_SYS:
		{
			for (auto arg: arguments) 
				arg->gen(g);
			for (auto arg: arguments)
				g.getPopEntity(arg->data_places[0]);
			g.genSysCall(function->inf.syscall.id, function->name);
			break;
		}
		case FT_C:
		{
			for (auto arg: arguments) 
				arg->gen(g);
			for (auto arg: arguments)
				g.getPopEntity(arg->data_places[0]);
			g.genCCall(function->name);
			break;
		}
		default:
			BOOST_ASSERT(false);
	}
}
