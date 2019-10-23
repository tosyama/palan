/// Function call model class definition.
///
/// PlnFunctionCall call function with arguments
/// and get return values.
/// e.g. ) funcion(a, b) -> c, d;
///
/// @file	PlnFunctionCall.cpp
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

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
	for (auto rv: f->return_vals) {
		PlnValue val;
		val.type = VL_WORK;
		val.inf.wk_type = rv->var_type2;
		val.is_readonly = rv->ptr_type & PTR_READONLY;
		val.is_cantfree = (rv->ptr_type & NO_PTR || !(rv->ptr_type & PTR_OWNERSHIP));
		
		values.push_back(val);
	}
}

PlnFunctionCall::PlnFunctionCall(PlnFunction* f, vector<PlnExpression*>& args, vector<PlnExpression*>& out_args)
	: PlnFunctionCall(f)
{
	arguments = move(args);
	out_arguments = move(out_args);
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
			BOOST_ASSERT(i<arg_dps.size());
			BOOST_ASSERT(p->var_type2->data_type == arg_data_types[i]);
			arg_dps[i]->data_type = arg_data_types[i];
			i++;
		}
	}
}

static vector<PlnDataPlace*> loadArgs(PlnDataAllocator& da, PlnScopeInfo& si,
	PlnFunction*f, vector<PlnExpression*> &args, vector<PlnExpression*> &out_args,
	vector<PlnClone*> &clones)
{
	vector<int> arg_dtypes = f->arg_dtypes;
	int out_start = f->num_in_param;
	if (f->has_va_arg) {
		// Add variable arguments data types.
		PlnParameter* va_arg = f->parameters.back();
		BOOST_ASSERT(va_arg->name == "...");
		bool is_output = va_arg->iomode & PIO_OUTPUT;
		vector<PlnExpression*> &target_args = is_output ?  out_args : args;
		int va_start = is_output ?  f->num_out_param : f->num_in_param;
		BOOST_ASSERT(va_start >= 0);
		int i=0;
		for (auto a: target_args) {
			for (auto v: a->values) {
				if (i >= va_start) {
					if (is_output)
						arg_dtypes.push_back(DT_OBJECT_REF);
					else
						arg_dtypes.push_back(v.getType()->data_type);
				}
				i++;
			}
		}
		if (!is_output)
			out_start += i - va_start;
	}

	auto tmp_arg_dps = da.prepareArgDps(f->type, f->ret_dtypes, arg_dtypes, false);

	vector<PlnDataPlace*> arg_dps(tmp_arg_dps.size());
	vector<int> ptr_types(arg_dps.size());
	{
		int i = 0;
		int ii = 0;
		int oi = out_start;
		for (auto p: f->parameters) {
			int xi;
			if (p->iomode & PIO_OUTPUT) {
				xi = oi; oi++;
			} else {
				xi = ii; ii++;
			}
			if (p->name == "...") {
				BOOST_ASSERT(p == f->parameters.back());
				for ( ;i<arg_dps.size(); i++) {
					arg_dps[xi] = tmp_arg_dps[i];
					switch (arg_dps[xi]->data_type) {
						case DT_OBJECT_REF:
							ptr_types[xi] = PTR_REFERENCE;
						default:
							ptr_types[xi] = NO_PTR;
					}
					xi++;
				}
				break;
			}

			arg_dps[xi] = tmp_arg_dps[i];
			ptr_types[xi] = f->parameters[i]->ptr_type;

			i++;
		}
	} 
 
	int i = 0;
	int j = 0;
	for (auto &a: args) {
		for (auto &v: a->values) {
			BOOST_ASSERT(arg_dps[i]->data_type != DT_UNKNOWN);

			if (ptr_types[i] == PTR_PARAM_MOVE && v.type == VL_VAR) {
				arg_dps[i]->do_clear_src = true;
			}

			PlnClone* clone = NULL;
			if (ptr_types[i] == PTR_PARAM_COPY) {
				BOOST_ASSERT(v.type == VL_LIT_ARRAY || v.type == VL_VAR || v.type == VL_WORK);
				if (v.type == VL_LIT_ARRAY) {
					a = v.inf.arrValue;
				}
				clone = new PlnClone(da, a, v.getType(), false);
			}

			int data_type = v.getType()->data_type;
			if (data_type != DT_OBJECT_REF && ptr_types[i]==PTR_REFERENCE) {
				BOOST_ASSERT(data_type == DT_SINT || data_type == DT_UINT || data_type == DT_FLOAT);
				BOOST_ASSERT(v.type == VL_VAR);
				arg_dps[i]->load_address = true;
			}

			if (clone)
				clone->finishAlloc(da, si);
			else
				a->data_places.push_back(arg_dps[i]);

			clones.push_back(clone);
			++i;
		}

		a->finish(da, si);

		for (auto v: a->values) {
			if (ptr_types[j] == PTR_PARAM_MOVE && v.type == VL_VAR) {
				auto var = v.inf.var;
				// Check if variable can write.
				if (!(var->ptr_type & PTR_OWNERSHIP)) {
					PlnCompileError err(E_CantUseMoveOwnership, var->name);
					err.loc = a->loc;
					throw err;
				}

				// Mark as freed variable.
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

	for (auto &a: out_args) {
		for (auto &v: a->values) {
			int data_type = v.getType()->data_type;
			if (data_type != DT_OBJECT_REF) {
				BOOST_ASSERT(data_type == DT_SINT || data_type == DT_UINT || data_type == DT_FLOAT);
				arg_dps[i]->load_address = true;
			}
			a->data_places.push_back(arg_dps[i]);
			++i;
		}
		a->finish(da, si);
	}

	return arg_dps;
}

void PlnFunctionCall::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	int func_type = function->type;

	if (arguments.size()+out_arguments.size()) 
		arg_dps = loadArgs(da, si, function, arguments, out_arguments, clones);

	for (auto dp: arg_dps)
		da.popSrc(dp);

	da.funcCalled(arg_dps, function->return_vals, func_type);

	ret_dps = da.prepareRetValDps(func_type, function->ret_dtypes, function->arg_dtypes, false);
	int i = 0;
	for (auto r: function->return_vals) {
		ret_dps[i]->data_type = r->var_type2->data_type;
		++i;
	}
	
	for (i=0; i<ret_dps.size(); ++i) {
		da.allocDp(ret_dps[i]);
		if (i >= data_places.size()) {
			if (function->return_vals[i]->ptr_type & PTR_OWNERSHIP) {
				PlnVariable *tmp_var = PlnVariable::createTempVar(da, function->return_vals[i]->var_type2, "ret" + std::to_string(i));
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

			for (auto arg: out_arguments) 
				arg->gen(g);

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
	f->addParam("size", PlnType::getSint(), PIO_INPUT, FPM_COPY, NULL);
	f->addRetValue(ret_name, PlnType::getObject(), false, false);
	internalFuncs[IFUNC_MALLOC] = f;

	f = new PlnFunction(FT_C, "__free");
	f->asm_name = "free";
	f->addParam("status", PlnType::getObject(), PIO_INPUT, FPM_REF, NULL);
	internalFuncs[IFUNC_FREE] = f;

	f = new PlnFunction(FT_C, "__exit");
	f->asm_name = "exit";
	f->addParam("status", PlnType::getSint(), PIO_INPUT, FPM_COPY, NULL);
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

