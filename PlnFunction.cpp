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

void PlnFunction::addParam(PlnParameter& param)
{
	parameters.push_back(&param);
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

int PlnFunction::finish()
{
	if (type == FT_PLN || type == FT_INLINE) {
		if (implement) {
			if (return_vals.size()==0) {	// if void, add return to tail.
				PlnStatement* s = new PlnStatement();
				s->type = ST_RETURN;
				s->parent = implement;
				s->inf.return_vals = new vector<PlnExpression*>;
				implement->statements.push_back(s);
			}

			inf.pln.stack_size += implement->stackSize();
		}
	}
	return 0;
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
			g.genEntryPoint(name);
			g.genLabel(name);
			g.genEntryFunc();		
			g.genLocalVarArea(inf.pln.stack_size);		
			implement->gen(g);
			break;
	}
}
