/// Expression model class definition.
///
/// Expression model returns some values.
/// The values are set specified place.
///
/// @file	PlnExpression.cpp
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

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


PlnValue::PlnValue(const PlnValue &src)
{
	*this = src;
	if (src.type == VL_LIT_STR) {
		inf.strValue = new string(*src.inf.strValue);
	}
}

PlnValue::PlnValue(uint64_t uintValue)
	: type(VL_LIT_UINT8), asgn_type(NO_ASGN)
{
	inf.uintValue = uintValue;
}

PlnValue::PlnValue(double floValue)
	: type(VL_LIT_FLO8), asgn_type(NO_ASGN)
{
	inf.floValue = floValue;
}

PlnValue::PlnValue(string strValue)
	: type(VL_LIT_STR), asgn_type(NO_ASGN)
{
	inf.strValue = new string(strValue);
}

PlnValue::PlnValue(PlnVariable* var)
	: type(VL_VAR), asgn_type(NO_ASGN)
{
	inf.var = var;
}
 
PlnValue::~PlnValue()
{
	if (type == VL_LIT_STR)
		delete inf.strValue;
}

PlnType* PlnValue::getType()
{
	switch(type) {
		case VL_LIT_INT8:
			return PlnType::getSint();
		case VL_LIT_UINT8:
			return PlnType::getUint();
		case VL_LIT_FLO8:
			return PlnType::getFlo();
		case VL_LIT_STR:
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

		case VL_LIT_FLO8:
			return da.getLiteralFloDp(inf.floValue);

		case VL_LIT_STR:
			return da.getROStrArrayDp(*inf.strValue);


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

// PlnExpression
PlnExpression::PlnExpression(PlnValue value) : type(ET_VALUE)
{
	values.push_back(value);
}

PlnExpression::~PlnExpression()
{
}

int PlnExpression::getDataType(int val_ind)
{
	BOOST_ASSERT(values.size() > val_ind && val_ind >= 0);
	return values[val_ind].getType()->data_type;
}

void PlnExpression::adjustType(const vector<PlnType*> &var_type)
{
}

void PlnExpression::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	if (data_places.size())
		da.pushSrc(data_places[0], values[0].getDataPlace(da));
}

void PlnExpression::gen(PlnGenerator& g)
{
	BOOST_ASSERT(data_places.size() <= 1);
	if (data_places.size())
		g.genSaveSrc(data_places[0]);
}

