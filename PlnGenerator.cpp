/// Assembly code generator class definition.
///
/// @file	PlnGenerator.cpp
/// @copyright	2017 YAMAGUCHI Toshinobu

#include <vector>
#include <iostream>
#include <string>
#include <string.h>
#include <sstream>
#include "PlnModel.h"
#include "PlnDataAllocator.h"
#include "PlnGenerator.h"

unique_ptr<PlnGenEntity> PlnGenerator::getPushEntity(PlnDataPlace* dp)
{
	if (dp->save_place) {
		return getEntity(dp->save_place);
	} else
		return getEntity(dp);
}

unique_ptr<PlnGenEntity> PlnGenerator::getPopEntity(PlnDataPlace* dp)
{
	auto e = getEntity(dp);
	if (dp->save_place) {
		auto se = getEntity(dp->save_place);
		string cmt = string("load ") + *dp->comment + " from " + *dp->save_place->comment;
		genMove(e.get(), se.get(), cmt);
	}
	return getEntity(dp);
}

