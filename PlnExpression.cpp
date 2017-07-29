#include <boost/assert.hpp>
#include "PlnModel.h"
#include "PlnX86_64Generator.h"

using std::endl;

// PlnExpression
PlnExpression::PlnExpression(PlnValue value)
	: type(ET_VALUE) 
{
	values.push_back(value);
}

void PlnExpression::dump(ostream& os, string indent)
{
	if (type == ET_VALUE) {
		for (auto &value: values)
		switch (value.type) {
			case VL_LIT_INT8:
				os << indent << "Int literal: " << value.inf.intValue << endl;
				break;
			case VL_VAR:
				os << indent << "Variable: " << value.inf.var->name << endl;
				break;
			case VL_RO_DATA:
				os << indent << "String literal: " << value.inf.rod->name.size() << endl;
				break;
			default:
				BOOST_ASSERT(false);
		}
	} else 
		os << indent << "Expression: " << type << endl;
}

void PlnExpression::gen(PlnGenerator& g)
{
}

// PlnFunctionCall
PlnFunctionCall::PlnFunctionCall()
	: PlnExpression(ET_FUNCCALL)
{
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
		case FT_SYS:
		{
			vector<PlnGenEntity*> gen_args;
			for(auto arg: arguments)
				gen_args.push_back(arg->values.front().genEntity(g));
				
			g.genSysCall(function->inf.syscall.id, gen_args, function->name);
			for (auto garg: gen_args)
				PlnGenEntity::freeEntity(garg);
			break;
		}
		default:
			BOOST_ASSERT(false);
	}
}

// PlnAssignment
PlnAssignment::PlnAssignment(vector<PlnVariable*>& lvals, PlnExpression* exp)
	: PlnExpression(ET_ASSIGN), lvals(move(lvals)), expression(exp)
{
}

void PlnAssignment::dump(ostream& os, string indent)
{
	os << indent << "Assign:";
	for (auto lv: lvals)
		os << " " << lv-> name;
	os << endl;
	expression->dump(os, indent+" ");	
}

void PlnAssignment::gen(PlnGenerator& g)
{
	expression->gen(g);
	for (int i=0; i<lvals.size(); ++i) {
		PlnGenEntity* le = lvals[i]->genEntity(g);
		PlnGenEntity* re = expression->values[i].genEntity(g);
		g.genMove(le, re, lvals[i]->name);
		PlnGenEntity::freeEntity(le);
		PlnGenEntity::freeEntity(re);
	}
}

