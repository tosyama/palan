/// Expression model class definition.
///
/// Expression model returns some values.
/// The values are set specified place.
///
/// @file	PlnExpression.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include "PlnExpression.h"
#include "PlnVariable.h"
#include "../PlnGenerator.h"

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

void PlnReturnPlace::dump(ostream& os, string indent)
{
	os << indent << "ReturnPlace:";
	switch (type) {
		case RP_NULL: os << "Null"; break;
		case RP_VAR: os << "Variable"; break;
		case RP_AS_IS: os << "As is"; break;
		case RP_ARGPLN: os << "Palan Argument"; break;
		case RP_ARGSYS: os << "Syscall Argument"; break;
		case RP_WORK: os << "Work Area"; break;
		default:
			os << "Unknown" << to_string(type);
	}
	os << endl;
}

PlnGenEntity* PlnReturnPlace::genEntity(PlnGenerator& g)
{
	switch (type) {
		case RP_NULL: return g.getNull();
		case RP_VAR: return inf.var->genEntity(g);
		case RP_AS_IS: return inf.as_is->genEntity(g);
		case RP_ARGPLN: return g.getArgument(inf.index);
		case RP_ARGSYS: return g.getSysArgument(inf.index);
		case RP_WORK: return g.getWork(inf.index);
		default:
			BOOST_ASSERT(false);
	}
	return NULL;
}

// PlnValue
PlnValue::PlnValue(int intValue)
	: type(VL_LIT_INT8)
{
	inf.intValue = intValue;
}

PlnValue::PlnValue(PlnReadOnlyData* rod)
	: type(VL_RO_DATA)
{
	inf.rod = rod;
}

PlnValue::PlnValue(PlnVariable* var)
	: type(VL_VAR)
{
	inf.var = var;
}

PlnGenEntity* PlnValue::genEntity(PlnGenerator& g)
{
	switch (type) {
		case VL_LIT_INT8:
			return g.getInt(inf.intValue);
		case VL_RO_DATA:
			return inf.rod->genEntity(g);
		case VL_VAR:
			return inf.var->genEntity(g);
	}
	BOOST_ASSERT(false);
}

void PlnReadOnlyData::gen(PlnGenerator &g)
{
	switch (type) {
		case RO_LIT_STR:
			g.genStringData(index, name); 
			break;
		default:
			BOOST_ASSERT(false);
	}
}

PlnGenEntity* PlnReadOnlyData::genEntity(PlnGenerator &g)
{
	switch (type) {
		case RO_LIT_STR:
			return g.getStrAddress(index); 
		default:
			BOOST_ASSERT(false);
	}
}

// PlnExpression
PlnExpression::PlnExpression(PlnValue value)
	: type(ET_VALUE) 
{
	values.push_back(value);
}

void PlnExpression::finish()
{
	if (ret_places[0].type == RP_AS_IS) {
		ret_places[0].inf.as_is = &values[0];
	}
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
	for (int i=0; i<ret_places.size(); ++i) {
		PlnGenEntity* re = values[i].genEntity(g);
		PlnGenEntity* le = ret_places[i].genEntity(g);
		
		g.genMove(le, re, ret_places[i].commentStr());
		PlnGenEntity::freeEntity(re);
		PlnGenEntity::freeEntity(le);
	}
}

