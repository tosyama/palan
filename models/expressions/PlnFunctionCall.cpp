/// Function call model class definition.
///
/// PlnFunctionCall call function with arguments
/// and get return values.
/// e.g. ) a = func(c, d);
///
/// @file	PlnFunctionCall.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>

#include "PlnFunctionCall.h"
#include "../PlnFunction.h"
#include "../PlnVariable.h"
#include "../PlnType.h"
#include "../../PlnModel.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../../PlnConstants.h"
#include "PlnClone.h"

using std::endl;

static PlnFunction* internalFuncs[IFUNC_NUM] = { NULL };
static bool is_init_ifunc = false;

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
		PlnValue val;
		val.type = VL_WORK;
		val.inf.wk_type = rv->var_type.back();
		values.push_back(val);
	}

	// insert clone/move owner expression if needed.
	for (i=0; i<arguments.size(); ++i) {
		if (i < f->parameters.size()) {
			int ptr_type = f->parameters[i]->ptr_type;
			if (ptr_type & PTR_CLONE) {
				auto clone_ex = new PlnClone(arguments[i]);
				arguments[i] = clone_ex;
			}
		}
	}
}

void PlnFunctionCall::finish(PlnDataAllocator& da, PlnScopeInfo& si)
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
		a->finish(da, si);

		++i;
	}

	for (auto dp: arg_dps)
		da.popSrc(dp);

	da.funcCalled(arg_dps, function->return_vals, func_type);

	ret_dps = da.prepareRetValDps(function->return_vals.size(), func_type, false);
	i = 0;
	for (auto r: function->return_vals) {
		ret_dps[i]->data_type = r->var_type.back()->data_type;
		++i;
	}
	
	for (i=0; i<ret_dps.size(); ++i) {
		da.allocDp(ret_dps[i]);
		da.releaseData(ret_dps[i]);
		if (i >= data_places.size()) {
			if (function->return_vals[i]->ptr_type & PTR_OWNERSHIP)
				free_dps.push_back(ret_dps[i]);
		}
	}

	i=0;
	for (auto dp: data_places) {
		da.pushSrc(dp, ret_dps[i]);
		i++;
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

			vector<unique_ptr<PlnGenEntity>> clr_es;
			for (auto arg: arguments) {
				auto dp = arg->data_places[0];
				g.genLoadDp(dp);
				if (arg->values[0].lval_type == LVL_MOVE)
					clr_es.push_back(g.getEntity(dp->src_place));
			}
			if (clr_es.size())
				g.genNullClear(clr_es);

			g.genCCall(function->name);
			for (auto dp: data_places) 
				g.genSaveSrc(dp);

			for (auto fdp: free_dps) {
				auto fe = g.getEntity(fdp);
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
				g.genLoadDp(arg->data_places[0]);

			g.genSysCall(function->inf.syscall.id, function->name);
			break;
		}
		case FT_C:
		{
			for (auto arg: arguments) 
				arg->gen(g);
			for (auto arg: arguments)
				g.genLoadDp(arg->data_places[0]);

			g.genCCall(function->name);
			break;
		}
		default:
			BOOST_ASSERT(false);
	}
}

static void initInternalFunctions()
{
	PlnFunction* f;
	string ret_name = "";

	f = new PlnFunction(FT_C, "malloc");
	vector<PlnType*> ret_type = { PlnType::getObject() };
	f->addRetValue(ret_name, &ret_type);
	internalFuncs[IFUNC_MALLOC] = f;

	f = new PlnFunction(FT_C, "free");
	internalFuncs[IFUNC_FREE] = f;
}

PlnFunction* PlnFunctionCall::getInternalFunc(PlnInternalFuncType func_type)
{
	BOOST_ASSERT(func_type < IFUNC_NUM);
	if (!is_init_ifunc) {
		initInternalFunctions();
		is_init_ifunc = true;
	}

	return internalFuncs[func_type];
}

