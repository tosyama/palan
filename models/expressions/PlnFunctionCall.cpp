/// Function call model class definition.
///
/// PlnFunctionCall call function with arguments
/// and get return values.
/// e.g. ) a = func(c, d);
///
/// @file	PlnFunctionCall.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include "../../PlnModel.h"
#include "../PlnFunction.h"
#include "PlnFunctionCall.h"
#include "../PlnVariable.h"
#include "../../PlnGenerator.h"

using std::endl;
using boost::adaptors::reverse;

// PlnFunctionCall
PlnFunctionCall:: PlnFunctionCall(PlnFunction* f, vector<PlnExpression*>& args)
	: PlnExpression(ET_FUNCCALL),
	function(f),
	arguments(move(args))
{
	int i=0;
	for (auto rv: f->return_vals) {
		PlnVariable* ret_var = new PlnVariable();
		ret_var->name = rv->name;
		ret_var->alloc_type = VA_RETVAL;
		ret_var->inf.index = i;

		values.push_back(PlnValue(ret_var));

		++i;
	}
}

void PlnFunctionCall::finish()
{
	PlnReturnPlace rp;
	switch (function->type) {
		case FT_PLN: rp.type = RP_ARGPLN; break;
		case FT_SYS: rp.type = RP_ARGSYS; break;
		case FT_C: rp.type = RP_ARGPLN; break;
		default:
			BOOST_ASSERT(false);
	}

	int i = function->return_vals.size();
	if (i==0) i=1;
	for (auto a: arguments) {
		rp.inf.index = i;
		a->ret_places.push_back(rp);
		a->finish();
		++i;
	}
}

void PlnFunctionCall:: dump(ostream& os, string indent)
{
	os << indent << "FunctionCall: " << function->name << endl;
	os << indent << " Arguments: " << arguments.size() << endl;
	for (auto a: arguments) {
		if (a) a->dump(os, indent + "  ");
		else os << indent + "  NULL" << endl;
	}
}

void PlnFunctionCall::gen(PlnGenerator &g)
{
	switch (function->type) {
		case FT_PLN:
		{
			for (auto arg: reverse(arguments)) 
				arg->gen(g);
			g.genCCall(function->name);
			int i = 0;
			for (auto rp: ret_places) {
				auto dst = rp.genEntity(g);
				auto src = values[i].genEntity(g);

				g.genMove(dst.get(), src.get(), rp.commentStr());

				++i;
			}
			break;
		}
		case FT_SYS:
		{
			for (auto arg: reverse(arguments)) 
				arg->gen(g);
			g.genSysCall(function->inf.syscall.id, function->name);
			break;
		}
		case FT_C:
		{
			for (auto arg: reverse(arguments)) 
				arg->gen(g);
			g.genCCall(function->name);
			break;
		}
		default:
			BOOST_ASSERT(false);
	}
}
