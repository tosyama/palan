/// Function model class definition.
///
/// Function model mainly manage return values
/// and paramater definition.
/// e.g.) func funcname(int arg1, arg2)->int32 ret1, ret2  { ... }
///
/// @file	PlnFunction.cpp
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

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
		parent(NULL), has_va_arg(false), num_in_param(0), num_out_param(0),
		call_count(0), generated(false)
{
}

PlnVariable* PlnFunction::addRetValue(const string& rname, PlnVarType* rtype)
{
	for (auto& r: return_vals)
		if (r.local_var->name != "" && r.local_var->name == rname)
			throw PlnCompileError(E_DuplicateVarName, rname);

	for (auto p: parameters)
		if (p->var->name == rname) {
			if (!rtype) {
				rtype = return_vals.back().local_var->var_type;
			}
			if (p->var->var_type->name() != rtype->name())
				throw PlnCompileError(E_InvalidReturnValType, rname);

			return_vals.push_back({p->var, rtype, true});
			ret_dtypes.push_back(p->var->var_type->data_type());

			return p->var;
		}

	auto ret_var = new PlnVariable();
	ret_var->name = rname;
	ret_var->var_type = rtype ? rtype : return_vals.back().local_var->var_type;

	auto t = ret_var->var_type;

	if (t->data_type() == DT_OBJECT_REF) {
		bool do_init = (t->mode[ALLOC_MD]=='h' || t->mode[IDENTITY_MD]=='m'); // not reference
		bool readonly = (t->mode[ACCESS_MD] == 'r');

		if (do_init)
			ret_var->ptr_type = PTR_REFERENCE | PTR_OWNERSHIP;
		else	
			ret_var->ptr_type = PTR_REFERENCE;

		if (readonly)
			ret_var->ptr_type |= PTR_READONLY;

	} else {
		ret_var->ptr_type = NO_PTR;
	}

	if (rname == "")
		ret_var->place = NULL;

	return_vals.push_back({ret_var, rtype, false});
	ret_dtypes.push_back(t->data_type());

	return ret_var;
}

PlnVariable* PlnFunction::addParam(const string& pname, PlnVarType* ptype, int iomode, PlnPassingMethod pass_method, PlnExpression* defaultVal)
{
	BOOST_ASSERT(!has_va_arg);
	BOOST_ASSERT(ptype || parameters.size());

	if (pname == "...") {
		has_va_arg = true;
	}

	for (auto p: parameters)
		if (p->var->name == pname) return NULL;

	PlnVariable* param_var = new PlnVariable();
	param_var->name = pname;
	param_var->var_type = ptype ? ptype : parameters.back()->var->var_type;

	PlnParameter* param = new PlnParameter();
	param->var = param_var;
	param->dflt_value = defaultVal;
	param->index = parameters.size();
	param->iomode = iomode;
	param->force_move = (pass_method == FPM_MOVEOWNER);

	auto t = param_var->var_type;
	if (t->data_type() == DT_OBJECT_REF) {
		param_var->ptr_type = PTR_REFERENCE;
		if (t->mode[ACCESS_MD] == 'r') {
			BOOST_ASSERT(pass_method != FPM_MOVEOWNER);
			param_var->ptr_type |= PTR_READONLY;
		}

		if (t->mode[ALLOC_MD] == 'h') {
			param_var->ptr_type |= PTR_OWNERSHIP;
		}

		if (pass_method == FPM_MOVEOWNER) {
			param_var->ptr_type |= PTR_OWNERSHIP;
			BOOST_ASSERT(t->mode[IDENTITY_MD] == 'm');

		} else if (pass_method == FPM_COPY) {
			if (t->mode[ALLOC_MD] == 'h') {
				param_var->ptr_type |= PTR_CLONE;
			} else if (t->mode[ALLOC_MD] == 'r') {
			} else
				BOOST_ASSERT(false);

		} else { // FMP_REF
			BOOST_ASSERT(false);
		}

	} else {
		BOOST_ASSERT(pass_method == FPM_COPY);
		param_var->ptr_type = NO_PTR;
		if (t->mode == "rir") {
			param_var->ptr_type = PTR_REFERENCE | PTR_READONLY;
		}
	}

	parameters.push_back(param);

	if (!has_va_arg) {
		if (iomode & PIO_OUTPUT) {
			num_out_param++;
			arg_dtypes.push_back(DT_OBJECT_REF);

		} else {
			num_in_param++;
			if (t->data_type() != DT_OBJECT_REF && param_var->var_type->mode[ALLOC_MD] == 'r') {
				arg_dtypes.push_back(DT_OBJECT_REF);
			} else {
				arg_dtypes.push_back(t->data_type());
			}
		}
	}

	return	param_var;
}

