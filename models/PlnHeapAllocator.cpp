/// Heap allocation class definition.
///
/// @file	PlnHeapAllocator.cpp
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "PlnHeapAllocator.h"
#include "PlnType.h"
#include "../PlnDataAllocator.h"
#include "../PlnGenerator.h"
#include "../PlnConstants.h"

PlnHeapAllocator::PlnHeapAllocator()
	: PlnExpression(ET_HP_ALLOC)
{
}

class PlnArrayHeapAllocator : public PlnHeapAllocator
{
public:
	PlnDataPlace* ret_dp, *size_dp;
	vector<PlnType*> &var_type;

	PlnArrayHeapAllocator(vector<PlnType*> &var_type);
	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void dump(ostream& os, string indent="") override;
	void gen(PlnGenerator& g) override;
};

PlnArrayHeapAllocator::PlnArrayHeapAllocator(vector<PlnType*> &var_type)
	: ret_dp(NULL), size_dp(NULL), var_type(var_type)
{
}

void PlnArrayHeapAllocator::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	BOOST_ASSERT(data_places.size());
	auto ainf = &var_type.back()->inf.fixedarray;

	BOOST_ASSERT(ainf->is_fixed_size);
	size_dp = da.getLiteralIntDp(ainf->alloc_size);
	da.memAlloced();
	ret_dp = da.prepareAccumulator(DT_OBJECT_REF);
	da.allocDp(ret_dp);
	da.pushSrc(data_places[0], ret_dp);
}

void PlnArrayHeapAllocator::dump(ostream& os, string indent)
{
	os << indent << "ArrayHeapAllocator" << endl;
}

void PlnArrayHeapAllocator::gen(PlnGenerator& g)
{
	auto ainf = &var_type.back()->inf.fixedarray;

	auto e = g.getEntity(ret_dp);
	g.genMemAlloc(e.get(), ainf->alloc_size, "alloc array");
	g.genSaveSrc(data_places[0]);
}

 PlnHeapAllocator* PlnHeapAllocator::createHeapAllocation(vector<PlnType*> &var_type)
{
	PlnType* vt = var_type.back();
	if (vt->name == "[]") {
		return new PlnArrayHeapAllocator(var_type);
	}

	return NULL;	
}
