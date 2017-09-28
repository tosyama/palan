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
#include "../PlnDataAllocator.h"
#include "../PlnGenerator.h"

using std::to_string;
using boost::adaptors::reverse;

// PlnValue
PlnValue::PlnValue(int64_t intValue)
	: type(VL_LIT_INT8)
{
	inf.intValue = intValue;
}

PlnValue::PlnValue(uint64_t uintValue)
	: type(VL_LIT_UINT8)
{
	inf.uintValue = uintValue;
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

PlnDataPlace* PlnValue::getDataPlace(PlnDataAllocator& da)
{
	switch(type) {
		case VL_LIT_INT8:
		case VL_LIT_UINT8:
			return da.getLiteralIntDp(inf.intValue);
		case VL_RO_DATA:
			return da.getReadOnlyDp(inf.rod->index);
		case VL_VAR:
			return inf.var->place;
	}
	BOOST_ASSERT(false);
}

unique_ptr<PlnGenEntity> PlnValue::genEntity(PlnGenerator& g)
{
	switch (type) {
		case VL_LIT_INT8:
		case VL_LIT_UINT8:
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

unique_ptr<PlnGenEntity> PlnReadOnlyData::genEntity(PlnGenerator &g)
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

void PlnExpression::finish(PlnDataAllocator& da)
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

static string exp_cmt(PlnValue& v, PlnDataPlace* dp)
{
	switch (v.type) {
		case VL_LIT_INT8:
		case VL_LIT_UINT8:
			return (string("$ -> ") + *dp->comment);
		case VL_VAR:
			return (v.inf.var->name+" -> " + *dp->comment);
		case VL_RO_DATA:
			return ("\"..\" -> " + *dp->comment);
		default:
			BOOST_ASSERT(false);
	}
}

void PlnExpression::gen(PlnGenerator& g)
{
	for (int i=0; i<data_places.size(); ++i) {
		auto re = values[i].genEntity(g);
	 	auto le = g.getPushEntity(data_places[i]);
		
		g.genMove(le.get(), re.get(), exp_cmt(values[i],data_places[i]));
	}
}

