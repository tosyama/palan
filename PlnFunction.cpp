#include <boost/assert.hpp>
#include "PlnModel.h"
#include "PlnScopeStack.h"
#include "PlnX86_64Generator.h"

using std::string;
using std::endl;

PlnFunction::PlnFunction(const string &func_name)
	: name(func_name), implement(NULL)
{
}

void PlnFunction::addParam(PlnVariable& param)
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
			if (implement) implement->dump(os, indent+" ");
			else os << indent << " No Implementation" << endl;
		break;
	}
}

void PlnFunction::gen(PlnGenerator &g)
{
	string ep(name);
	if (name == "main") ep = "_start";
	switch (type) {
		case FT_PLN:
			g.genEntryPoint(ep);
			g.genLabel(ep);
			implement->gen(g);
			break;
	}
}
