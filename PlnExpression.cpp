#include <boost/assert.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include "PlnModel.h"
#include "PlnX86_64Generator.h"

using std::endl;
using std::to_string;
using boost::adaptors::reverse;

// PlnReturnPlace
string PlnReturnPlace::commentStr()
{
	switch (type) {
		case RP_ARGPLN:
		case RP_ARGSYS:
			return "arg" + to_string(inf.index);
		case RP_VAR:
			return inf.var->name;
	}
	return "";
}

PlnGenEntity* PlnReturnPlace::genEntity(PlnGenerator& g)
{
	switch (type) {
		case RP_ARGPLN:
			return g.getArgument(inf.index);
			break;
		case RP_ARGSYS:
			return g.getSysArgument(inf.index);
			break;
		case RP_VAR:
			return inf.var->genEntity(g);
		default:
			BOOST_ASSERT(false);
	}
	return NULL;
}

// PlnExpression
PlnExpression::PlnExpression(PlnValue value)
	: type(ET_VALUE) 
{
	values.push_back(value);
}

void PlnExpression::finish()
{
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
	BOOST_ASSERT(type == ET_VALUE);

	PlnGenEntity* re = values[0].genEntity(g);
	PlnGenEntity* le = ret_places[0].genEntity(g);
	
	g.genMove(le, re, ret_places[0].commentStr());
	PlnGenEntity::freeEntity(re);
	PlnGenEntity::freeEntity(le);
}

PlnMultiExpression::PlnMultiExpression(
	PlnExpression* first, PlnExpression* second)
	: PlnExpression(ET_MULTI)
{
	values = first->values;
	exps.push_back(first);
	append(second);
}

void PlnMultiExpression::append(PlnExpression* exp)
{
	values.insert(values.end(), exp->values.begin(), exp->values.end());
	exps.push_back(exp);
}

void PlnMultiExpression::finish()
{
	int i=0;
	for (auto exp: exps) {
		for (auto v: exp->values) {
			exp->ret_places.push_back(ret_places[i]);
			i++;
		}
	}
}

void PlnMultiExpression::dump(ostream& os, string indent)
{
	os << indent << "MultiExpression: " << exps.size() << endl;
	for (auto e: exps)
		e->dump(os, indent+" ");
}

void PlnMultiExpression::gen(PlnGenerator& g)
{
	for (auto e: exps) {
		e->gen(g);
	}

	int i=0;
	for (auto exp: exps)
		for (auto rp: exp->ret_places) {
			PlnGenEntity* re = rp.genEntity(g);
			PlnGenEntity* le = ret_places[i].genEntity(g);
			g.genMove(le, re, ret_places[i].commentStr());
			PlnGenEntity::freeEntity(re);
			PlnGenEntity::freeEntity(le);
			i++;
		}
}

// PlnFunctionCall
PlnFunctionCall::PlnFunctionCall()
	: PlnExpression(ET_FUNCCALL)
{
}

void PlnFunctionCall::finish()
{
	if (function->type == FT_SYS) {
		PlnReturnPlace rp;
		rp.type = RP_ARGSYS;
		int i = 1;
		for (auto a: arguments) {
			rp.inf.index = i;
			a->ret_places.push_back(rp);
			a->finish();
			i++;
		}
	} else {
		BOOST_ASSERT(false); //not implemmented.
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
		case FT_SYS:
		{
			for (auto arg: reverse(arguments)) 
				arg->gen(g);
			g.genSysCall(function->inf.syscall.id, function->name);
			break;
		}
		default:
			BOOST_ASSERT(false);
	}
}

// PlnAssignment
PlnAssignment::PlnAssignment(vector<PlnValue>& lvals, PlnExpression* exp)
	: PlnExpression(ET_ASSIGN), expression(exp)
{
	values = move(lvals);
}

void PlnAssignment::finish()
{
	PlnReturnPlace rp;
	rp.type = RP_VAR;
	for (auto lv: values) {
		rp.inf.var = lv.inf.var;
		expression->ret_places.push_back(rp);
	}
	expression->finish();
}

void PlnAssignment::dump(ostream& os, string indent)
{
	os << indent << "Assign:";
	for (auto lv: values)
		os << " " << lv.inf.var->name;
	os << endl;
	expression->dump(os, indent+" ");	
}

void PlnAssignment::gen(PlnGenerator& g)
{
	expression->gen(g);
}

