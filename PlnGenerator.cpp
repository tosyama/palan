/// Assembly code generator class definition.
///
/// @file	PlnGenerator.cpp
/// @copyright	2017-2020 YAMAGUCHI Toshinobu

#include <vector>
#include <iostream>
#include <string>
#include <string.h>
#include <sstream>
#include <boost/assert.hpp>
#include "PlnModel.h"
#include "PlnDataAllocator.h"
#include "PlnGenerator.h"

void PlnGenerator::genLoadDp(PlnDataPlace* dp, bool load_save)
{
	PlnDataPlace *src_dp;
	string src_cmt;
	if (dp->save_place) {
		if (!load_save) return;

		src_dp = dp->save_place;
		src_cmt = dp->src_place->cmt() + src_dp->cmt();

	} else {
		src_dp = dp->src_place;
		src_cmt = src_dp->cmt();
	}

	auto srce = getEntity(src_dp);
	auto dste = getEntity(dp);
	if (dp->load_address && !dp->save_place) {
		genLoadAddress(dste.get(), srce.get(), "address of " + src_cmt + " -> " + dp->cmt());

	} else {
		genMove(dste.get(), srce.get(), src_cmt + " -> " + dp->cmt());
		if (dp->do_clear_src && !dp->save_place) {
			vector<unique_ptr<PlnGenEntity>> clr_es;
			clr_es.push_back(getEntity(src_dp));
			genNullClear(clr_es);
		}
	}
}

void PlnGenerator::genSaveSrc(PlnDataPlace* dp)
{
	auto save_dp = dp->save_place;
	if (save_dp) {
		auto src_dp = dp->src_place;
		BOOST_ASSERT(src_dp);

		auto save = getEntity(save_dp);
		auto srce = getEntity(src_dp);
		string opt_cmt = (save_dp == dp) ? " (accelerate)" : " for save";

		if (dp->load_address) {
			genLoadAddress(save.get(), srce.get(), "address of " + src_dp->cmt() + " -> " + save_dp->cmt() + opt_cmt);
		} else {
			genMove(save.get(), srce.get(), src_dp->cmt() + " -> " + save_dp->cmt() + opt_cmt);
			if (dp->do_clear_src) {
				vector<unique_ptr<PlnGenEntity>> clr_es;
				clr_es.push_back(getEntity(src_dp));
				genNullClear(clr_es);
			}
		}
	}
}

void PlnGenerator::genSaveDp(PlnDataPlace* dp) {
	if (dp->save_place && dp != dp->save_place) {
		PlnDataPlace *src_dp;
		string src_cmt;

		src_dp = dp->save_place;
		src_cmt = dp->src_place->cmt() + src_dp->cmt();

		auto srce = getEntity(src_dp);
		auto dste = getEntity(dp);
		genMove(dste.get(), srce.get(), src_cmt + " -> " + dp->cmt());

		// Basically, save_place used for register usage confliction.
		// do_clear_src used for variable on stack memory.
		// and this case will happen at "f(arr>>, arr2[i]);".
		// Following code not requrired because two condition don't occur same time.

		//if (dp->do_clear_src) {
		//	vector<unique_ptr<PlnGenEntity>> clr_es;
		//	clr_es.push_back(getEntity(src_dp));
		//	genNullClear(clr_es);
		//}
	}
}
