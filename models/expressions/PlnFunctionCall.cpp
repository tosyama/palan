/// Function call model class definition.
///
/// PlnFunctionCall call function with arguments
/// and get return values.
/// e.g. ) funcion(a, b) -> c, d;
///
/// @file	PlnFunctionCall.cpp
/// @copyright	2017-2018 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>

#include "PlnFunctionCall.h"
#include "../PlnFunction.h"
#include "../PlnVariable.h"
#include "../PlnType.h"
#include "../../PlnModel.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../../PlnConstants.h"
#include "../../PlnScopeStack.h"
#include "PlnClone.h"
#include "PlnArrayValue.h"

static PlnFunction* internalFuncs[IFUNC_NUM] = { NULL };
static bool is_init_ifunc = false;

static PlnExpression* createDefaultArg(PlnExpression* default_val)
{
	BOOST_ASSERT(default_val);

	if (default_val->type == ET_VALUE) {
		PlnValType vt = default_val->values[0].type;
		if (vt == VL_LIT_INT8 || vt == VL_LIT_UINT8 || vt == VL_LIT_FLO8 || vt == VL_LIT_STR) {
			return new PlnExpression(default_val->values[0]);
		}
	} else if (default_val->type == ET_ARRAYVALUE) {
		PlnArrayValue* av = new PlnArrayValue(*static_cast<PlnArrayValue*>(default_val));
		return av;
	}

	BOOST_ASSERT(false);
}

// PlnFunctionCall
PlnFunctionCall::PlnFunctionCall(PlnFunction* f)
	: PlnExpression(ET_FUNCCALL), function(f)
{
	for (auto rv: f->return_vals) {
		PlnValue val;
		val.type = VL_WORK;
		val.inf.wk_type = new vector<PlnType*>(rv->var_type);
		values.push_back(val);
	}
}

PlnFunctionCall::PlnFunctionCall(PlnFunction* f, vector<PlnExpression*>& args)
	: PlnFunctionCall(f)
{
	arguments = move(args);
	// Set dafault arguments if arg == NULL
	int i=0;
	for (auto &a: arguments) {
		if (!a) {
			BOOST_ASSERT(f->parameters[i]->dflt_value);
			a = createDefaultArg(f->parameters[i]->dflt_value);
		}
		i+=a->values.size();
	}

	while (i<f->parameters.size()) {
		arguments.push_back(new PlnExpression(*f->parameters[i]->dflt_value));
		i+= arguments.back()->values.size();
	}
}

PlnFunctionCall::~PlnFunctionCall()
{
	for (auto a: arguments)
		delete a;
	for (auto fv: free_vars)
		delete fv;
	for (auto fex: free_exs)
		delete fex;
}

void PlnFunctionCall::loadArgDps(PlnDataAllocator& da, vector<int> arg_data_types)
{
	PlnFunction* f = function;
	BOOST_ASSERT(!arguments.size());

	arg_dps = da.prepareArgDps(f->type, f->ret_dtypes, arg_data_types, false);
	if (f->parameters.size()) {
		BOOST_ASSERT(f->parameters.size() == arg_data_types.size());
		int i=0;
		for (auto p: f->parameters) {
			BOOST_ASSERT(p->var_type.back()->data_type == arg_data_types[i]);
			arg_dps[i]->data_type = arg_data_types[i];
			i++;
		}
	}
}

static vector<PlnDataPlace*> loadArgs(PlnDataAllocator& da, PlnScopeInfo& si,
	PlnFunction*f, vector<PlnExpression*> &args, vector<PlnClone*> &clones)
{
	vector<int> arg_dtypes;
	int i=0;
	for (auto a: args) {
		for (auto v: a->values) {
			if (i < f->arg_dtypes.size()) {
				arg_dtypes.push_back(f->arg_dtypes[i]);
			} else {
				arg_dtypes.push_back(v.getType()->data_type);
			}
		}
	}

	auto arg_dps = da.prepareArgDps(f->type, f->ret_dtypes, arg_dtypes, false);

	i = 0;
	for (auto p: f->parameters) {
		arg_dps[i]->data_type = p->var_type.back()->data_type;
		++i;
	}

	i = 0;
	int j = 0;
	for (auto &a: args) {
		if (i<f->parameters.size())
			a->adjustType(f->parameters[i]->var_type);

		for (auto v: a->values) {
			auto t = v.getType();
			int ptr_type = (f->parameters.size()>i) ? f->parameters[i]->ptr_type : NO_PTR;

			// in the case declaration parameters omited or variable length parameter
			if (arg_dps[i]->data_type == DT_UNKNOWN) {
				arg_dps[i]->data_type = t->data_type;
			}

			if (ptr_type == PTR_PARAM_MOVE && v.type == VL_VAR) {
				arg_dps[i]->do_clear_src = true;
			}

			PlnClone* clone = NULL;
			if (ptr_type == PTR_PARAM_COPY && v.type == VL_VAR) {
				clone = new PlnClone(da, v.inf.var->var_type, false);
				a->data_places.push_back(clone->src_dp);
			} else if (ptr_type == PTR_PARAM_COPY && v.type == VL_WORK) {
				clone = new PlnClone(da, *v.inf.wk_type, false);
				a->data_places.push_back(clone->src_dp);
			} else {
				a->data_places.push_back(arg_dps[i]);
			}
			clones.push_back(clone);

			++i;
		}

		a->finish(da, si);

		for (auto v: a->values) {
			int ptr_type = (f->parameters.size()>j) ? f->parameters[j]->ptr_type : NO_PTR;
			if (ptr_type == PTR_PARAM_MOVE && v.type == VL_VAR) {
				// Mark as freed variable.
				auto var = v.inf.var;
				if (!si.exists_current(var))
					si.push_owner_var(var);
				si.set_lifetime(var, VLT_FREED);
			}
			if (clones[j]) {
				clones[j]->data_places.push_back(arg_dps[j]);
				clones[j]->finish(da, si);
			}

			++j;
		}
	}

	return arg_dps;
}

