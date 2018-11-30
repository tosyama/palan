/// Function model class definition.
///
/// Function model mainly manage return values
/// and paramater definition.
/// e.g.) int ret1, int ret2 funcname(int arg1, int arg2) { ... }
///
/// @file	PlnFunction.cpp
/// @copyright	2017-2018 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <functional>
#include "PlnFunction.h"
#include "PlnModule.h"
#include "PlnBlock.h"
#include "PlnStatement.h"
#include "PlnType.h"
#include "PlnVariable.h"
#include "../PlnDataAllocator.h"
#include "../PlnGenerator.h"
#include "../PlnConstants.h"
#include "../PlnScopeStack.h"
#include "../PlnMessage.h"
#include "../PlnException.h"

using std::string;
using std::endl;
using std::to_string;

PlnFunction::PlnFunction(int func_type, const string &func_name)
	:	type(func_type), name(func_name), retval_init(NULL), implement(NULL),
		parent(NULL)
{
}

PlnVariable* PlnFunction::addRetValue(const string& rname, vector<PlnType*> &rtype, bool do_init)
{
	for (auto r: return_vals)
		if (r->name != "" && r->name == rname) return NULL;

	for (auto p: parameters)
		if (p->name == rname) {
			if (!rtype.size()) 
				rtype = return_vals.back()->var_type;
			if (p->var_type.size() != rtype.size())
				return NULL;

			for (int i=0; i<rtype.size(); i++)
				if (p->var_type[i] != rtype[i])
					return NULL;
			p->param_type = PRT_PARAM | PRT_RETVAL;

			return_vals.push_back(p);
			ret_dtypes.push_back(p->var_type.back()->data_type);

			return p;
		}

	auto ret_var = new PlnParameter();
	ret_var->name = rname;
	ret_var->var_type = rtype.size() ? move(rtype) : return_vals.back()->var_type;
	ret_var->param_type = PRT_RETVAL;

	auto t = ret_var->var_type.back();
	if (t->data_type == DT_OBJECT_REF) {
		if (do_init)
			ret_var->ptr_type = PTR_REFERENCE | PTR_OWNERSHIP;
		else	
			ret_var->ptr_type = PTR_REFERENCE;
	} else {
		ret_var->ptr_type = NO_PTR;
	}

	if (rname == "")
		ret_var->place = NULL;

	return_vals.push_back(ret_var);
	ret_dtypes.push_back(t->data_type);

	return ret_var;
}

PlnParameter* PlnFunction::addParam(const string& pname, vector<PlnType*> &ptype, PlnPassingMethod pass_method, PlnValue* defaultVal)
{
	for (auto p: parameters)
		if (p->name == pname) return NULL;

	PlnParameter* param = new PlnParameter();
	param->name = pname;
	param->var_type = ptype.size() ? move(ptype) : parameters.back()->var_type;
	param->dflt_value = defaultVal;
	param->param_type = PRT_PARAM;

	auto t = param->var_type.back();
	if (t->data_type == DT_OBJECT_REF) {
		if (pass_method == FPM_MOVEOWNER) 
			param->ptr_type = PTR_PARAM_MOVE;
		else if (pass_method == FPM_COPY) // FMP_COPY
			param->ptr_type = PTR_PARAM_COPY;
		else // FMP_REF
			param->ptr_type = PTR_REFERENCE;

	} else {
		param->ptr_type = NO_PTR;
	}

	parameters.push_back(param);
	arg_dtypes.push_back(t->data_type);

	return	param;
}

