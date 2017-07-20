#include <boost/assert.hpp>
#include "PlnModel.h"
#include "PlnX86_64Generator.h"

using std::endl;

// PlnExpression
void PlnExpression::dump(ostream& os, string indent)
{
	if (type == ET_VALUE) {
		switch (value.type) {
			case VL_LIT_INT8:
				os << indent << "Int literal: " << value.inf.intValue << endl;
				break;
			case VL_RO_DATA:
				os << indent << "String literal: " << value.inf.rod->name.size() << endl;
				break;
		}
	} else 
		os << indent << "Expression: " << type << endl;
}

void PlnExpression::gen(PlnGenerator& g)
{
}

// PlnFunctionCall
void PlnFunctionCall:: dump(ostream& os, string indent)
{
	os << indent << "FunctionCall: " << function->name << endl;
	os << indent << " Arguments: " << arguments.size() << endl;
	for (auto a: arguments)
		a->dump(os, indent + "  ");
}

void PlnFunctionCall::gen(PlnGenerator &g)
{
	switch (function->type) {
		case FT_SYS:
		{
			vector<PlnGenEntity*> gen_args;
			for(auto arg: arguments)
				gen_args.push_back(arg->value.genEntity(g));
				
			g.genSysCall(function->inf.syscall.id, gen_args, function->name);
			for (auto garg: gen_args)
				PlnGenEntity::freeEntity(garg);
			break;
		}
		default:
			BOOST_ASSERT(false);
	}
}
