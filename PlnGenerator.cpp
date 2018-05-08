/// Assembly code generator class definition.
///
/// @file	PlnGenerator.cpp
/// @copyright	2017 YAMAGUCHI Toshinobu

#include <vector>
#include <iostream>
#include <string>
#include <string.h>
#include <sstream>
#include <boost/assert.hpp>
#include "PlnModel.h"
#include "PlnDataAllocator.h"
#include "PlnGenerator.h"

void PlnGenerator::genLoadDp(PlnDataPlace* dp)
{
	PlnDataPlace *src_dp;
	string src_cmt;
	if (dp->save_place) {
		src_dp = dp->save_place;
		src_cmt = dp->src_place->cmt() + src_dp->cmt();
	} else {
		src_dp = dp->src_place;

		if (src_dp->type == DP_INDRCT_OBJ) {
			if (auto base_dp = src_dp->data.indirect.base_dp) {
				genLoadDp(base_dp);
			}
			if (auto index_dp = src_dp->data.indirect.index_dp) {
				genLoadDp(index_dp);
			}
		}

		src_cmt = src_dp->cmt();
	}

	if (dp->type == DP_INDRCT_OBJ) {
		if (auto base_dp = dp->data.indirect.base_dp) {
			genLoadDp(base_dp);
		}
		if (auto index_dp = dp->data.indirect.index_dp) {
			genLoadDp(index_dp);
		}
	}

	auto srce = getEntity(src_dp);
	auto dste = getEntity(dp);
	genMove(dste.get(), srce.get(), src_cmt + " -> " + dp->cmt());
}

void PlnGenerator::genSaveSrc(PlnDataPlace* dp)
{
	auto savdp = dp->save_place;
	if (savdp) {
		auto src_dp = dp->src_place;

		if (src_dp->type == DP_INDRCT_OBJ) {
			if (auto base_dp = src_dp->data.indirect.base_dp) {
				genLoadDp(base_dp);
			}
			if (auto index_dp = src_dp->data.indirect.index_dp) {
				genLoadDp(index_dp);
			}
		}
		auto srcdp = dp->src_place;
		BOOST_ASSERT(srcdp);
		auto save = getEntity(savdp);
		auto srce = getEntity(srcdp);
		genMove(save.get(), srce.get(), srcdp->cmt() + " -> " + savdp->cmt() + " for save");
	}
}
