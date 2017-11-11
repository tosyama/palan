/// Function model class definition.
///
/// Function model mainly manage return values
/// and paramater definition.
/// e.g.) int ret1, int ret2 funcname(int arg1, int arg2) { ... }
///
/// @file	PlnFunction.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
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

using std::string;
using std::endl;
using std::to_string;

PlnFunction::PlnFunction(int func_type, const string &func_name)
	: type(func_type), name(func_name), retval_init(NULL), implement(NULL)
{
}

void PlnFunction::setParent(PlnModule* parent_mod)
{
	parent_type = FP_MODULE;
	parent.module = parent_mod;
}

void PlnFunction::setRetValues(vector<PlnVariable*>& vars)
{
	return_vals = move(vars);
	vector<PlnType*> t;
	for (auto rv: return_vals) {
		if (rv->var_type.size()) t = rv->var_type;
		else rv->var_type = t;

		auto t = rv->var_type.back();
		if (t->data_type == DT_OBJECT_REF) {
			rv->ptr_type = PTR_REFERENCE | PTR_OWNERSHIP;
		} else {
			rv->ptr_type = NO_PTR;
		}
	}
}

PlnParameter* PlnFunction::addParam(string& pname, vector<PlnType*> *ptype, PlnPassingMethod pass_method, PlnValue* defaultVal)
{
	for (auto rv: return_vals)
		if (rv->name == pname) return NULL;
	for (auto p: parameters)
		if (p->name == pname) return NULL;

	PlnParameter* param = new PlnParameter();
	param->name = pname;
	param->var_type = ptype ? move(*ptype) : parameters.back()->var_type;
	param->dflt_value = defaultVal;

	auto t = param->var_type.back();
	if (t->data_type == DT_OBJECT_REF) {
		if (pass_method == FPM_MOVEOWNER) 
			param->ptr_type = PTR_REFERENCE | PTR_OWNERSHIP;
		else // FMP_COPY
			param->ptr_type = PTR_REFERENCE | PTR_OWNERSHIP | PTR_CLONE;
	} else {
		param->ptr_type = NO_PTR;
	}

	parameters.push_back(param);

	return	param;
}

void PlnFunction::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	if (type == FT_PLN || type == FT_INLINE) {
		if (implement) {
			si.push_scope(this);

			// Allocate stack space for return value if needed.
			vector<PlnValue> rets;
			for (auto r: return_vals) {
				if (r->name == "") r->place = NULL;
				else rets.push_back(PlnValue(r));
			}

			// Allocate stack space for parameters.
			auto dps = da.prepareArgDps(return_vals.size(), parameters.size(), FT_PLN, true);
			int i=0;
			for (auto p: parameters) {
				auto t = p->var_type.back();
				auto dp = da.allocData(t->size, t->data_type);
				dp->comment = &p->name;
				p->place = dp;

				dps[i]->data_type = t->data_type;
				p->load_place = dps[i];
				i++;

				if (p->ptr_type & PTR_OWNERSHIP)
					si.push_owner_var(p);
				// TODO: delete from rets if exists.
			}

			if (rets.size()) {
				retval_init = new PlnVarInit(rets);
				retval_init->finish(da, si);
			}

			// Insert return statement to end of function if needed.
			if (implement->statements.size() == 0 || implement->statements.back()->type != ST_RETURN) {
				vector<PlnExpression *> rv;
				PlnReturnStmt* rs = new PlnReturnStmt(rv,implement);
				implement->statements.push_back(rs);
			}

			implement->finish(da, si);

			for (auto r: return_vals) {
				if (r->place)
					da.releaseData(r->place);
			}
			for (auto p: parameters)
				da.releaseData(p->place);

			si.pop_owner_vars(this);
			si.pop_scope();

			da.finish(save_regs, save_reg_dps);
			inf.pln.stack_size = da.stack_size;
		}
	}
}

void PlnFunction::dump(ostream& os, string indent)
{
	os << indent << "Function: " << name << endl;
	os << indent << " Type: " << type << endl;
	os << indent << " Returns: " << return_vals.size() << endl;
	os << indent << " Paramaters: " << parameters.size() << endl;
	switch (type) {
		case FT_PLN: 
			if (implement) {
				os << indent << " Stack size: " << inf.pln.stack_size << endl;
				for (auto r: return_vals) {
					os << indent << " RetValue: " << r->var_type.back()->name << " " << r->name;
					if (r->place)
						os << "(" << r->place->data.stack.offset << ")" << endl;
					else
						os << "(NULL)" << endl;
				}
				for (auto p: parameters)
					os << indent << " Paramater: " << p->var_type.back()->name << " " << p->name
						<< "(" << p->place->data.stack.offset << ")" << endl;
				implement->dump(os, indent+" ");

			} else os << indent << " No Implementation" << endl;
		break;
	}
}

void PlnFunction::gen(PlnGenerator &g)
{
	switch (type) {
		case FT_PLN:
		{
			g.genEntryPoint(name);
			g.genLabel(name);
			g.genEntryFunc();		
			g.genLocalVarArea(inf.pln.stack_size);		
			for (int i=0; i<save_regs.size(); ++i) {
				auto sav_e = g.getPopEntity(save_reg_dps[i]);
				g.genSaveReg(save_regs[i], sav_e.get());
			}
 
			for (auto p: parameters) {
				auto le = g.getPopEntity(p->place);
				auto re = g.getPopEntity(p->load_place);
				g.genMove(le.get(), re.get(), string("param->") + p->name);
			}

			if (retval_init) retval_init->gen(g);
			// TODO: if malloc failed.
			
			implement->gen(g);
			break;
		}
	}
}

