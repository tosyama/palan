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
#include "PlnType.h"
#include "PlnVariable.h"
#include "../PlnConstants.h"
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

PlnType* PlnValue::getType()
{
	switch(type) {
		case VL_LIT_INT8:
			return PlnType::getSint();
		case VL_LIT_UINT8:
			return PlnType::getUint();
		case VL_RO_DATA:
			return PlnType::getReadOnlyCStr();
		case VL_VAR:
			return inf.var->var_type;
		case VL_WORK:
			return inf.wk_type;
	}
	BOOST_ASSERT(false);
}

PlnDataPlace* PlnValue::getDataPlace(PlnDataAllocator& da)
{
	switch(type) {
		case VL_LIT_INT8:
				return da.getLiteralIntDp(inf.intValue);
		case VL_LIT_UINT8:
			{
				auto dp = da.getLiteralIntDp(inf.intValue);
				dp->data_type = DT_UINT;
				return dp;
			}
		case VL_RO_DATA:
			return da.getReadOnlyDp(inf.rod->index);
		case VL_VAR:
			return inf.var->place;
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

// PlnExpression
PlnExpression::PlnExpression(PlnValue value)
	: type(ET_VALUE), val_place(NULL)
{
	values.push_back(value);
}

int PlnExpression::getDataType(int val_ind)
{
	BOOST_ASSERT(values.size() > val_ind && val_ind >= 0);
	return values[val_ind].getType()->data_type;
}

void PlnExpression::finish(PlnDataAllocator& da)
{
	val_place = values[0].getDataPlace(da);
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
	BOOST_ASSERT(data_places.size() <= 1);
	if (data_places.size()) {
		auto re = g.getEntity(val_place);
	 	auto le = g.getPushEntity(data_places[0]);
		g.genMove(le.get(), re.get(), exp_cmt(values[0],data_places[0]));
	}
}

