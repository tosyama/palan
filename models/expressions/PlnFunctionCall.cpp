/// Function call model class definition.
///
/// PlnFunctionCall call function with arguments
/// and get return values.
/// e.g. ) a = func(c, d);
///
/// @file	PlnFunctionCall.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>

#include "../../PlnModel.h"
#include "../PlnFunction.h"
#include "PlnFunctionCall.h"
#include "../PlnVariable.h"
#include "../PlnType.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"

using std::endl;

// PlnFunctionCall
PlnFunctionCall:: PlnFunctionCall(PlnFunction* f, vector<PlnExpression*>& args)
	: PlnExpression(ET_FUNCCALL),
	function(f),
	arguments(move(args))
{
	int i=0;
	for (auto rv: f->return_vals) {
		PlnVariable* ret_var = new PlnVariable();
		ret_var->name = rv->name;
		ret_var->inf.index = i;
		ret_var->var_type = rv->var_type;

		values.push_back(PlnValue(ret_var));
		++i;
	}
}

void PlnFunctionCall::finish(PlnDataAllocator& da)
{
	int i = 0;
	int func_type;
	switch (function->type) {
		case FT_PLN: func_type=DPF_PLN; break;
		case FT_C: func_type=DPF_C; break;
		case FT_SYS: func_type=DPF_SYS; break;
	}		
	auto dps = da.prepareArgDps(
		arguments.size(), function->parameters,
		function->return_vals, func_type);

	for (auto a: arguments) {
		a->data_places.push_back(dps[i]);
		a->finish(da);
		da.allocDp(dps[i]);

		++i;
	}
	da.funcCalled(dps, function->return_vals, func_type);
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
			for (auto arg: arguments)
				g.getPopEntity(arg->data_places[0]);

			g.genCCall(function->name);
			int i = 0;
			for (auto dp: data_places) {
				auto dst = g.getPushEntity(data_places[i]);
				auto src = g.getArgument(i, function->return_vals[i]->var_type->size);

				g.genMove(dst.get(), src.get(), string("ret -> ") + data_places[i]->cmt());

				++i;
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
