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
#include "PlnBlock.h"
#include "PlnStatement.h"
#include "PlnType.h"
#include "PlnVariable.h"
#include "../PlnDataAllocator.h"
#include "../PlnGenerator.h"

using std::string;
using std::endl;
using std::to_string;

PlnFunction::PlnFunction(PlnFncType func_type, const string &func_name)
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

	for (auto rv: return_vals)
		rv->alloc_type = VA_STACK;
}

PlnParameter* PlnFunction::addParam(string& pname, PlnType* ptype, PlnValue* defaultVal)
{
	for (auto rv: return_vals)
		if (rv->name == pname) return NULL;
	for (auto p: parameters)
		if (p->name == pname) return NULL;

	PlnParameter* param = new PlnParameter();
	param->name = pname;
	param->var_type = ptype;
	param->dflt_value = defaultVal;

	param->alloc_type = VA_STACK;
	parameters.push_back(param);

	return	param;
}

void PlnFunction::finish(PlnDataAllocator& da)
{
	if (type == FT_PLN || type == FT_INLINE) {
		if (implement) {
			for (auto r: return_vals)
				r->place = da.allocData(8);
			for (auto p: parameters)
				p->place = da.allocData(8);

			if (implement->statements.back()->type != ST_RETURN) {
				PlnReturnStmt* rs = new PlnReturnStmt(NULL, implement);
				implement->statements.push_back(rs);
			}

			for (auto r: return_vals)
				da.releaseData(r->place);
			for (auto p: parameters)
				da.releaseData(p->place);

			implement->finish(da);
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
				for (auto r: return_vals)
					os << indent << " RetValue: " << r->var_type->name << " " << r->name
						<< "(" << r->place->data.stack.offset << ")" << endl;
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
				auto arg = g.getArgument(i, p->var_type->size);
				auto prm = p->genEntity(g);
				g.genMove(prm.get(), arg.get(), p->name);
				++i;
			}
			implement->gen(g);
			break;
		}
	}
}