vector<string> PlnFunction::getParamStrs() const
{
	vector<string> param_types;
	for (auto p: parameters) {
		string pname = p->var->var_type->name();
		if (p->force_move) 
			pname += ">>";
		if (p->var->name == "...")
			pname += "...";
		param_types.push_back(pname);
	}
	return param_types;
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
		seed += p->var->var_type->name();
		seed += "." + p->var->var_type->mode;
		seed += "." + to_string(p->force_move);
	}

	size_t hash = std::hash<string>{}(seed);
	string hash_str;

	int width = (sizeof(hash) * 8) / 6;
	for (int i=0; i<width; i++) {
		int c = (hash >> (i*6)) & 0x3f;
		hash_str.push_back(digits[c]);
	}

	BOOST_ASSERT(f->name.size() < 200);	// need define spec & check gen buffer.
	return root + f->name + "." + hash_str;
}

void PlnFunction::genAsmName()
{
	asm_name = mangling(this);
}

void PlnFunction::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	if (type == FT_PLN) {
		if (implement) {
			si.push_scope(this);

			// Allocate arguments place.
			int i=0;
			auto arg_dps = da.prepareArgDps(FT_PLN, ret_dtypes, arg_dtypes, true);
			BOOST_ASSERT(arg_dps.size() == parameters.size());
			for (auto p: parameters) {
				arg_dps[i]->data_type = p->var->var_type->data_type();
				da.allocDp(arg_dps[i]);
				++i;
			}

			// Allocate stack space for parameters.
			i = 0;
			vector<PlnVariable*> for_release;
			for (auto p: parameters) {
				if (p->var->var_type->mode[IDENTITY_MD] == 'm')
					si.push_owner_var(p->var);

				PlnVarType *t = p->var->var_type;
				auto dp = da.allocData(t->size(), t->data_type());
				dp->comment = &p->var->name;

				da.pushSrc(dp, arg_dps[i]);
				da.popSrc(dp);
				BOOST_ASSERT(!dp->save_place);

				p->var->place = dp;
				for_release.push_back(p->var);

				i++;
			}

			// Allocate return values.
			{
				vector<PlnValue> init_vals;
				for (auto& ret_val: return_vals) {
					if (!ret_val.is_share_with_param) {
						PlnValue val(ret_val.local_var);
						init_vals.push_back(val);
						for_release.push_back(ret_val.local_var);
					}
				}
				if (init_vals.size()) {
					retval_init = new PlnVarInit(init_vals);
					retval_init->finish(da, si);
				}
			}

			// Insert return statement to end of function if needed.
			if (implement->statements.size() == 0 || implement->statements.back()->type != ST_RETURN) {
				if (return_vals.size() > 0 && return_vals.front().local_var->name == "") {
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
	} else if (type == FT_C || type == FT_SYS) {
		BOOST_ASSERT(!implement);
	} else { // FT_INLINE
		BOOST_ASSERT(false);
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
				g.genLoadDp(p->var->place);
			}

			if (retval_init)
				retval_init->gen(g);

			// TODO: if malloc failed.
			
			implement->gen(g);
			g.genEndFunc();
			break;
		}

		default: // FT_C, FT_SYS, FT_INLINE
			break;
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

