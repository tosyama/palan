#include <boost/assert.hpp>
#include "PlnModel.h"
#include "PlnScopeStack.h"
#include "PlnX86_64Generator.h"

using std::string;
using std::endl;

PlnFunction::PlnFunction(PlnFncType func_type, const string &func_name)
	: type(func_type), name(func_name), implement(NULL)
{
	if (func_type == FT_PLN) {
		inf.pln.stack_size = 0;
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
	param->var_type = ptype;
	param->alloc_type = VA_UNKNOWN;
	param->dflt_value = defaultVal;
	
	parameters.push_back(param);

	return	param;
}

void PlnFunction::setParent(PlnScopeItem& scope)
{
	switch (scope.type) {
		case SC_MODULE:
			parent_type = FP_MODULE;
			parent.module = scope.inf.module;
			break;
		default:
			BOOST_ASSERT(false);
	}
}

void PlnFunction::finish()
{
	if (type == FT_PLN || type == FT_INLINE) {
		for (auto rv: return_vals)
			if (rv->alloc_type == VA_UNKNOWN) {
				inf.pln.stack_size += rv->var_type->size;
				rv->alloc_type = VA_STACK;
				rv->inf.stack.pos_from_base = inf.pln.stack_size;
			}
			
		for (auto p: parameters) 
			if (p->alloc_type == VA_UNKNOWN) {
				inf.pln.stack_size += p->var_type->size;
				p->alloc_type = VA_STACK;
				p->inf.stack.pos_from_base = inf.pln.stack_size;
			}

		if (implement) {
			if (implement->statements.back()->type != ST_RETURN) {
				PlnReturnStmt* rs = new PlnReturnStmt(NULL, implement);
				implement->statements.push_back(rs);
			}

			implement->finish();

			inf.pln.stack_size += implement->totalStackSize();
		}
	}
}

void PlnFunction::dump(ostream& os, string indent)
{
	os << indent << "Function: " << name << endl;
	os << indent << " Type: " << type << endl;
	os << indent << " Paramaters: " << parameters.size() << endl;
	os << indent << " Returns: " << return_vals.size() << endl;
	switch (type) {
		case FT_PLN: 
			if (implement) {
				os << indent << " Stack size: " << inf.pln.stack_size << endl;
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
				PlnGenEntity* arg = g.getArgument(i);
				PlnGenEntity* prm = p->genEntity(g);
				g.genMove(prm, arg, p->name);
				PlnGenEntity::freeEntity(arg);
				PlnGenEntity::freeEntity(prm);
				++i;
			}
			implement->gen(g);
			break;
		}
	}
}

