/// Function call model class definition.
///
/// PlnFunctionCall call function with arguments
/// and get return values.
/// e.g. ) funcion(a, b) -> c, d;
///
/// @file	PlnFunctionCall.cpp
/// @copyright	2017-2020 YAMAGUCHI Toshinobu 

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
#include "../../PlnMessage.h"
#include "../../PlnException.h"

static PlnFunction* internalFuncs[IFUNC_NUM] = { NULL };
static bool is_init_ifunc = false;

// PlnFunctionCall
PlnFunctionCall::PlnFunctionCall(PlnFunction* f)
	: PlnExpression(ET_FUNCCALL), function(f)
{
	for (auto& rv: f->return_vals) {
		PlnValue val;
		val.type = VL_WORK;
		val.inf.wk_type = rv.var_type;
		values.push_back(val);
	}
}

PlnFunctionCall::PlnFunctionCall(PlnFunction* f, vector<PlnArgument>& args)
	: PlnFunctionCall(f)
{
	arguments = move(args);
}

PlnFunctionCall::PlnFunctionCall(PlnFunction* f, vector<PlnExpression*>& args)
	: PlnFunctionCall(f)
{
	vector<PlnParameter*> &iparams = f->parameters;
	BOOST_ASSERT(args.size() == iparams.size());

	int p_ind = 0;
	for (auto arg_ex: args) {
		BOOST_ASSERT(arg_ex->values.size() == 1);
		arguments.push_back({arg_ex});
		auto &arg = arguments.back();
		arg.inf.push_back({iparams[p_ind], PIO_INPUT});
		p_ind++;
	}
}

PlnFunctionCall::~PlnFunctionCall()
{
	for (auto a: arguments)
		delete a.exp;
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
			BOOST_ASSERT(i<arg_dps.size());
			BOOST_ASSERT(p->var->var_type->data_type() == arg_data_types[i]);
			arg_dps[i]->data_type = arg_data_types[i];
			i++;
		}
	}
}

