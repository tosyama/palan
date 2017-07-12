#include <boost/assert.hpp>
#include "PlnModel.h"
#include "PlnX86_64Generator.h"

// PlnExpression
void PlnExpression::gen(PlnGenerator& g)
{
}

// PlnValue
PlnGenEntity* PlnValue::gen(PlnGenerator& g)
{
	switch (type) {
		case VL_LIT_INT:
			return g.getInt(inf.intValue);
	}
	BOOST_ASSERT(false);
}

void PlnFunctionCall::addArgument(PlnExpression& arg)
{
	arguments.push_back(&arg);
}

void PlnFunctionCall::gen(PlnGenerator &g)
{
	switch (function->type) {
		case FT_SYS:
			vector<PlnGenEntity*> gen_args;
			for(auto arg: arguments)
				gen_args.push_back(arg->value.gen(g));
				
			g.genSysCall(function->inf.syscall.id, gen_args, function->name);
			for (auto garg: gen_args)
				PlnGenEntity::freeEntity(garg);
			break;
	}
}
