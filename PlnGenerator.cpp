/// Assembly code generator class definition.
///
/// @file	PlnGenerator.cpp
/// @copyright	2017-2019 YAMAGUCHI Toshinobu

#include <vector>
#include <iostream>
#include <string>
#include <string.h>
#include <sstream>
#include <boost/assert.hpp>
#include "PlnModel.h"
#include "PlnDataAllocator.h"
#include "PlnGenerator.h"

static void preLoadDp(PlnGenerator &g, PlnDataPlace *dp)
{
	if (dp->type == DP_INDRCT_OBJ) {
		auto base_dp = dp->data.indirect.base_dp;
		auto index_dp = dp->data.indirect.index_dp;

		if (base_dp) g.genLoadDp(base_dp, false);
		if (index_dp && index_dp->type != DP_LIT_INT) g.genLoadDp(index_dp, false);
		if (base_dp) g.genSaveDp(base_dp);
		if (index_dp) g.genSaveDp(index_dp);
	}
}

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
		preLoadDp(*this, src_dp);

		src_cmt = src_dp->cmt();
	}

	preLoadDp(*this, dp);

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
	auto savdp = dp->save_place;
	if (savdp) {
		auto src_dp = dp->src_place;

		preLoadDp(*this, src_dp);

		auto srcdp = dp->src_place;
		BOOST_ASSERT(srcdp);
		auto save = getEntity(savdp);
		auto srce = getEntity(srcdp);
		string opt_cmt = (savdp == dp) ? " (accelerate)" : " for save";
		// genMove(save.get(), srce.get(), srcdp->cmt() + " -> " + savdp->cmt() + opt_cmt);

		if (dp->load_address) {
			genLoadAddress(save.get(), srce.get(), "address of " + srcdp->cmt() + " -> " + savdp->cmt() + opt_cmt);
		} else {
			genMove(save.get(), srce.get(), srcdp->cmt() + " -> " + savdp->cmt() + opt_cmt);
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

		preLoadDp(*this, dp);

		auto srce = getEntity(src_dp);
		auto dste = getEntity(dp);
		genMove(dste.get(), srce.get(), src_cmt + " -> " + dp->cmt());

		// BOOST_ASSERT(!dp->do_clear_src);
		// Basically, save_place used for register usage confliction.
		// do_clear_src used for variable on stack memory.
		// Following code not requrired because two condition don't occur same time.
		if (dp->do_clear_src) {
			vector<unique_ptr<PlnGenEntity>> clr_es;
			clr_es.push_back(getEntity(src_dp));
			genNullClear(clr_es);
		}
	}
}
