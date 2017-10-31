/// PlnClone expression class definition.
///
/// @file	PlnClone.cpp
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "PlnClone.h"
#include "../PlnType.h"
#include "../../PlnConstants.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"

PlnClone::PlnClone(PlnExpression* src)
	: PlnExpression(ET_CLONE), clone_src(src)
{
	BOOST_ASSERT(src->values.size() == 1);
	for (auto sval: src->values) {
		PlnValue val;
		val.type = VL_WORK;
		// TODO: lval_type?
		val.inf.wk_type = sval.getType();
		values.push_back(val);
	}

	auto t = values[0].getType();
	if (t->name == "[]") {
		int item_size = t->inf.fixedarray.item_size;
		int asize = 0;
		for (int i: *t->inf.fixedarray.sizes)
			asize += i;
		asize *= item_size;
		copy_size = asize;
	} else
		BOOST_ASSERT(false);
}

void PlnClone::finish(PlnDataAllocator& da)
{
	clone_dp = da.allocData(8, DT_OBJECT_REF);
	da.memAlloced();
	da.prepareMemCopyDps(cpy_dst_dp, cpy_src_dp);
	clone_src->data_places.push_back(cpy_src_dp);
	clone_src->finish(da);

	da.allocDp(cpy_src_dp);
	da.allocDp(cpy_dst_dp);
	da.memCopyed(cpy_dst_dp, cpy_src_dp);
	da.releaseData(clone_dp);
}

void PlnClone::dump(ostream& os, string indent)
{
	os << indent << "Clone:" << endl;
	clone_src->dump(os, indent+" ");
}

void PlnClone::gen(PlnGenerator& g)
{
	auto clone_e = g.getEntity(clone_dp);
	static string cmt = "clone";
	g.genMemAlloc(clone_e.get(), copy_size, cmt);
	clone_src->gen(g);
	g.getPopEntity(cpy_src_dp);
	auto cp_dst_e = g.getPopEntity(cpy_dst_dp);
	g.genMove(cp_dst_e.get(), clone_e.get(), cmt + " -> " + *cpy_dst_dp->comment);
	g.genMemCopy(copy_size, cmt);
	auto e = g.getPushEntity(data_places[0]);
	g.genMove(e.get(), clone_e.get(), cmt + " -> " + *data_places[0]->comment);
}

