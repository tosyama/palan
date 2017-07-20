#include "PlnModel.h"
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
	switch (type) {
		case FT_PLN:
			g.genLabel(name);
			implement->gen(g);
			break;
	}
}
