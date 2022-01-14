/// Function call model class definition.
///
/// PlnFunctionCall call function with arguments
/// and get return values.
/// e.g. ) funcion(a, b) -> c, d;
///
/// @file	PlnFunctionCall.cpp
/// @copyright	2017-2021 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>

#include "../../PlnConstants.h"
#include "PlnFunctionCall.h"
#include "../PlnFunction.h"
#include "../PlnVariable.h"
#include "../PlnType.h"
#include "../../PlnModel.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
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

void PlnFunctionCall::loadArgDps(PlnDataAllocator& da)
{
	PlnFunction* f = function;
	BOOST_ASSERT(!arguments.size());
	
	arg_dps = f->createArgDps();
	da.setArgDps(f->type, arg_dps, false);
}

static vector<PlnDataPlace*> loadArgs(PlnFunctionCall *fcall, PlnDataAllocator& da, PlnScopeInfo& si)
{
	auto f = fcall->function;
	auto& clones = fcall->clones;
	auto arg_dps = f->createArgDps();

	if (f->has_va_arg) {
		// Add variable arguments data types.
		for (auto &arg: fcall->arguments) {
			int i=0;
			for (auto& inf: arg.inf) {
				if (inf.param->passby == FPM_IN_VARIADIC) {
					PlnDataPlace* dp = new PlnDataPlace(8, arg.exp->getDataType(i));
					dp->status = DS_READY_ASSIGN;
					arg_dps.push_back(dp);

				} else if (inf.param->passby == FPM_OUT_VARIADIC) {
					PlnDataPlace* dp = new PlnDataPlace(8, DT_OBJECT_REF);
					dp->status = DS_READY_ASSIGN;
					arg_dps.push_back(dp);
				}
				i++;
			}
		}
	}

	da.setArgDps(f->type, arg_dps, false);

	int j = 0;
	for (auto &arg: fcall->arguments) {
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
			if (argval.param->passby == FPM_IN_BYREF_CLONE) {
				BOOST_ASSERT(v.type == VL_LIT_ARRAY || v.type == VL_VAR || v.type == VL_WORK || v.type == VL_LIT_STR);
				if (v.type == VL_LIT_ARRAY) {
					PlnExpression* tmp = v.inf.arrValue;
					v.inf.arrValue = NULL;	 // == arg.exp->values[vi].inf.arrValue;
					delete arg.exp;
					arg.exp = tmp;
				}

				if (v.type == VL_WORK) {
					if (v.inf.wk_type->mode[ALLOC_MD] != 'h') {
						clone = new PlnClone(da, arg.exp, argval.param->var->var_type, false);
					}
				} else {
					clone = new PlnClone(da, arg.exp, argval.param->var->var_type, false);
				}

			} else if (argval.param->passby == FPM_IN_BYREF) {
				// reference paramater
				if (v.type == VL_WORK && v.inf.wk_type->mode[ALLOC_MD]=='h') { // e.g. return value of function
					BOOST_ASSERT(v.inf.wk_type->mode[ALLOC_MD]=='h');
					// needs to free after call func.
					PlnVariable *tmp_var = PlnVariable::createTempVar(da, argval.param->var->var_type, "free_var");
					da.pushSrc(tmp_var->place, arg_dps[dp_i], false);
					PlnExpression *free_ex = PlnFreer::getFreeEx(tmp_var);

					fcall->free_work_vars.push_back(tmp_var);
					fcall->free_exs.push_back(free_ex);
				}

			}

			if ((argval.param->passby == FPM_IN_BYREF && arg.exp->getDataType(vi) != DT_OBJECT_REF)
				|| (argval.param->passby == FPM_OUT_BYREF && arg.exp->getDataType(vi) != DT_OBJECT_REF)
				|| (argval.param->passby == FPM_OUT_VARIADIC && arg.exp->getDataType(vi) != DT_OBJECT_REF)
				|| (argval.param->passby == FPM_OUT_BYREFADDR_GETOWNER)
				|| (argval.param->passby == FPM_OUT_BYREFADDR)) {
				arg_dps[dp_i]->load_address = true;
			}

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
		arg_dps = loadArgs(this, da, si);

	for (auto dp: arg_dps)
		da.popSrc(dp);

	for (auto free_work_var: free_work_vars)
		da.popSrc(free_work_var->place);

	da.funcCalled(arg_dps, func_type, function->never_return);

	ret_dps = function->createRetValDps();
	da.setRetValDps(function->type, ret_dps, false);

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

	for (auto free_work_var: free_work_vars)
		da.releaseDp(free_work_var->place);

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

			for (auto free_work_var: free_work_vars)
				g.genLoadDp(free_work_var->place);

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
			int i=0;
			for (auto arg: arguments) {
				if (clones[i]) clones[i]->genAlloc(g);
				arg.exp->gen(g);
				if (clones[i]) clones[i]->gen(g);
				i++;
			}
			//for (auto& arg: arguments)
			//	arg.exp->gen(g);

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
	f->addParam("size", PlnVarType::getSint(), PIO_INPUT, FPM_IN_BYVAL, NULL);
	f->addRetValue(ret_name, PlnVarType::getObject());
	internalFuncs[IFUNC_MALLOC] = f;

	f = new PlnFunction(FT_C, "__free");
	f->asm_name = "free";
	f->addParam("ptr", PlnVarType::getObject(), PIO_INPUT, FPM_IN_BYREF, NULL);
	internalFuncs[IFUNC_FREE] = f;

	f = new PlnFunction(FT_C, "__exit");
	f->asm_name = "exit";
	f->addParam("status", PlnVarType::getSint(), PIO_INPUT, FPM_IN_BYVAL, NULL);
	f->never_return = true;
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