static vector<PlnDataPlace*> loadArgs(PlnDataAllocator& da, PlnScopeInfo& si,
	PlnFunction*f, vector<PlnArgument> &args, vector<PlnClone*> &clones)
{
	vector<int> arg_dtypes = f->arg_dtypes;

	if (f->has_va_arg) {
		// Add variable arguments data types.
		for (auto &arg: args) {
			int i=0;
			for (auto& inf: arg.inf) {
				if (inf.param->var->name == "...") {
					if (inf.param->iomode & PIO_OUTPUT)
						arg_dtypes.push_back(DT_OBJECT_REF);
					else {
						arg_dtypes.push_back(arg.exp->getDataType(i));
					}
				}
				i++;
			}
		}
	}

	auto arg_dps = da.prepareArgDps(f->type, f->ret_dtypes, arg_dtypes, false);

	int j = 0;
	for (auto &arg: args) {
		int vi = 0;
		for (auto &v: arg.exp->values) {
			PlnArgValueInf& argval = arg.inf[vi];
			int dp_i = argval.param->index;
			if (argval.va_idx >= 0)
				dp_i += argval.va_idx;
			BOOST_ASSERT(arg_dps[dp_i]->data_type != DT_UNKNOWN);


			if (argval.opt == AG_MOVE && v.type == VL_VAR) {
				arg_dps[dp_i]->do_clear_src = true;
			} 

			PlnClone* clone = NULL;
			if (argval.param->iomode == PIO_INPUT
					&& argval.param->var->var_type->mode[ALLOC_MD]=='h'
					&& argval.opt == AG_NONE) {
				BOOST_ASSERT(v.type == VL_LIT_ARRAY || v.type == VL_VAR || v.type == VL_WORK);
				if (v.type == VL_LIT_ARRAY) {
					arg.exp = v.inf.arrValue;
					// TODO?: delete arg.exp before assgin?
				}
				clone = new PlnClone(da, arg.exp, v.getVarType(), false);
			}

			auto pvar_type = argval.param->var->var_type;
			if (argval.param->passby == FPM_VAR_REF
				|| (argval.param->passby == FPM_ANY_OUT && arg.exp->getDataType(vi) != DT_OBJECT_REF)) {
				arg_dps[dp_i]->load_address = true;
			} 
			/*if (argval.param->iomode == PIO_INPUT) {
				if (pvar_type->mode[ALLOC_MD]=='r' && arg.exp->getDataType(vi) != DT_OBJECT_REF) {
					arg_dps[dp_i]->load_address = true;
				}
			} else if (argval.param->iomode == PIO_OUTPUT) {
				if (arg.exp->getDataType(vi) != DT_OBJECT_REF) {
					arg_dps[dp_i]->load_address = true;
				}
			} else
				BOOST_ASSERT(false);
			*/

			if (clone)
				clone->finishAlloc(da, si);
			else
				arg.exp->data_places.push_back(arg_dps[dp_i]);

			clones.push_back(clone);

			vi++;
		}

		arg.exp->finish(da, si);

		vi = 0;
		for (auto &v: arg.exp->values) {
			PlnArgValueInf& argval = arg.inf[vi];
			int dp_i = argval.param->index;
			if (argval.va_idx >= 0)
				dp_i += argval.va_idx;

			if (arg.inf[vi].opt == AG_MOVE && v.type == VL_VAR) {
				auto var = v.inf.var;
				// Check if variable can write.
				if (var->var_type->mode[IDENTITY_MD] != 'm') {
					PlnCompileError err(E_CantUseMoveOwnershipFrom, var->name);
					err.loc = arg.exp->loc;
					throw err;
				}

				// Mark as freed variable.
				if (!si.exists_current(var))
					si.push_owner_var(var);
				si.set_lifetime(var, VLT_FREED);
			}
			vi++;

			if (clones[j]) {
				clones[j]->data_places.push_back(arg_dps[dp_i]);
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

	da.funcCalled(arg_dps, func_type);

	ret_dps = da.prepareRetValDps(func_type, function->ret_dtypes, function->arg_dtypes, false);
	int i = 0;
	for (auto r: function->return_vals) {
		ret_dps[i]->data_type = r.local_var->var_type->data_type();
		++i;
	}
	
	for (i=0; i<ret_dps.size(); ++i) {
		da.allocDp(ret_dps[i]);
		if (i >= data_places.size()) {
			if (function->return_vals[i].var_type->mode[ALLOC_MD] == 'h') {
				PlnVariable *tmp_var = PlnVariable::createTempVar(da, function->return_vals[i].local_var->var_type, "ret" + std::to_string(i));
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
	function->call_count++;
	switch (function->type) {
		case FT_PLN:
		{
			int i=0;
			for (auto arg: arguments) {
				if (clones[i]) clones[i]->genAlloc(g);
				arg.exp->gen(g);
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

			g.genCCall(function->asm_name, arg_dtypes, function->has_va_arg);

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
				arg.exp->gen(g);

			for (auto dp: arg_dps)
				g.genLoadDp(dp, false);

			for (auto dp: arg_dps)
				g.genSaveDp(dp);

			g.genSysCall(function->inf.syscall.id, function->asm_name);

			break;
		}
		case FT_C:
		{
			for (auto& arg: arguments)
				arg.exp->gen(g);

			for (auto dp: arg_dps)
				g.genLoadDp(dp, false);

			for (auto dp: arg_dps)
				g.genSaveDp(dp);

			vector<int> arg_dtypes;
			for (auto dp: arg_dps)
				arg_dtypes.push_back(dp->data_type);

			g.genCCall(function->asm_name, arg_dtypes, function->has_va_arg);

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

	f = new PlnFunction(FT_C, "__malloc");
	f->asm_name = "malloc";
	f->addParam("size", PlnType::getSint()->getVarType(), PIO_INPUT, FPM_VAR_COPY, NULL);
	f->addRetValue(ret_name, PlnType::getObject()->getVarType());
	internalFuncs[IFUNC_MALLOC] = f;

	f = new PlnFunction(FT_C, "__free");
	f->asm_name = "free";
	f->addParam("ptr", PlnType::getObject()->getVarType(), PIO_INPUT, FPM_VAR_COPY, NULL);
	internalFuncs[IFUNC_FREE] = f;

	f = new PlnFunction(FT_C, "__exit");
	f->asm_name = "exit";
	f->addParam("status", PlnType::getSint()->getVarType(), PIO_INPUT, FPM_VAR_COPY, NULL);
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

