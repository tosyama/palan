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
	: type(VL_LIT_INT8), asgn_type(NO_ASGN)
{
	inf.intValue = intValue;
}

PlnValue::PlnValue(uint64_t uintValue)
	: type(VL_LIT_UINT8), asgn_type(NO_ASGN)
{
	inf.uintValue = uintValue;
}

PlnValue::PlnValue(PlnReadOnlyData* rod)
	: type(VL_RO_DATA), asgn_type(NO_ASGN)
{
	inf.rod = rod;
}

PlnValue::PlnValue(PlnVariable* var)
	: type(VL_VAR), asgn_type(NO_ASGN)
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
			return inf.var->var_type.back();
		case VL_WORK:
			return inf.wk_type->back();
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
			PlnVariable *var = inf.var;
			if (var->ptr_type & PTR_INDIRECT_ACCESS) {
				if (!var->place) {
					var->place = new PlnDataPlace(var->var_type.back()->size, var->var_type.back()->data_type);
					var->place->comment = &var->name;
				}
				return var->place;
			} else
				return da.getSeparatedDp(inf.var->place);
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

bool PlnExpression::isLitNum(int& num_type)
{
	if (type != ET_VALUE) return false;

	auto t = values[0].type;
	if (t == VL_LIT_INT8 || t== VL_LIT_UINT8) {
		num_type = t;
		return true;
	}
	return false;
}

void PlnExpression::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	if (!val_place)
		val_place = values[0].getDataPlace(da);
	if (data_places.size())
		da.pushSrc(data_places[0], val_place);
}

void PlnExpression::gen(PlnGenerator& g)
{
	BOOST_ASSERT(data_places.size() <= 1);
	if (data_places.size())
		g.genSaveSrc(data_places[0]);
}

