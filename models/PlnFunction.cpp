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

using std::string;
using std::endl;
using std::to_string;

PlnFunction::PlnFunction(int func_type, const string &func_name)
	: type(func_type), name(func_name), implement(NULL)
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
	PlnType* t;
	for (auto rv: return_vals) {
		if (rv->var_type) t = rv->var_type;
		else rv->var_type = t;
	}
}

PlnParameter* PlnFunction::addParam(string& pname, PlnType* ptype, PlnValue* defaultVal)
{
	for (auto rv: return_vals)
		if (rv->name == pname) return NULL;
	for (auto p: parameters)
		if (p->name == pname) return NULL;

	PlnParameter* param = new PlnParameter();
	param->name = pname;
	param->var_type = ptype ? ptype : parameters.back()->var_type;
	param->dflt_value = defaultVal;

	parameters.push_back(param);

	return	param;
}

void PlnFunction::finish(PlnDataAllocator& da)
{
	if (type == FT_PLN || type == FT_INLINE) {
		if (implement) {
			for (auto r: return_vals) {
				if (r->name == "") r->place = NULL;
				else {
					auto t = r->var_type;
					r->place = da.allocData(t->size, t->data_type);
					r->place->comment = &r->name;
				}
			}

			auto dps = da.prepareArgDps(return_vals.size(), parameters.size(), FT_PLN, true);
			int i=0;
			for (auto p: parameters) {
				auto t = p->var_type;
				auto dp = da.allocData(t->size, t->data_type);
				dp->comment = &p->name;
				p->place = dp;
				p->load_place = dps[i];
				i++;
			}

			if (implement->statements.back()->type != ST_RETURN) {
				vector<PlnExpression *> rv;
				PlnReturnStmt* rs = new PlnReturnStmt(rv,implement);
				implement->statements.push_back(rs);
			}

			implement->finish(da);

			for (auto r: return_vals) {
				if (r->place)
					da.releaseData(r->place);
			}
			for (auto p: parameters)
				da.releaseData(p->place);

			da.finish();
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
					os << indent << " RetValue: " << r->var_type->name << " " << r->name;
					if (r->place)
						os << "(" << r->place->data.stack.offset << ")" << endl;
					else
						os << "(NULL)" << endl;
				}
				for (auto p: parameters)
					os << indent << " Paramater: " << p->var_type->name << " " << p->name
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
			
			int i=return_vals.size();
			if (i==0) i = 1;
			
			for (auto p: parameters) {
				auto le = g.getPopEntity(p->place);
				auto re = g.getPopEntity(p->load_place);
				g.genMove(le.get(), re.get(), string("param->") + p->name);
			}
			
			implement->gen(g);
			break;
		}
	}
}