static string mangling(PlnFunction* f)
{
	static char digits[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.";

	if (f->type == FT_C || f->type == FT_SYS)
		return f->name;

	PlnBlock *b = f->parent;
	string root;
	while (b && b->parent_func) {
		root = b->parent_func->name + "." + root;
		b = b->parent_func->parent;
	}
	
	string seed = "";
	for (auto p: f->parameters) {
		seed += "|";
		seed += p->var_type.back()->name;
	}
	size_t hash = std::hash<string>{}(seed);
	string hash_str;

	int width = (sizeof(hash) * 8) / 6;
	for (int i=0; i<width; i++) {
		int c = (hash >> (i*6)) & 0x3f;
		hash_str.push_back(digits[c]);
	}

	return root + f->name + "." + hash_str;
}

void PlnFunction::genAsmName()
{
	asm_name = mangling(this);
}

void PlnFunction::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	if (type == FT_PLN || type == FT_INLINE) {
		if (implement) {
			si.push_scope(this);

			// Allocate arguments place.
			int i=0;
			auto arg_dps = da.prepareArgDps(FT_PLN, ret_dtypes, arg_dtypes, true);
			BOOST_ASSERT(arg_dps.size() == parameters.size());
			for (auto p: parameters) {
				arg_dps[i]->data_type = p->var_type.back()->data_type;
				da.allocDp(arg_dps[i]);
				++i;
			}

			// Allocate stack space for parameters.
			i = 0;
			vector<PlnVariable*> for_release;
			for (auto p: parameters) {
				if (p->ptr_type & PTR_OWNERSHIP)
					si.push_owner_var(p);

				PlnType *t = p->var_type.back();
				auto dp = da.allocData(t->size, t->data_type);
				dp->comment = &p->name;

				da.pushSrc(dp, arg_dps[i]);
				da.popSrc(dp);
				BOOST_ASSERT(!dp->save_place);

				p->place = dp;
				for_release.push_back(p);

				i++;
			}

			// Allocate return values.
			{
				vector<PlnValue> init_vals;
				for (auto ret_var: return_vals) {
					if (ret_var->name != "") {
						if (!(static_cast<PlnParameter*>(ret_var)->param_type & PRT_PARAM)) {
							PlnValue val(ret_var);
							init_vals.push_back(val);
							for_release.push_back(ret_var);
						}
					}
				}
				if (init_vals.size()) {
					retval_init = new PlnVarInit(init_vals);
					retval_init->finish(da, si);
				}
			}

			// Insert return statement to end of function if needed.
			if (implement->statements.size() == 0 || implement->statements.back()->type != ST_RETURN) {
				if (return_vals.size() > 0 && return_vals.front()->name == "") {
					PlnCompileError err(E_NeedRetValues);
					err.loc = implement->loc;
					err.loc.begin_line = err.loc.end_line;
					err.loc.begin_col= err.loc.end_col;
					throw err;
				}
				vector<PlnExpression *> rv;
				PlnReturnStmt* rs = new PlnReturnStmt(rv,implement);
				implement->statements.push_back(rs);
			}

			// Finish contents.
			implement->finish(da, si);

			// Release parameters & return values.
			for (auto v: for_release) 
				da.releaseDp(v->place);

			si.pop_owner_vars(this);
			si.pop_scope();

			da.finish(save_regs, save_reg_dps);
			inf.pln.stack_size = da.stack_size;
		}
	}
}

void PlnFunction::gen(PlnGenerator &g)
{
	switch (type) {
		case FT_PLN:
		{
			if (!implement) return;
			g.genLabel(asm_name);
			g.genEntryFunc();		
			g.genLocalVarArea(inf.pln.stack_size);		
			for (int i=0; i<save_regs.size(); ++i) {
				auto sav_e = g.getEntity(save_reg_dps[i]);
				g.genSaveReg(save_regs[i], sav_e.get());
			}
 
			for (auto p: parameters) {
				// no genSaveSrc because always save_place == NULL.
				g.genLoadDp(p->place);
			}

			if (retval_init)
				retval_init->gen(g);

			// TODO: if malloc failed.
			
			implement->gen(g);
			g.genEndFunc();
			break;
		}
	}
}

void PlnFunction::clear()
{
	if (retval_init) {
		delete retval_init;
		retval_init = NULL;
	}
	if (implement) {
		delete implement;
		implement = NULL;
	}
	parent = NULL;
}