void PlnFunctionCall::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	int func_type = function->type;

	if (arguments.size()) 
		arg_dps = loadArgs(da, si, function, arguments, clones);

	for (auto dp: arg_dps)
		da.popSrc(dp);

	da.funcCalled(arg_dps, function->return_vals, func_type);

	ret_dps = da.prepareRetValDps(func_type, function->ret_dtypes, function->arg_dtypes, false);
	int i = 0;
	for (auto r: function->return_vals) {
		ret_dps[i]->data_type = r->var_type.back()->data_type;
		++i;
	}
	
	for (i=0; i<ret_dps.size(); ++i) {
		da.allocDp(ret_dps[i]);
		if (i >= data_places.size()) {
			if (function->return_vals[i]->ptr_type & PTR_OWNERSHIP) {
				PlnVariable *tmp_var = PlnVariable::createTempVar(da, function->return_vals[i]->var_type, "ret" + std::to_string(i));
				PlnExpression *free_ex = PlnFreer::getFreeEx(tmp_var);

				free_vars.push_back(tmp_var);
				free_exs.push_back(free_ex);
				
				da.pushSrc(tmp_var->place, ret_dps[i]);
			} else {
				da.releaseDp(ret_dps[i]);
			}
		}
	}

	i=0;
	for (auto dp: data_places) {
		da.pushSrc(dp, ret_dps[i]);
		i++;
	}
	for (auto free_var: free_vars) {
		da.popSrc(free_var->place);
	}

	for (auto free_ex: free_exs)
		free_ex->finish(da, si);

	for (auto free_var: free_vars)
		da.releaseDp(free_var->place);

}

void PlnFunctionCall::gen(PlnGenerator &g)
{
	switch (function->type) {
		case FT_PLN:
		{
			int i=0;
			for (auto arg: arguments) {
				arg->gen(g);
				if (clones[i]) clones[i]->gen(g);
				i++;
			}

			for (auto dp: arg_dps)
				g.genLoadDp(dp, false);

			for (auto dp: arg_dps)
				g.genSaveDp(dp);

			vector<int> arg_dtypes;
			for (auto dp: arg_dps)
				arg_dtypes.push_back(dp->data_type);

			g.genCCall(function->asm_name, arg_dtypes, -1);

			for (auto dp: data_places) 
				g.genSaveSrc(dp);

			for (auto free_var: free_vars)
				g.genLoadDp(free_var->place);

			for (auto free_ex: free_exs)
				free_ex->gen(g);

			break;
		}
		case FT_SYS:
		{
			for (auto arg: arguments) 
				arg->gen(g);

			for (auto dp: arg_dps)
				g.genLoadDp(dp, false);

			for (auto dp: arg_dps)
				g.genSaveDp(dp);

			g.genSysCall(function->inf.syscall.id, function->asm_name);

			break;
		}
		case FT_C:
		{
			for (auto arg: arguments) 
				arg->gen(g);

			for (auto dp: arg_dps)
				g.genLoadDp(dp, false);

			for (auto dp: arg_dps)
				g.genSaveDp(dp);

			vector<int> arg_dtypes;
			for (auto dp: arg_dps)
				arg_dtypes.push_back(dp->data_type);

			g.genCCall(function->asm_name, arg_dtypes, 0);

			for (auto dp: data_places) 
				g.genSaveSrc(dp);

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
	f->asm_name = f->name;
	vector<PlnType*> ret_type = { PlnType::getObject() };
	f->addRetValue(ret_name, ret_type, false);
	internalFuncs[IFUNC_MALLOC] = f;

	f = new PlnFunction(FT_C, "free");
	f->asm_name = f->name;
	internalFuncs[IFUNC_FREE] = f;

	f = new PlnFunction(FT_C, "exit");
	f->asm_name = f->name;
	internalFuncs[IFUNC_EXIT] = f;
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

